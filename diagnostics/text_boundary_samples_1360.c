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
#define PAGE_SIZE 4096
#define COUNT 2

static const struct {
    uint32_t off;
    uint64_t pde;
    uint64_t pte;
} targets[COUNT] = {
    {0x000000, UINT64_C(0x0000000021897003), UINT64_C(0x040000001da40101)},
    {0xbc0000, UINT64_C(0x000000002189d023), UINT64_C(0x040000001e600121)},
};
static uint8_t samples[COUNT][PAGE_SIZE];

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
    printf("EDGE_RESTORE index=%u write_rc=%d read_rc=%d value=0x%016" PRIx64 "\n",
           index, w, r, readback);
    return w || r || readback != original;
}

int main(void)
{
    uint32_t fw = kernel_get_fw_version();
    if (fw != 0x13600007U) {
        printf("EDGE_ABORT fw=0x%08x\n", fw);
        return 2;
    }
    struct timeval boot = {0};
    size_t boot_len = sizeof(boot);
    if (sysctlbyname("kern.boottime", &boot, &boot_len, NULL, 0) ||
        boot_len != sizeof(boot) || boot.tv_sec <= 0) {
        puts("EDGE_ABORT boot read failed");
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
        puts("EDGE_ABORT state or pmap guard failed; no write");
        return 1;
    }
    uint64_t dmap = UINT64_C(0xffff800000000000) |
                    ((uint64_t)pml4i << 39) | ((uint64_t)pdpi << 30);
    uint64_t slots[COUNT];
    for (unsigned index = 0; index < COUNT; ++index) {
        uint64_t va = base + targets[index].off;
        uint64_t pml4e, pdpe, pde = 0, pte = 0;
        uint64_t slot = dmap + root + (((va >> 39) & 0x1ff) * 8);
        if (rq(slot, &pml4e) || !(pml4e & 1) || (pml4e & 0x80))
            goto guard_fail;
        slot = dmap + (pml4e & PHYS_MASK) + (((va >> 30) & 0x1ff) * 8);
        if (rq(slot, &pdpe) || !(pdpe & 1) || (pdpe & 0x80))
            goto guard_fail;
        slot = dmap + (pdpe & PHYS_MASK) + (((va >> 21) & 0x1ff) * 8);
        if (rq(slot, &pde) || pde != targets[index].pde)
            goto guard_fail;
        slot = dmap + (pde & PHYS_MASK) + (((va >> 12) & 0x1ff) * 8);
        if (rq(slot, &pte) || pte != targets[index].pte)
            goto guard_fail;
        slots[index] = slot;
        continue;
guard_fail:
        printf("EDGE_ABORT leaf index=%u pde=0x%016" PRIx64
               " pte=0x%016" PRIx64 "; no write\n", index, pde, pte);
        return 1;
    }

    for (unsigned index = 0; index < COUNT; ++index) {
        uint64_t original = targets[index].pte;
        uint64_t current = 0;
        if (rq(slots[index], &current) || current != original) {
            printf("EDGE_ABORT changed pte index=%u value=0x%016" PRIx64
                   "; no further write\n", index, current);
            return 1;
        }
        uint64_t modified = original & ~(UINT64_C(1) << 58);
        uint64_t readback = 0;
        int w = kernel_copyin(&modified, slots[index], sizeof(modified));
        int r = rq(slots[index], &readback);
        printf("EDGE_CLEAR index=%u write_rc=%d read_rc=%d value=0x%016" PRIx64 "\n",
               index, w, r, readback);
        if (w || r || readback != modified) {
            if (restore(index, slots[index], original))
                puts("EDGE_RESTORE_FAILED; do not repeat");
            return 3;
        }
        int read_error = 0;
        for (unsigned off = 0; off < PAGE_SIZE; off += 256) {
            int rc = kernel_copyout(base + targets[index].off + off,
                                    samples[index] + off, 256);
            if (rc) {
                printf("EDGE_READ_FAILED index=%u off=%u rc=%d\n", index, off, rc);
                read_error = 1;
                break;
            }
        }
        if (restore(index, slots[index], original)) {
            puts("EDGE_RESTORE_FAILED; do not repeat");
            return 4;
        }
        if (read_error)
            return 5;
    }

    printf("EDGE_BEGIN fw=0x%08x text=0x%016" PRIx64
           " root=0x%016" PRIx64 " boot_sec=%jd boot_usec=%ld count=%u\n",
           fw, base, root, (intmax_t)boot.tv_sec, (long)boot.tv_usec, COUNT);
    static const char digits[] = "0123456789abcdef";
    for (unsigned index = 0; index < COUNT; ++index) {
        printf("PAGE index=%u off=0x%06x pte=0x%016" PRIx64 "\n",
               index, targets[index].off, targets[index].pte);
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
    printf("EDGE_END count=%u bytes=%u\n", COUNT, COUNT * PAGE_SIZE);
    return 0;
}
