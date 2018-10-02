#ifndef _BOOT_INFO_H_
#define _BOOT_INFO_H_

#include "types.h"

typedef struct _boot_info_t {
  uint64_t config_paddr;
  uint32_t config_size;
  uint32_t num_mod;
  char **mod_str;
  uint64_t *mod_paddr;
  uint32_t *mod_size;
} boot_info_t;

void parse_boot_info(uint8_t *addr, boot_info_t *info);

#endif
