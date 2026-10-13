#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../include/khook.h"

#define KBASE UINT64_C(0xffffffff80000000)
#define SCRATCH UINT64_C(0x10000000)
#define IDT UINT64_C(0x20000000)
#define TSS UINT64_C(0x30000000)

static uint8_t scratch[0x5000], idt[0x1000], tss[0x1000];
static uint64_t gate_lo, gate_hi, saved_ist;
static uint64_t fail_read, fail_write;

uint64_t kgadget(kgadget_t gadget)
{
  return KBASE + 0x10000 + (uint64_t)gadget * 0x10;
}

static uint8_t *map(uint64_t addr, size_t len)
{
  if (addr >= SCRATCH && addr - SCRATCH <= sizeof scratch - len)
    return scratch + (addr - SCRATCH);
  if (addr >= IDT && addr - IDT <= sizeof idt - len)
    return idt + (addr - IDT);
  if (addr >= TSS && addr - TSS <= sizeof tss - len)
    return tss + (addr - TSS);
  return NULL;
}

static int fake_read(uint64_t addr, void *dst, size_t len)
{
  uint8_t *src = map(addr, len);
  if (!src || addr == fail_read)
    return -1;
  memcpy(dst, src, len);
  return 0;
}

static int fake_write(uint64_t addr, const void *src, size_t len)
{
  uint8_t *dst = map(addr, len);
  if (!dst)
    return -1;
  if (addr == fail_write) {
    fail_write = 0;  /* one failing write, so rollback can be exercised */
    return -1;
  }
  memcpy(dst, src, len);
  return 0;
}

static uint64_t at(uint64_t addr)
{
  uint64_t value = 0;
  assert(fake_read(addr, &value, sizeof value) == 0);
  return value;
}

static void set(uint64_t addr, uint64_t value)
{
  assert(fake_write(addr, &value, sizeof value) == 0);
}

static void setup(khook_t *hook)
{
  memset(scratch, 0, sizeof scratch);
  memset(idt, 0, sizeof idt);
  memset(tss, 0, sizeof tss);
  fail_read = fail_write = 0;
  gate_lo = UINT64_C(0x80008e0000201000);
  gate_hi = UINT64_C(0xffffffff);
  saved_ist = UINT64_C(0xffff800000200000);
  set(IDT + 16, gate_lo);
  set(IDT + 24, gate_hi);
  set(TSS + 4, UINT64_C(0xffff800000100000));
  set(TSS + KH_TSS_IST(6), saved_ist);
  assert(kh_init(hook, fake_read, fake_write, KBASE, SCRATCH, 6) == 0);
  kh_set_tables(hook, IDT, TSS);
  kh_set_core(hook, 0);
  kh_set_hook_target(hook, KBASE + 0x2000);
  hook->oneshot = 1;
}

int main(void)
{
  khook_t hook;

  setup(&hook);
  fail_read = IDT + 16;
  assert(kh_install(&hook, NULL) == -17);
  assert(at(TSS + KH_TSS_IST(6)) == saved_ist);
  fail_read = 0;

  setup(&hook);
  fail_write = IDT + 16;
  assert(kh_install(&hook, NULL) == -10);
  assert(at(IDT + 16) == gate_lo);
  assert(at(IDT + 24) == gate_hi);
  assert(at(TSS + KH_TSS_IST(6)) == saved_ist);

  setup(&hook);
  assert(kh_install(&hook, NULL) == 0);
  fail_write = SCRATCH + KH_OFF_RESUME + KH_RC_RBP;
  assert(kh_trigger(&hook) == -2);
  assert(kh_restore(&hook) == 0);

  setup(&hook);
  assert(kh_install(&hook, NULL) == 0);
  assert(hook.installed == 1);
  assert(kh_restore(&hook) == 0);
  assert(hook.installed == 0);
  assert(at(IDT + 16) == gate_lo);
  assert(at(IDT + 24) == gate_hi);
  assert(at(TSS + KH_TSS_IST(6)) == saved_ist);

  puts("hook copy/rollback contract: 4 host cases passed");
  return 0;
}
