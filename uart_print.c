#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

MODULE_LICENSE("Dual BSD/GPL");

#define BUF_SIZE 512

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

void static_print(char* text){
	while(*text){
		_uart_print(*text++);
	}
}

void uart_print(char* fmt, ...){

	va_list args;

	// Allocate sprint buffer.
	// If BUF_SIZE isn't enough, message will becut.
	char *str_buf = kmalloc(BUF_SIZE, GFP_ATOMIC);
	char *orig_buf = str_buf;
	if(!str_buf){
		static_print("uart_print(): Failed to allocate kernel buffer for snprintf().\n\0");
		return;
	}

	va_start(args, fmt);
	vsnprintf(str_buf, BUF_SIZE, fmt, args);
	va_end(args);

	static_print(str_buf);

	kfree(orig_buf);

}

void buffer_uart_print(char* buffer, uint length, uint columns){

	int i;
	for(i = 1; i < length+1; i++){
		uart_print("%.2hhx ", buffer[i-1]);
		if(i % columns == 0)
			uart_print("\n");

	}
}
