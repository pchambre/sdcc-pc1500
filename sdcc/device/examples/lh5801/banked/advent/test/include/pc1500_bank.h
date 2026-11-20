/* pc1500_bank.h for lcdtest.sh: one address space, nothing to switch. */
#ifndef PC1500_BANK_H
#define PC1500_BANK_H

void bank_select (unsigned char bank);
unsigned char bank_current (void);

#endif
