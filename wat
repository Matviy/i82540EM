/usr/src/i82540EM/main.c: In function ‘i82540EM_isr’:
/usr/src/i82540EM/main.c:21:6: warning: unused variable ‘i’ [-Wunused-variable]
   21 |  int i = 0;
      |      ^
/usr/src/i82540EM/main.c: In function ‘rx_data’:
/usr/src/i82540EM/main.c:40:34: warning: initialization of ‘struct i82540EM *’ from ‘long unsigned int’ makes pointer from integer without a cast [-Wint-conversion]
   40 |  struct i82540EM *i82540EM_dev = param;
      |                                  ^~~~~
/usr/src/i82540EM/main.c:47:2: warning: ISO C90 forbids mixed declarations and code [-Wdeclaration-after-statement]
   47 |  int i = 0;
      |  ^~~
/usr/src/i82540EM/main.c: In function ‘i82540EM_probe’:
/usr/src/i82540EM/main.c:181:51: warning: passing argument 3 of ‘tasklet_init’ makes integer from pointer without a cast [-Wint-conversion]
  181 |  tasklet_init(&i82540EM_dev->rx_tasklet, rx_data, i82540EM_dev);
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
/usr/src/i82540EM/main.c:233:2: warning: ISO C90 forbids mixed declarations and code [-Wdeclaration-after-statement]
  233 |  u32 ctrl = readl(i82540EM_dev->regs + i82540EM_CTRL);
      |  ^~~
/usr/src/i82540EM/main.c:290:2: warning: ISO C90 forbids mixed declarations and code [-Wdeclaration-after-statement]
  290 |  u32 rctl = readl(i82540EM_dev->regs + i82540EM_RCTL);
      |  ^~~
/usr/src/i82540EM/main.c: In function ‘i82540EM_remove’:
/usr/src/i82540EM/main.c:338:2: warning: ISO C90 forbids mixed declarations and code [-Wdeclaration-after-statement]
  338 |  struct net_device *net_dev = pci_get_drvdata(pci_dev);
      |  ^~~~~~
