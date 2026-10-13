#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <ps5/kernel.h>

/* A bounded, boot-specific 64 KiB region in one observed 2 MiB text leaf. */
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
#define REGION_OFF 0x2A0000
#define REGION_PAGES 16
#define PAGE_SIZE 4096
#define ANCHOR_PAGE 8
#define SYS_GETUID_IN_ANCHOR 0x0d60
#define PHYS_MASK UINT64_C(0x000ffffffffff000)
#define EXPECTED_PDE UINT64_C(0x040000001dc001a1)
#define STAGE_AP_COMPLETE UINT64_C(0x41505f434f4d504c)

static uint8_t region[REGION_PAGES * PAGE_SIZE];

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

static int restore(uint64_t slot, uint64_t original)
{
    uint64_t readback = 0;
    int w = kernel_copyin(&original, slot, sizeof(original));
    int r = rq(slot, &readback);
    printf("REGION_RESTORE write_rc=%d read_rc=%d value=0x%016" PRIx64 "\n",
           w, r, readback);
    return w || r || readback != original;
}

static int leaf(uint64_t dmap, uint64_t root, uint64_t va,
                uint64_t *slot_out, uint64_t *entry_out)
{
    uint64_t pml4e, pdpe, pde;
    uint64_t slot = dmap + root + (((va >> 39) & 0x1ff) * 8);
    if (rq(slot, &pml4e) || !(pml4e & 1) || (pml4e & 0x80))
        return 1;
    slot = dmap + (pml4e & PHYS_MASK) + (((va >> 30) & 0x1ff) * 8);
    if (rq(slot, &pdpe) || !(pdpe & 1) || (pdpe & 0x80))
        return 2;
    slot = dmap + (pdpe & PHYS_MASK) + (((va >> 21) & 0x1ff) * 8);
    if (rq(slot, &pde) || !(pde & 1) || !(pde & 0x80))
        return 3;
    *slot_out = slot;
    *entry_out = pde;
    return 0;
}

int main(void)
{
    const uint32_t fw = kernel_get_fw_version();
    printf("REGION_PREFLIGHT fw=0x%08x\n", fw);
    if (fw != 0x13600007U)
        return 2;

    struct timeval boot = {0};
    size_t boot_len = sizeof(boot);
    int boot_rc = sysctlbyname("kern.boottime", &boot, &boot_len, NULL, 0);
    printf("REGION_BOOT rc=%d len=%zu sec=%jd usec=%ld\n",
           boot_rc, boot_len, (intmax_t)boot.tv_sec, (long)boot.tv_usec);
    if (boot_rc || boot_len != sizeof(boot) || boot.tv_sec <= 0)
        return 2;

    const uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
    const uint64_t cave = base + DATA_CAVE_OFF;
    const uint64_t first_va = base + REGION_OFF;
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
        puts("REGION_ABORT state or pmap guard failed; no write");
        return 1;
    }

    const uint64_t dmap = UINT64_C(0xffff800000000000) |
                          ((uint64_t)pml4i << 39) | ((uint64_t)pdpi << 30);
    uint64_t pde_slot = 0, pde = 0;
    for (unsigned page = 0; page < REGION_PAGES; ++page) {
        uint64_t slot = 0, entry = 0;
        uint64_t va = first_va + (uint64_t)page * PAGE_SIZE;
        int rc = leaf(dmap, root, va, &slot, &entry);
        if (rc || entry != EXPECTED_PDE || (page && slot != pde_slot)) {
            printf("REGION_ABORT leaf page=%u rc=%d slot=0x%016" PRIx64
                   " entry=0x%016" PRIx64 "; no write\n",
                   page, rc, slot, entry);
            return 1;
        }
        if (!page) {
            pde_slot = slot;
            pde = entry;
        }
    }

    uint64_t modified = pde & ~(UINT64_C(1) << 58);
    uint64_t readback = 0;
    int w = kernel_copyin(&modified, pde_slot, sizeof(modified));
    int r = rq(pde_slot, &readback);
    printf("REGION_CLEAR write_rc=%d read_rc=%d value=0x%016" PRIx64 "\n",
           w, r, readback);
    if (w || r || readback != modified) {
        if (restore(pde_slot, pde))
            puts("REGION_RESTORE_FAILED; do not repeat");
        return 3;
    }

    int read_error = 0;
    for (unsigned offset = 0; offset < sizeof(region); offset += 256) {
        int rc = kernel_copyout(first_va + offset, region + offset, 256);
        if (rc) {
            printf("REGION_READ_FAILED offset=%u rc=%d\n", offset, rc);
            read_error = 1;
            break;
        }
    }
    if (restore(pde_slot, pde)) {
        puts("REGION_RESTORE_FAILED; do not repeat");
        return 4;
    }
    if (read_error)
        return 5;

    const uint8_t known[8] = {0x55, 0x48, 0x89, 0xe5,
                              0x48, 0x8b, 0x87, 0x40};
    if (memcmp(region + ANCHOR_PAGE * PAGE_SIZE + SYS_GETUID_IN_ANCHOR,
               known, sizeof(known))) {
        puts("REGION_ABORT anchor mismatch after restore; no data emitted");
        return 6;
    }

    printf("REGION_BEGIN fw=0x%08x base=0x%016" PRIx64
           " pages=%u bytes=%u root=0x%016" PRIx64
           " pde_slot=0x%016" PRIx64 " boot_sec=%jd boot_usec=%ld\n",
           fw, first_va, REGION_PAGES, (unsigned)sizeof(region), root,
           pde_slot, (intmax_t)boot.tv_sec, (long)boot.tv_usec);
    for (unsigned page = 0; page < REGION_PAGES; ++page) {
        printf("PAGE %02u va=0x%016" PRIx64 "\n", page,
               first_va + (uint64_t)page * PAGE_SIZE);
        for (unsigned off = 0; off < PAGE_SIZE; off += 64) {
            char hex[129];
            static const char digits[] = "0123456789abcdef";
            for (unsigned i = 0; i < 64; ++i) {
                uint8_t byte = region[page * PAGE_SIZE + off + i];
                hex[2 * i] = digits[byte >> 4];
                hex[2 * i + 1] = digits[byte & 15];
            }
            hex[128] = 0;
            printf("DATA %04x %s\n", off, hex);
        }
    }
    printf("REGION_END pages=%u bytes=%u\n", REGION_PAGES,
           (unsigned)sizeof(region));
    return 0;
}
