#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef __KERNEL__
#define __KERNEL__
#endif

FILE *output = fopen("output.txt", "w");


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A kernel-based keylogger module");

int __init init_keylogger(void)
{
	printk(KERN_INFO "Hello! Keylogger module successfully loaded.\n");
	return 0;
}

void __exit exit_module(void)
{
	printk(KERN_INFO "Exiting module.\n");
	return;
}

module_init(init_keylogger);
module_exit(exit_module);
