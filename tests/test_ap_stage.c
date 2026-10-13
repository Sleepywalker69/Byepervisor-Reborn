#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../include/stage_status.h"

void vmcb_disable_np(uint64_t *param);

static uint8_t memory[0x30000];
static uint64_t stage;
static uint64_t params[6];
static const uint64_t heap = UINT64_C(0x100000000);

static void put64(size_t offset, uint64_t value)
{
  memcpy(memory + offset, &value, sizeof value);
}

static uint64_t get64(size_t offset)
{
  uint64_t value;
  memcpy(&value, memory + offset, sizeof value);
  return value;
}

static void setup(void)
{
  memset(memory, 0, sizeof memory);
  memset(params, 0, sizeof params);
  stage = STAGE_HOOK_FIRED;
  params[1] = (uint64_t)(uintptr_t)memory;
  params[2] = (uint64_t)(uintptr_t)&stage;
  params[4] = 0x80;
  params[5] = 0x100;
  put64(0x80, heap);
  for (int core = 0; core < 16; core++) {
    size_t pcpu_pa = 0x1000 + (size_t)core * 0x100;
    size_t vmcb_pa = 0x10000 + (size_t)core * 0x1000;
    put64(0x100 + (size_t)core * 8, heap + pcpu_pa);
    put64(pcpu_pa + 0xa8, heap + vmcb_pa);
    put64(vmcb_pa + 0x90, 9);
    put64(vmcb_pa + 0xb0, UINT64_C(0x200000) + (uint64_t)core * 0x1000);
    put64(vmcb_pa + 0xb8, 1);
    put64(vmcb_pa + 0xc0, UINT32_MAX);
  }
}

static void assert_untouched(void)
{
  for (int core = 0; core < 16; core++) {
    size_t vmcb_pa = 0x10000 + (size_t)core * 0x1000;
    assert(get64(vmcb_pa + 0x90) == 9);
    assert(get64(vmcb_pa + 0xc0) == UINT32_MAX);
  }
}

int main(void)
{
  setup();
  vmcb_disable_np(params);
  assert(stage == STAGE_AP_COMPLETE);
  for (int core = 0; core < 16; core++) {
    size_t vmcb_pa = 0x10000 + (size_t)core * 0x1000;
    assert(get64(vmcb_pa + 0x90) == 8);
    assert(get64(vmcb_pa + 0xc0) == 0);
  }
  vmcb_disable_np(params); /* A later AP must not downgrade completion. */
  assert(stage == STAGE_AP_COMPLETE);

  setup();
  put64(0x100 + 15 * 8, 0);
  vmcb_disable_np(params);
  assert(stage == STAGE_AP_INVALID);
  assert_untouched();

  setup();
  put64(0x10000 + 15 * 0x1000 + 0x90, 7);
  vmcb_disable_np(params);
  assert(stage == STAGE_AP_INVALID);
  for (int core = 0; core < 15; core++) {
    size_t vmcb_pa = 0x10000 + (size_t)core * 0x1000;
    assert(get64(vmcb_pa + 0x90) == 9);
  }

  setup();
  put64(0x100 + 15 * 8, get64(0x100 + 14 * 8));
  vmcb_disable_np(params);
  assert(stage == STAGE_AP_INVALID);
  assert_untouched();

  setup();
  stage = STAGE_AP_INVALID;
  vmcb_disable_np(params);
  assert(stage == STAGE_AP_INVALID);

  puts("AP stage completion gates: 5 host cases passed");
  return 0;
}
