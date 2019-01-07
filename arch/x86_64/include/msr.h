#ifndef _MSR_H_
#define _MSR_H_

/* MSR */
#define IA32_FEATURE_CONTROL          0x3A
#define IA32_VMX_BASIC                0x480
#define IA32_VMX_PINBASED_CTLS        0x481
#define IA32_VMX_PROCBASED_CTLS       0x482
#define IA32_VMX_EXIT_CTLS            0x483
#define IA32_VMX_ENTRY_CTLS           0x484
#define IA32_VMX_CR0_FIXED0           0x486
#define IA32_VMX_CR0_FIXED1           0x487
#define IA32_VMX_CR4_FIXED0           0x488
#define IA32_VMX_CR4_FIXED1           0x489
#define IA32_VMX_PROCBASED_CTLS2      0x48B
#define IA32_VMX_TRUE_PINBASED_CTLS   0x48D
#define IA32_VMX_TRUE_PROCBASED_CTLS  0x48E
#define IA32_VMX_TRUE_EXIT_CTLS       0x48F
#define IA32_VMX_TRUE_ENTRY_CTLS      0x490
#define IA32_EFER                     0xC0000080
#define IA32_FS_BASE                  0xC0000100
#define IA32_GS_BASE                  0xC0000101

#endif
