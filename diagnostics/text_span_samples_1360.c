#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <ps5/kernel.h>

#define DATA_CAVE_OFF 0x2D39000
#define PMAP_STORE_OFF 0x3B48FF8
#define DMPML4I_OFF 0x3B49294
#define DMPDPI_OFF 0x3B49298
#define IDT_ARRAY_OFF 0x3BB4300
#define COMMON_TSS_OFF 0x3BB6FE0
#define CAVE_STATE_OFF 0x600
#define CAVE_SAVE_OFF 0x620
#define KH_OFF_USER 0x440
#define HOOK_IST_OFF 0x4c
#define PHYS_MASK UINT64_C(0x000ffffffffff000)
#define STAGE_AP_COMPLETE UINT64_C(0x41505f434f4d504c)
#define SAMPLE_COUNT 4
#define PAGE_SIZE 4096

static const struct {
    uint32_t off;
    uint64_t pde;
} targets[SAMPLE_COUNT] = {
    {0x400000, UINT64_C(0x040000001de001a1)},
    {0x600000, UINT64_C(0x040000001e0001a1)},
    {0x800000, UINT64_C(0x040000001e2001a1)},
    {0xa00000, UINT64_C(0x040000001e4001a1)},
};

static uint8_t samples[SAMPLE_COUNT][PAGE_SIZE];

static int rq(uint64_t addr, uint64_t *out)
{
    *out = 0;
    return kernel_copyout(addr, out, sizeof(*out));
}

static int ru32(uint64_t addr, uint32_t *out)
{
    *out = 0;
    return kernel_copyout(addr, out, sizeof(*out));
}

static int restore(unsigned index, uint64_t slot, uint64_t original)
{
    uint64_t readback = 0;
    int w = kernel_copyin(&original, slot, sizeof(original));
    int r = rq(slot, &readback);
    printf("SAMPLE_RESTORE index=%u write_rc=%d read_rc=%d value=0x%016" PRIx64 "\n",
           index, w, r, readback);
    return w || r || readback != original;
}

int main(void)
{
    uint32_t fw = kernel_get_fw_version();
    if (fw != 0x13600007U) {
        printf("SAMPLE_ABORT fw=0x%08x\n", fw);
        return 2;
    }
    struct timeval boot = {0};
    size_t boot_len = sizeof(boot);
    int boot_rc = sysctlbyname("kern.boottime", &boot, &boot_len, NULL, 0);
    if (boot_rc || boot_len != sizeof(boot) || boot.tv_sec <= 0) {
        printf("SAMPLE_ABORT boot rc=%d len=%zu\n", boot_rc, boot_len);
        return 2;
    }

    uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
    uint64_t cave = base + DATA_CAVE_OFF;
    uint64_t stage, hook_state, saved_lo, saved_hi, saved_ist;
    uint64_t live_lo, live_hi, live_ist, root;
    uint32_t pml4i, pdpi;
    if (rq(cave + KH_OFF_USER, &stage) ||
        rq(cave + CAVE_STATE_OFF, &hook_state) ||
        rq(cave + CAVE_SAVE_OFF, &saved_lo) ||
        rq(cave + CAVE_SAVE_OFF + 8, &saved_hi) ||
        rq(cave + CAVE_SAVE_OFF + 16, &saved_ist) ||
        rq(base + IDT_ARRAY_OFF + 16, &live_lo) ||
        rq(base + IDT_ARRAY_OFF + 24, &live_hi) ||
        rq(base + COMMON_TSS_OFF + HOOK_IST_OFF, &live_ist) ||
        stage != STAGE_AP_COMPLETE || hook_state != 0 ||
        saved_lo != live_lo || saved_hi != live_hi ||
        saved_ist != live_ist ||
        ru32(base + DMPML4I_OFF, &pml4i) ||
        ru32(base + DMPDPI_OFF, &pdpi) ||
        rq(base + PMAP_STORE_OFF + 0x28, &root) || !root) {
        puts("SAMPLE_ABORT state or pmap guard failed; no write");
        return 1;
    }

    uint64_t dmap = UINT64_C(0xffff800000000000) |
                    ((uint64_t)pml4i << 39) | ((uint64_t)pdpi << 30);
    uint64_t slots[SAMPLE_COUNT];
    for (unsigned index = 0; index < SAMPLE_COUNT; ++index) {
        uint64_t va = base + targets[index].off;
        uint64_t pml4e, pdpe, pde = 0;
        uint64_t slot = dmap + root + (((va >> 39) & 0x1ff) * 8);
        if (rq(slot, &pml4e) || !(pml4e & 1) || (pml4e & 0x80)) {
            printf("SAMPLE_ABORT pml4 index=%u; no write\n", index);
            return 1;
        }
        slot = dmap + (pml4e & PHYS_MASK) + (((va >> 30) & 0x1ff) * 8);
        if (rq(slot, &pdpe) || !(pdpe & 1) || (pdpe & 0x80)) {
            printf("SAMPLE_ABORT pdp index=%u; no write\n", index);
            return 1;
        }
        slot = dmap + (pdpe & PHYS_MASK) + (((va >> 21) & 0x1ff) * 8);
        if (rq(slot, &pde) || pde != targets[index].pde) {
            printf("SAMPLE_ABORT pde index=%u value=0x%016" PRIx64
                   "; no write\n", index, pde);
            return 1;
        }
        slots[index] = slot;
    }

    for (unsigned index = 0; index < SAMPLE_COUNT; ++index) {
        uint64_t current = 0;
        if (rq(slots[index], &current) || current != targets[index].pde) {
            printf("SAMPLE_ABORT changed pde index=%u value=0x%016" PRIx64
                   "; no further write\n", index, current);
            return 1;
        }
        uint64_t modified = targets[index].pde & ~(UINT64_C(1) << 58);
        uint64_t readback = 0;
        int w = kernel_copyin(&modified, slots[index], sizeof(modified));
        int r = rq(slots[index], &readback);
        printf("SAMPLE_CLEAR index=%u write_rc=%d read_rc=%d value=0x%016" PRIx64 "\n",
               index, w, r, readback);
        if (w || r || readback != modified) {
            if (restore(index, slots[index], targets[index].pde))
                puts("SAMPLE_RESTORE_FAILED; do not repeat");
            return 3;
        }
        int read_error = 0;
        for (unsigned off = 0; off < PAGE_SIZE; off += 256) {
            int rc = kernel_copyout(base + targets[index].off + off,
                                    samples[index] + off, 256);
            if (rc) {
                printf("SAMPLE_READ_FAILED index=%u off=%u rc=%d\n", index, off, rc);
                read_error = 1;
                break;
            }
        }
        if (restore(index, slots[index], targets[index].pde)) {
            puts("SAMPLE_RESTORE_FAILED; do not repeat");
            return 4;
        }
        if (read_error)
            return 5;
    }

    printf("SAMPLE_BEGIN fw=0x%08x text=0x%016" PRIx64
           " root=0x%016" PRIx64 " boot_sec=%jd boot_usec=%ld count=%u\n",
           fw, base, root, (intmax_t)boot.tv_sec, (long)boot.tv_usec,
           SAMPLE_COUNT);
    static const char digits[] = "0123456789abcdef";
    for (unsigned index = 0; index < SAMPLE_COUNT; ++index) {
        printf("PAGE index=%u off=0x%06x pde=0x%016" PRIx64 "\n",
               index, targets[index].off, targets[index].pde);
        for (unsigned off = 0; off < PAGE_SIZE; off += 64) {
            char hex[129];
            for (unsigned i = 0; i < 64; ++i) {
                uint8_t byte = samples[index][off + i];
                hex[i * 2] = digits[byte >> 4];
                hex[i * 2 + 1] = digits[byte & 15];
            }
            hex[128] = 0;
            printf("DATA %04x %s\n", off, hex);
        }
    }
    printf("SAMPLE_END count=%u bytes=%u\n", SAMPLE_COUNT,
           SAMPLE_COUNT * PAGE_SIZE);
    return 0;
}
