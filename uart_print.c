#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("Dual BSD/GPL");

void _uart_print(char character){
	asm volatile(
		"mov $0x3FD, %%dx\n\t"	\
		"in %%dx, %%al\n\t"	\
		"and $0x20, %%al\n\t"	\
		"cmp $0x20, %%al\n\t"	\
		"jne .-5\n\t"		\
					\
		"mov %0, %%al\n\t"	\
		"mov $0x3F8, %%dx\n\t"	\
		"out %%al, %%dx"	\
					\
		: /* No outputs */	\
		: "r" (character)	\
		: "rax", "rdx"		\
	);
}

// Uses the uart to print a null-terminated string.
// If it's not null terminated, you're gonna goof.
void uart_print(char* str){
	while(*str){
		_uart_print(*str++);
	}
}

