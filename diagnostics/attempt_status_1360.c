#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <ps5/kernel.h>
#include "../include/attempt_receipt.h"

#define DATA_CAVE_OFF 0x2D39000
#define KH_OFF_USER 0x440
#define CAVE_STATE_OFF 0x600
#define CAVE_SAVE_OFF 0x620
#define IDT_ARRAY_OFF 0x3BB4300
#define COMMON_TSS_OFF 0x3BB6FE0
#define HOOK_IST_OFF 0x4c

static int read_q(uint64_t address, uint64_t *value)
{
    *value = 0;
    return kernel_copyout(address, value, sizeof(*value));
}

int main(void)
{
    const uint32_t fw = kernel_get_fw_version();
    struct timeval boot = {0};
    size_t boot_len = sizeof(boot);
    if (fw != 0x13600007U ||
        sysctlbyname("kern.boottime", &boot, &boot_len, NULL, 0) ||
        boot_len != sizeof(boot) || boot.tv_sec <= 0 ||
        boot.tv_usec < 0 || boot.tv_usec >= 1000000) {
        puts("BPR_STATUS_V2 ok=0 reason=identity");
        return 2;
    }

    const uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
    const uint64_t cave = base + DATA_CAVE_OFF;
    uint64_t stage = 0, hook = 0, arm = 0, complete = 0;
    uint64_t saved_lo = 0, saved_hi = 0, saved_ist = 0;
    uint64_t live_lo = 0, live_hi = 0, live_ist = 0;
    if (!base ||
        read_q(cave + KH_OFF_USER, &stage) ||
        read_q(cave + CAVE_STATE_OFF, &hook) ||
        read_q(cave + CAVE_ARM_ID_OFF, &arm) ||
        read_q(cave + CAVE_COMPLETE_ID_OFF, &complete) ||
        read_q(cave + CAVE_SAVE_OFF, &saved_lo) ||
        read_q(cave + CAVE_SAVE_OFF + 8, &saved_hi) ||
        read_q(cave + CAVE_SAVE_OFF + 16, &saved_ist) ||
        read_q(base + IDT_ARRAY_OFF + 16, &live_lo) ||
        read_q(base + IDT_ARRAY_OFF + 24, &live_hi) ||
        read_q(base + COMMON_TSS_OFF + HOOK_IST_OFF, &live_ist)) {
        puts("BPR_STATUS_V2 ok=0 reason=read");
        return 1;
    }

    const unsigned gate_ok = (saved_lo || saved_hi) &&
                             saved_lo == live_lo && saved_hi == live_hi;
    const unsigned ist_ok = saved_ist == live_ist;
    const unsigned ok = attempt_receipt_valid(stage, hook, arm, complete,
                                               saved_lo, saved_hi,
                                               live_lo, live_hi,
                                               saved_ist, live_ist);
    printf("BPR_STATUS_V2 ok=%u fw=%08x boot=%jd.%06ld "
           "base=%016" PRIx64 " stage=%016" PRIx64 " "
           "hook=%016" PRIx64 " arm=%016" PRIx64 " "
           "complete=%016" PRIx64 " gate=%u ist=%u\n",
           ok, fw, (intmax_t)boot.tv_sec, (long)boot.tv_usec,
           base, stage, hook, arm, complete, gate_ok, ist_ok);
    return ok ? 0 : 1;
}
