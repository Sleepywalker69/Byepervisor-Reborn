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

static void check_call_frame(int prefix_ret)
{
  khook_t hook;
  kh_chain_t chain;
  uint8_t chain_buf[0x100];
  const uint64_t target = KBASE + 0x3000;
  uint64_t handler = SCRATCH + KH_OFF_BIG_CHAIN + 0x58;
  size_t target_off;

  setup(&hook);
  kh_chain_init(&chain, chain_buf, sizeof chain_buf);
  if (prefix_ret)
    assert(kh_chain_g(&chain, KERNEL_GADGET_RET_TRAMP) == 0);
  assert(kh_chain_call(&chain, target, 3, 1, 2, 3) == 0);
  target_off = chain.len - sizeof(uint64_t);
  assert(kh_install(&hook, &chain) == 0);
  assert(at(handler + target_off) == target);
  assert(((handler + target_off + sizeof(uint64_t)) & 0xF) == 8);
  if (prefix_ret)
    assert(at(handler + target_off - 8) == 0);
  else
    assert(at(handler + target_off - 8) == kgadget(KERNEL_GADGET_RET_TRAMP));
  assert(kh_restore(&hook) == 0);
}

static void check_invalid_call_rejected(void)
{
  khook_t hook;
  kh_chain_t chain;
  uint8_t chain_buf[0x100];

  setup(&hook);
  kh_chain_init(&chain, chain_buf, sizeof chain_buf);
  assert(kh_chain_call(&chain, 0, 3, 1, 2, 3) == -1);
  assert(chain.err == 1);
  assert(kh_install(&hook, &chain) == -11);
}

static void check_resume_chain_call_frame(void)
{
  khook_t hook;
  kh_chain_t chain;
  uint8_t chain_buf[0x800];
  const uint64_t target = KBASE + 0x2590b0;
  const uint64_t handler = SCRATCH + KH_OFF_BIG_CHAIN + 0x58;
  size_t target_off;

  setup(&hook);
  kh_chain_init(&chain, chain_buf, sizeof chain_buf);
  assert(kh_chain_store(&chain, SCRATCH + 0x440, 1) == 0);
  assert(kh_chain_g(&chain, KERNEL_GADGET_POP_RDX) == 0);
  assert(kh_chain_q(&chain, 0x1234000) == 0);
  assert(kh_chain_g(&chain, KERNEL_GADGET_POP_RCX) == 0);
  assert(kh_chain_q(&chain, SCRATCH + 0x448) == 0);
  assert(kh_chain_g(&chain, KERNEL_GADGET_ST_RCX_RDX) == 0);
  assert(kh_chain_memcpy(&chain, SCRATCH + 0x1000, SCRATCH + 0x2000, 0xf00) == 0);
  for (unsigned i = 0; i < 6; i++)
    assert(kh_chain_store(&chain, SCRATCH + 0x1f00 + 8 * i, i + 1) == 0);
  assert(chain.len == 0x180);
  assert(kh_chain_call(&chain, target, 3, SCRATCH + 0x668,
                       SCRATCH + 0x660, 8) == 0);
  target_off = chain.len - sizeof(uint64_t);
  assert(kh_chain_g(&chain, KERNEL_GADGET_POP_RCX) == 0);
  assert(kh_chain_q(&chain, SCRATCH + 0x670) == 0);
  assert(kh_chain_g(&chain, KERNEL_GADGET_ST_RCX_RAX) == 0);
  assert(kh_chain_store(&chain, SCRATCH + 0x678, 2) == 0);
  assert(kh_install(&hook, &chain) == 0);
  assert(at(handler + target_off) == target);
  assert(((handler + target_off + sizeof(uint64_t)) & 0xF) == 8);
  assert(at(handler + target_off - 8) == kgadget(KERNEL_GADGET_RET_TRAMP));
  assert(at(handler + target_off + 8) == kgadget(KERNEL_GADGET_POP_RCX));
  assert(kh_restore(&hook) == 0);
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

  check_call_frame(0);
  check_call_frame(1);
  check_invalid_call_rejected();
  check_resume_chain_call_frame();

  puts("hook copy/rollback and call-frame contract: 8 host cases passed");
  return 0;
}
