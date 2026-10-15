#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <ps5/kernel.h>
#include "../include/attempt_receipt.h"

#define DATA_CAVE_OFF 0x2d39000
#define KH_OFF_USER 0x440
#define CAVE_STATE_OFF 0x600
#define CAVE_SAVE_OFF 0x620
#define IDT_ARRAY_OFF 0x3bb4300
#define COMMON_TSS_OFF 0x3bb6fe0
#define PMAP_STORE_OFF 0x3b48ff8
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
        puts("BPR_CALL_PROBE_V1 ok=0 reason=identity");
        return 2;
    }

    const uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
    const uint64_t cave = base + DATA_CAVE_OFF;
    uint64_t stage = 0, hook = 0, arm = 0, complete = 0, root = 0;
    uint64_t saved_lo = 0, saved_hi = 0, saved_ist = 0;
    uint64_t live_lo = 0, live_hi = 0, live_ist = 0;
    uint64_t src = 0, dst = 0, ret = 0, done = 0;
    if (!base ||
        read_q(cave + KH_OFF_USER, &stage) ||
        read_q(cave + CAVE_STATE_OFF, &hook) ||
        read_q(cave + CAVE_ARM_ID_OFF, &arm) ||
        read_q(cave + CAVE_COMPLETE_ID_OFF, &complete) ||
        read_q(base + PMAP_STORE_OFF + 0x28, &root) ||
        read_q(cave + CAVE_SAVE_OFF, &saved_lo) ||
        read_q(cave + CAVE_SAVE_OFF + 8, &saved_hi) ||
        read_q(cave + CAVE_SAVE_OFF + 16, &saved_ist) ||
        read_q(base + IDT_ARRAY_OFF + 16, &live_lo) ||
        read_q(base + IDT_ARRAY_OFF + 24, &live_hi) ||
        read_q(base + COMMON_TSS_OFF + HOOK_IST_OFF, &live_ist) ||
        read_q(cave + CAVE_CALL_SRC_OFF, &src) ||
        read_q(cave + CAVE_CALL_DST_OFF, &dst) ||
        read_q(cave + CAVE_CALL_RET_OFF, &ret) ||
        read_q(cave + CAVE_CALL_DONE_OFF, &done)) {
        puts("BPR_CALL_PROBE_V1 ok=0 reason=read");
        return 1;
    }

    const unsigned receipt_ok = root != 0 && (root & 0xfff) == 0 &&
        attempt_receipt_valid(stage, hook, arm, complete,
                              saved_lo, saved_hi, live_lo, live_hi,
                              saved_ist, live_ist);
    const unsigned call_ok = receipt_ok && src == CALL_SRC_SENTINEL &&
        dst == CALL_SRC_SENTINEL && ret == cave + CAVE_CALL_DST_OFF &&
        done == CALL_DONE_SENTINEL;
    printf("BPR_CALL_PROBE_V1 ok=%u receipt=%u fw=%08x boot=%jd.%06ld "
           "base=%016" PRIx64 " root=%016" PRIx64 " stage=%016" PRIx64 " "
           "hook=%016" PRIx64 " arm=%016" PRIx64 " complete=%016" PRIx64 " "
           "gate=%u ist=%u src=%016" PRIx64 " dst=%016" PRIx64 " "
           "ret=%016" PRIx64 " done=%016" PRIx64 "\n",
           call_ok, receipt_ok, fw, (intmax_t)boot.tv_sec, (long)boot.tv_usec,
           base, root, stage, hook, arm, complete,
           (saved_lo || saved_hi) && saved_lo == live_lo && saved_hi == live_hi,
           saved_ist == live_ist, src, dst, ret, done);
    return call_ok ? 0 : 1;
}
