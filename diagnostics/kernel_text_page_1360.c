#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ps5/kernel.h>

/* One boot-specific 4 KiB capture; the measured 2 MiB PDE is restored. */
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
#define TEXT_PAGE_OFF 0x2A8000
#define SYS_GETUID_IN_PAGE 0x0d60
#define PHYS_MASK UINT64_C(0x000ffffffffff000)
#define EXPECTED_PDE UINT64_C(0x040000001dc001a1)
#define STAGE_AP_COMPLETE UINT64_C(0x41505f434f4d504c)

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
    printf("CAPTURE_RESTORE write_rc=%d read_rc=%d value=0x%016" PRIx64 "\n",
           w, r, readback);
    return w || r || readback != original;
}

int main(void)
{
    const uint32_t fw = kernel_get_fw_version();
    printf("CAPTURE_PREFLIGHT fw=0x%08x\n", fw);
    if (fw != 0x13600007U)
        return 2;

    const uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
    const uint64_t cave = base + DATA_CAVE_OFF;
    const uint64_t va = base + TEXT_PAGE_OFF;
    uint64_t stage, hook_state, saved_lo, saved_hi, saved_ist;
    uint64_t live_lo, live_hi, live_ist, root, pml4e, pdpe, pde = 0;
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
        puts("CAPTURE_ABORT state or pmap guard failed; no write");
        return 1;
    }

    uint64_t dmap = UINT64_C(0xffff800000000000) |
                    ((uint64_t)pml4i << 39) | ((uint64_t)pdpi << 30);
    uint64_t pml4_slot = dmap + root + (((va >> 39) & 0x1ff) * 8);
    if (rq(pml4_slot, &pml4e) || !(pml4e & 1)) {
        puts("CAPTURE_ABORT PML4 guard failed; no write");
        return 1;
    }
    uint64_t pdp_slot = dmap + (pml4e & PHYS_MASK) + (((va >> 30) & 0x1ff) * 8);
    if (rq(pdp_slot, &pdpe) || !(pdpe & 1) || (pdpe & 0x80)) {
        puts("CAPTURE_ABORT PDP guard failed; no write");
        return 1;
    }
    uint64_t pde_slot = dmap + (pdpe & PHYS_MASK) + (((va >> 21) & 0x1ff) * 8);
    if (rq(pde_slot, &pde) || pde != EXPECTED_PDE) {
        printf("CAPTURE_ABORT PDE guard failed value=0x%016" PRIx64
               "; no write\n", pde);
        return 1;
    }

    uint64_t modified = pde & ~(UINT64_C(1) << 58);
    uint64_t readback = 0;
    int w = kernel_copyin(&modified, pde_slot, sizeof(modified));
    int r = rq(pde_slot, &readback);
    printf("CAPTURE_CLEAR write_rc=%d read_rc=%d value=0x%016" PRIx64 "\n",
           w, r, readback);
    if (w || r || readback != modified) {
        if (restore(pde_slot, pde))
            puts("CAPTURE_RESTORE_FAILED; do not repeat");
        return 3;
    }

    uint8_t page[4096];
    int read_error = 0;
    for (unsigned offset = 0; offset < sizeof(page); offset += 256) {
        int rc = kernel_copyout(va + offset, page + offset, 256);
        if (rc) {
            printf("CAPTURE_READ_FAILED offset=%u rc=%d\n", offset, rc);
            read_error = 1;
            break;
        }
    }
    if (restore(pde_slot, pde)) {
        puts("CAPTURE_RESTORE_FAILED; do not repeat");
        return 4;
    }
    if (read_error)
        return 5;

    const uint8_t known[8] = {0x55, 0x48, 0x89, 0xe5,
                              0x48, 0x8b, 0x87, 0x40};
    printf("CAPTURE_BEGIN fw=0x%08x va=0x%016" PRIx64
           " bytes=%u known8_match=%d\n", fw, va, (unsigned)sizeof(page),
           memcmp(page + SYS_GETUID_IN_PAGE, known, sizeof(known)) == 0);
    for (unsigned offset = 0; offset < sizeof(page); offset += 64) {
        printf("DATA %04x ", offset);
        for (unsigned i = 0; i < 64; ++i)
            printf("%02x", page[offset + i]);
        putchar('\n');
    }
    printf("CAPTURE_END bytes=%u\n", (unsigned)sizeof(page));
    return 0;
}
