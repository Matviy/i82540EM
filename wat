/usr/src/i82540EM/main.c: In function ‘test_isr’:
/usr/src/i82540EM/main.c:22:6: warning: unused variable ‘i’ [-Wunused-variable]
   22 |  int i = 0;
      |      ^
/usr/src/i82540EM/main.c: In function ‘rx_data’:
/usr/src/i82540EM/main.c:41:34: warning: initialization of ‘struct i82540EM *’ from ‘long unsigned int’ makes pointer from integer without a cast [-Wint-conversion]
   41 |  struct i82540EM *i82540EM_dev = param;
      |                                  ^~~~~
/usr/src/i82540EM/main.c:48:2: warning: ISO C90 forbids mixed declarations and code [-Wdeclaration-after-statement]
   48 |  int i = 0;
      |  ^~~
In file included from ./include/linux/printk.h:7,
                 from ./include/linux/kernel.h:15,
                 from ./include/linux/list.h:9,
                 from ./include/linux/module.h:9,
                 from /usr/src/i82540EM/main.c:1:
./include/linux/kern_levels.h:5:18: warning: format ‘%llx’ expects argument of type ‘long long unsigned int’, but argument 8 has type ‘void *’ [-Wformat=]
    5 | #define KERN_SOH "\001"  /* ASCII Start Of Header */
      |                  ^~~~~~
./include/linux/kern_levels.h:14:19: note: in expansion of macro ‘KERN_SOH’
   14 | #define KERN_INFO KERN_SOH "6" /* informational */
      |                   ^~~~~~~~
/usr/src/i82540EM/main.c:50:10: note: in expansion of macro ‘KERN_INFO’
   50 |   printk(KERN_INFO "rx_data(): Descriptor[%d]: Length:%x Status:%x Checksum:%x Error:%x Special: %x Buffer DMA Address: %llx\n",
      |          ^~~~~~~~~
/usr/src/i82540EM/main.c:50:124: note: format string is defined here
   50 |   printk(KERN_INFO "rx_data(): Descriptor[%d]: Length:%x Status:%x Checksum:%x Error:%x Special: %x Buffer DMA Address: %llx\n",
      |                                                                                                                         ~~~^
      |                                                                                                                            |
      |                                                                                                                            long long unsigned int
      |                                                                                                                         %p
/usr/src/i82540EM/main.c: In function ‘i82540EM_probe’:
/usr/src/i82540EM/main.c:190:51: warning: passing argument 3 of ‘tasklet_init’ makes integer from pointer without a cast [-Wint-conversion]
  190 |  tasklet_init(&i82540EM_dev->rx_tasklet, rx_data, i82540EM_dev);
      |                                                   ^~~~~~~~~~~~
      |                                                   |
      |                                                   struct i82540EM *
In file included from ./include/linux/kernel_stat.h:9,
                 from ./include/linux/cgroup.h:26,
                 from ./include/net/netprio_cgroup.h:11,
                 from ./include/linux/netdevice.h:42,
                 from ./include/linux/etherdevice.h:21,
                 from /usr/src/i82540EM/main.c:4:
./include/linux/interrupt.h:674:48: note: expected ‘long unsigned int’ but argument is of type ‘struct i82540EM *’
  674 |     void (*func)(unsigned long), unsigned long data);
      |                                  ~~~~~~~~~~~~~~^~~~
/usr/src/i82540EM/main.c:248:2: warning: ISO C90 forbids mixed declarations and code [-Wdeclaration-after-statement]
  248 |  u32 ctrl = readl(i82540EM_dev->regs + i82540EM_CTRL);
      |  ^~~
/usr/src/i82540EM/main.c:313:2: warning: ISO C90 forbids mixed declarations and code [-Wdeclaration-after-statement]
  313 |  u32 rctl = readl(i82540EM_dev->regs + i82540EM_RCTL);
      |  ^~~
/usr/src/i82540EM/main.c: In function ‘i82540EM_remove’:
/usr/src/i82540EM/main.c:367:2: warning: ISO C90 forbids mixed declarations and code [-Wdeclaration-after-statement]
  367 |  struct net_device *net_dev = pci_get_drvdata(pci_dev);
      |  ^~~~~~
