cmd_/usr/src/i82540EM/i82540EM.ko := ld -r -m elf_x86_64  -z max-page-size=0x200000  --build-id  -T ./scripts/module-common.lds -o /usr/src/i82540EM/i82540EM.ko /usr/src/i82540EM/i82540EM.o /usr/src/i82540EM/i82540EM.mod.o;  true
