#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include "types.h"
#include "cpu.h"
#include "atomic.h"
#include "smp.h"

typedef union _spinlock {
  uint32_t data;
  struct {
    uint16_t next;
    uint16_t owner;
  } obj;
} spinlock_t;

#define SPINLOCK_UNLOCKED {.data = 0}

static inline void spin_lock_init(spinlock_t *lock)
{
  atomic_store(&lock->data, 0);
}

static inline void spin_lock(spinlock_t *lock)
{
  uint16_t me = atomic_fetch_add(&lock->obj.next, 1);
  while (me != lock->obj.owner)
    pause();
}

static inline void spin_unlock(spinlock_t *lock)
{
  atomic_increment(&lock->obj.owner);
}

/* return true if acquired lock */
static inline bool spin_trylock(spinlock_t *lock)
{
  uint16_t me = lock->obj.next;
  uint16_t next = me + 1;
  uint32_t data = ((uint32_t) me << 16) + me;
  uint32_t new_data = ((uint32_t) next << 16) + me;

  return atomic_cmp_xchg(&lock->data, &data, new_data);
}

#endif
