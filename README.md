# linux_kernel_project

Usage Instructions
1. Run command "make"
2. Load module into kernel using "sudo insmod keylogger.ko"
3. Remove module from kernel using "sudo rmmod keylogger.ko"


Supported features
- supports all keys on a standard US keyboard
- writes all pressed keys to the terminal using "dmesg"
- writes all characters to two output files
- raw_output.txt contains all keys pressed without any formatting
- simple_output.txt contains all alphanumeric and punctuation keys with formatting
- prints out timestamps in the terminal and output files every fifteen seconds
- prints out the number of words captured by the program once unloaded from the kernel; this can be seen by using "dmesg" after unloading the kernel for the module

Limitations
- each output buffer can only contain 1000 characters
- program can't determine when the window is switched, so backspace can produce inaccurate results
