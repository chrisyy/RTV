#ifndef _BOOT_INFO_H_
#define _BOOT_INFO_H_

#include "types.h"

#define BOOT_STRING_MAX 32

typedef struct _boot_info_t {
  uint64_t config_paddr;
  uint32_t config_size;
  uint16_t num_mod;
  char **mod_str;
  uint64_t *mod_paddr;
  uint64_t *mod_size;
  uint16_t *num_cpus;
  uint32_t *ram_size;
} boot_info_t;

void parse_boot_info(boot_info_t *info);

#endif
