#ifndef __SERIALBOOT_H
#define __SERIALBOOT_H

int serialboot(void);
void boot(unsigned long r1, unsigned long r2, unsigned long r3, unsigned long addr);

#endif // __SERIALBOOT_H