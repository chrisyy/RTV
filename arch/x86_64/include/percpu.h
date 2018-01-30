#ifndef _PERCPU_H_
#define _PERCPU_H_

#define PER_CPU_SEG_STR "fs"

/* Define a per-CPU variable */
#define DEF_PER_CPU(type,var) __attribute__ ((section(".percpu"))) type var

/* Define an initialization function for a per-CPU variable */
#define INIT_PER_CPU(var)                                                       \
  void __##var##_ctor_func(void);                                               \
  __attribute__ ((section(".percpu.ctor"))) void (*__##var##_ctor_ptr)(void) =  \
    __##var##_ctor_func;                                                        \
  void __##var##_ctor_func(void)

#define __percpu_arg(x) "%%"PER_CPU_SEG_STR":%"#x

#define percpu_write(var, val)                          \
  do {                                                  \
    switch (sizeof(typeof(var))) {                      \
    case 1:                                             \
      __asm__ volatile("movb %1, "__percpu_arg(0)       \
                       : "+m" (var) : "qi" (val));      \
      break;                                            \
    case 2:                                             \
      __asm__ volatile("movw %1, "__percpu_arg(0)       \
                       : "+m" (var) : "ri" (val));      \
      break;                                            \
    case 4:                                             \
      __asm__ volatile("movl %1, "__percpu_arg(0)       \
                       : "+m" (var) : "ri" (val));      \
      break;                                            \
    case 8:                                             \
      __asm__ volatile("movq %1, "__percpu_arg(0)       \
                       : "+m" (var) : "ri" (val));      \
      break;                                            \
    default:                                            \
      panic(__func__, "percpu_op_dest: bad size");      \
    }                                                   \
  } while (0)

#define percpu_read(var)                                \
  ({                                                    \
    typeof(var) _percpu_ret_;                           \
    switch (sizeof(typeof(var))) {                      \
    case 1:                                             \
      __asm__ volatile("movb "__percpu_arg(1)", %0"     \
                       : "=q" (_percpu_ret_)            \
                       : "m" (var));                    \
      break;                                            \
    case 2:                                             \
      __asm__ volatile("movw "__percpu_arg(1)", %0"     \
                       : "=r" (_percpu_ret_)            \
                       : "m" (var));                    \
      break;                                            \
    case 4:                                             \
      __asm__ volatile("movl "__percpu_arg(1)", %0"     \
                       : "=r" (_percpu_ret_)            \
                       : "m" (var));                    \
      break;                                            \
    case 8:                                             \
      __asm__ volatile("movq "__percpu_arg(1)", %0"     \
                       : "=r" (_percpu_ret_)            \
                       : "m" (var));                    \
      break;                                            \
    default:                                            \
      panic(__func__, "percpu_op_src: bad size");       \
    }                                                   \
    _percpu_ret_;                                       \
  }) 

extern uint8_t *percpu_virt[MAX_CPUS];

extern void percpu_init(void);

#endif
