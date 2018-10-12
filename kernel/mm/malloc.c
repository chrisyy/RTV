/*                       RTV Real-Time Hypervisor
 * Copyright (C) 2017  Ying Ye
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mm/malloc.h"
#include "vm.h"
#include "debug.h"
#include "mm/physical.h"
#include "utils/list.h"
#include "utils/screen.h"

#define BUDDY_MIN_ORDER 5
#define BUDDY_MAX_ORDER 22

#define BUDDY_MIN_BLK (1 << BUDDY_MIN_ORDER)
#define BUDDY_MAX_BLK (1 << BUDDY_MAX_ORDER)

#define USED 0xFF

/*
 * Metadata about this particular block,
 * stored at the beginning of this block.
 * One per free block of memory.
 * There is also 1B meta isFree at the 
 * end of the block.
 */
typedef struct _buddy_list
{
  struct list_head list;
} buddy_list_t;

/*
 * Bucket of 2^order sized free memory blocks.
 */
typedef struct _buddy_bucket
{
    struct list_head ptr;
    uint32_t count;
    uint32_t size;
} buddy_bucket_t;

#define BUDDY_ENTRIES (BUDDY_MAX_ORDER - BUDDY_MIN_ORDER + 1)
static buddy_bucket_t bsystem[BUDDY_ENTRIES];
static void *mem_base = 0;

/* return the minimum N that 2^N >= v */
static uint8_t next_power2(uint32_t v)
{
  uint8_t cnt = BUDDY_MIN_ORDER;
  uint32_t p = 1 << cnt;

  if (v < BUDDY_MIN_BLK) 
    return BUDDY_MIN_ORDER;

  while (p < v) {
    p <<= 1;
    cnt++;
  }
  return cnt;
}

void check_buddy_table()
{
  buddy_list_t *blt;
  uint8_t entry = 0;
  uint32_t total = 0;

  while (1) {
    uint8_t counter = 0;

    printf("Count: %u", bsystem[entry].count);
    printf("  Order: %u\n", entry + BUDDY_MIN_ORDER);
    total += bsystem[entry].count * bsystem[entry].size;
    list_for_each_entry(blt, &bsystem[entry].ptr, list) {
      if (*(((uint8_t *) blt) + bsystem[entry].size - 1) != entry)
        printf("ERROR: postamble not set for %llX\n", (uint64_t) blt);
      counter++;
    }
    if (counter != bsystem[entry].count)
      printf("Count is incorrect!");

    entry++;
    if (entry >= BUDDY_ENTRIES)
        break;
  }

  printf("Total free memory: %u\n", total);
}

/* See if we can merge */
static uint8_t buddy_try_merge(buddy_list_t *blt, uint8_t entry)
{
  uint8_t merge_count = 0;

  if (bsystem[entry].count == 0)
    return 0;

  while (entry < BUDDY_ENTRIES - 1) {
    uint64_t buddy_addr = (uint64_t) blt;
    /* find the buddy */
    buddy_addr -= (uint64_t) mem_base;
    buddy_addr ^= bsystem[entry].size;
    buddy_addr += (uint64_t) mem_base;

    /* merge */
    if (*(uint8_t *) (buddy_addr + bsystem[entry].size - 1) == entry) {
      buddy_list_t *buddy = (buddy_list_t *) buddy_addr;
      buddy_list_t *merged;
      if (merge_count > 0) {
        bsystem[entry].count--;
        list_del(&blt->list);
      }

      bsystem[entry].count--;
      list_del(&buddy->list);

      entry++;
      merged = (blt < buddy) ? blt : buddy;
      *(((uint8_t *) merged) + bsystem[entry].size - 1) = entry;
      list_add(&merged->list, &bsystem[entry].ptr);
      bsystem[entry].count++;

      blt = merged;
      merge_count++;
    } else
      break;
  }

  return merge_count;
}

/*
 * Free the given pointer (and coalesce it with its buddy if possible).
 * @param ptr The pointer to free.
 */
void free(void *ptr)
{
  uint8_t *addr = (uint8_t *) ptr;
  uint8_t entry;
  buddy_list_t *blt;

  addr -= sizeof(uint64_t);
  entry = *addr;
  if (*(addr + bsystem[entry].size - 1) != USED) {
    printf("%s: double free memory %llX\n", __func__, (uint64_t) ptr);
    return;
  }
  blt = (buddy_list_t *) addr;

  if (buddy_try_merge(blt, entry))
    return;

  /* no merge, add it to the list */
  *(addr + bsystem[entry].size - 1) = entry;
  list_add(&blt->list, &bsystem[entry].ptr);
  bsystem[entry].count++;
}

static void buddy_split(buddy_bucket_t *bucket)
{
  uint8_t *post;
  uint8_t entry;
  buddy_list_t *new_blt;
  buddy_list_t *blt = list_first_entry(&bucket->ptr, buddy_list_t, list);
  list_del(&blt->list);
  bucket->count--;

  /* Add two blocks to the lower order bucket */
  bucket--;
  bucket->count += 2;

  new_blt = (buddy_list_t *) (((uint8_t *) blt) + bucket->size);
  list_add(&new_blt->list, &bucket->ptr);
  list_add(&blt->list, &bucket->ptr);
  post = ((uint8_t *) new_blt) - 1;
  entry = (uint8_t) (bucket - &bsystem[0]);
  *post = entry;
  post += bucket->size;
  *post = entry;
}

/*
 * Find the smallest block that will contain size and return it.
 * @param size The size of the data this allocation must be able to hold.
 */
void *malloc(uint32_t size)
{
  uint8_t *pre, *post;
  buddy_list_t *blt;
  /* 
   * preamble: block order (8B), only the first 1B stores
   * the order, the 8B is for alignment of the allocated data,
   * a performance optimization
   * postamble: block order (1B), if block is used, set to USED
   */
  uint8_t entry = next_power2(size + 9) - BUDDY_MIN_ORDER;

  if (bsystem[entry].count == 0) {
    uint8_t split_count = 0;

    while (bsystem[entry].count == 0) {
      entry++;
      if (entry >= BUDDY_ENTRIES) 
        panic("out of memory");
      split_count++;
    }

    while (split_count--) {
      buddy_split(&bsystem[entry]);
      entry--;
    }
  }

  blt = list_first_entry(&bsystem[entry].ptr, buddy_list_t, list);
  list_del(&blt->list);
  bsystem[entry].count--;
  /* preamble/postamble */
  pre = (uint8_t *) blt;
  post = pre + bsystem[entry].size - 1;
  *pre = entry;
  pre += sizeof(uint64_t);
  *post = USED;

  return pre;
}

void malloc_init(void)
{
  uint64_t size, pages;
  uint64_t paddr, vaddr;
  buddy_list_t *initial;
  uint32_t i;
  uint8_t list_count = BUDDY_MAX_ORDER - BUDDY_MIN_ORDER + 1;

  if ((1 << BUDDY_MIN_ORDER) < sizeof(buddy_list_t) + 1)
    panic("minimum block size too large");

  for (i = 0; i < list_count; i++) {
    bsystem[i].count = 0;
    bsystem[i].size = 1 << (i + BUDDY_MIN_ORDER);
    INIT_LIST_HEAD(&bsystem[i].ptr);
  }

  size = 1 << BUDDY_MAX_ORDER;
  pages = 1 << (BUDDY_MAX_ORDER - PG_BITS);
  vaddr = MALLOC_PHY_BASE;
  /* malloc memory is not executable, set XD */
  if (size < LARGE_PG_SIZE) {
    paddr = alloc_phys_frames(pages);
    if (paddr == 0)
      panic("out of physical memory");
    for (i = 0; i < pages; i++) {
      vm_map_page_unrestricted(paddr, PGT_P | PGT_RW | PGT_XD, vaddr);
      vaddr += PG_SIZE;
      paddr += PG_SIZE;
    }
  } else {
    paddr = alloc_phys_frames_aligned(pages, LARGE_PG_SIZE);
    if (paddr == 0)
      panic("out of physical memory");
    pages = 1 << (BUDDY_MAX_ORDER - LARGE_PG_BITS);
    for (i = 0; i < pages; i++) {
      vm_map_large_page_unrestricted(paddr, PDT_P | PDT_RW | PDT_XD | PDT_PS, vaddr);
      vaddr += LARGE_PG_SIZE;
      paddr += LARGE_PG_SIZE;
    }
  }
  mem_base = (void *) MALLOC_PHY_BASE;

  /* set up the initial buddy block */
  initial = (buddy_list_t *) mem_base;
  list_count--;
  bsystem[list_count].count = 1;
  list_add(&initial->list, &bsystem[list_count].ptr);
  *(((uint8_t *) mem_base) + bsystem[list_count].size - 1)
    = list_count;
}
