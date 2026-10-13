#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <ps5/kernel.h>

#include "../include/attempt_receipt.h"
#include "../include/y2jb_arm_file.h"

#define DATA_CAVE_OFF 0x2D39000ULL
#define KH_OFF_USER 0x440ULL
#define CAVE_STATE_OFF 0x600ULL
#define CAVE_SAVE_OFF 0x620ULL
#define IDT_ARRAY_OFF 0x3BB4300ULL
#define COMMON_TSS_OFF 0x3BB6FE0ULL
#define PMAP_STORE_OFF 0x3B48FF8ULL
#define HOOK_IST_OFF 0x4cULL

static int read_q(uint64_t address, uint64_t *value)
{
    *value = 0;
    return kernel_copyout(address, value, sizeof(*value));
}

static int arm_file_matches(const char *title, uint64_t arm,
                            uint64_t base, uint64_t root)
{
    char path[128];
    if (snprintf(path, sizeof(path),
                 "/user/temp/common_temp/bpr-y2jb-%s.log", title) <= 0)
        return 0;
    struct stat st;
    if (stat(path, &st) || st.st_size <= 0 || st.st_size > 4096)
        return 0;
    FILE *file = fopen(path, "r");
    if (!file)
        return 0;
    int ok = y2jb_arm_file_matches(file, title, arm, base, root);
    fclose(file);
    return ok;
}

int main(void)
{
    struct timeval boot = {0};
    size_t boot_len = sizeof(boot);
    if (kernel_get_fw_version() != 0x13600007U ||
        sysctlbyname("kern.boottime", &boot, &boot_len, NULL, 0) ||
        boot_len != sizeof(boot) || boot.tv_sec <= 0 ||
        boot.tv_usec < 0 || boot.tv_usec >= 1000000)
        return 2;

    const uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
    const uint64_t cave = base + DATA_CAVE_OFF;
    uint64_t stage, hook, arm, complete, root;
    uint64_t saved_lo, saved_hi, saved_ist;
    uint64_t live_lo, live_hi, live_ist;
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
        read_q(base + COMMON_TSS_OFF + HOOK_IST_OFF, &live_ist))
        return 3;

    if (!root || (root & 0xfff) ||
        !attempt_receipt_valid(stage, hook, arm, complete,
                               saved_lo, saved_hi, live_lo, live_hi,
                               saved_ist, live_ist))
        return 4;

    int title_51 = arm_file_matches("PPSA01651", arm, base, root);
    int title_52 = arm_file_matches("PPSA01652", arm, base, root);
    if (title_51 == title_52)
        return 5;

    char path[128];
    snprintf(path, sizeof(path), "/data/bpr-postwake-probe-%016" PRIx64 ".log",
             arm);
    FILE *file = fopen(path, "w");
    if (!file)
        return 6;
    int written = fprintf(file,
        "BPR_POSTWAKE_PROBE ok=1 title=%s fw=13600007 "
        "boot=%jd.%06ld arm=%016" PRIx64 " complete=%016" PRIx64 " "
        "base=%016" PRIx64 " root=%016" PRIx64 " "
        "stage=%016" PRIx64 " hook=%016" PRIx64 " gate=1 ist=1\n",
        title_51 ? "PPSA01651" : "PPSA01652",
        (intmax_t)boot.tv_sec, (long)boot.tv_usec,
        arm, complete, base, root, stage, hook);
    int flush_rc = fflush(file);
    int close_rc = fclose(file);
    return written > 0 && !flush_rc && !close_rc ? 0 : 7;
}
