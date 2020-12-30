/usr/src/i82540em/i82540EM.c: In function ‘i82540EM_probe’:
/usr/src/i82540em/i82540EM.c:139:2: warning: ISO C90 forbids mixed declarations and code [-Wdeclaration-after-statement]
  139 |  u32 ctrl = readl(i82540EM_dev->regs + i82540EM_CTRL);
      |  ^~~
/usr/src/i82540em/i82540EM.c:197:2: warning: ISO C90 forbids mixed declarations and code [-Wdeclaration-after-statement]
  197 |  u32 rctl = readl(i82540EM_dev->regs + i82540EM_RCTL);
      |  ^~~
/usr/src/i82540em/i82540EM.c: In function ‘i82540EM_remove’:
/usr/src/i82540em/i82540EM.c:247:2: warning: ISO C90 forbids mixed declarations and code [-Wdeclaration-after-statement]
  247 |  struct net_device *net_dev = pci_get_drvdata(pci_dev);
      |  ^~~~~~
