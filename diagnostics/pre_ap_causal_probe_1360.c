#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <ps5/kernel.h>

/* Compile against a fresh read-only preflight. Defaults are unarmed. */
#ifndef EXPECTED_BOOT_SEC
#define EXPECTED_BOOT_SEC 0
#endif
#ifndef EXPECTED_BOOT_USEC
#define EXPECTED_BOOT_USEC 0
#endif
#ifndef EXPECTED_TEXT_BASE
#define EXPECTED_TEXT_BASE 0
#endif
#ifndef EXPECTED_PDE
#define EXPECTED_PDE 0
#endif

#define DATA_CAVE_OFF 0x2D39000
#define CAVE_STATE_OFF 0x600
#define KH_OFF_USER 0x440
#define PMAP_STORE_OFF 0x3B48FF8
#define DMPML4I_OFF 0x3B49294
#define DMPDPI_OFF 0x3B49298
#define PHYS_MASK UINT64_C(0x000ffffffffff000)
#define SYS_GETUID_OFF 0x2a8d60
#define STAGE_AP_COMPLETE UINT64_C(0x41505f434f4d504c)

static int rq(uint64_t address, uint64_t *out)
{
    *out = 0;
    return kernel_copyout(address, out, sizeof(*out));
}

int main(void)
{
    if (!EXPECTED_BOOT_SEC || !EXPECTED_TEXT_BASE || !EXPECTED_PDE) {
        puts("BASELINE_UNARMED; no write");
        return 2;
    }
    uint32_t fw = kernel_get_fw_version();
    if (fw != 0x13600007U) {
        printf("BASELINE_ABORT fw=0x%08x; no write\n", fw);
        return 2;
    }
    struct timeval boot = {0};
    size_t boot_len = sizeof(boot);
    if (sysctlbyname("kern.boottime", &boot, &boot_len, NULL, 0) ||
        boot_len != sizeof(boot) ||
        boot.tv_sec != EXPECTED_BOOT_SEC ||
        boot.tv_usec != EXPECTED_BOOT_USEC) {
        printf("BASELINE_ABORT boot mismatch sec=%jd usec=%ld; no write\n",
               (intmax_t)boot.tv_sec, (long)boot.tv_usec);
        return 1;
    }
    uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
    if (base != EXPECTED_TEXT_BASE) {
        printf("BASELINE_ABORT text base=0x%016" PRIx64 "; no write\n", base);
        return 1;
    }
    uint64_t stage = 0, hook = 0;
    if (rq(base + DATA_CAVE_OFF + KH_OFF_USER, &stage) ||
        rq(base + DATA_CAVE_OFF + CAVE_STATE_OFF, &hook) ||
        stage != 0 || hook != 0 || stage == STAGE_AP_COMPLETE) {
        printf("BASELINE_ABORT stage=0x%016" PRIx64
               " hook=0x%016" PRIx64 "; no write\n", stage, hook);
        return 1;
    }
    uint32_t pml4i = 0, pdpi = 0;
    uint64_t root = 0;
    if (kernel_copyout(base + DMPML4I_OFF, &pml4i, sizeof(pml4i)) ||
        kernel_copyout(base + DMPDPI_OFF, &pdpi, sizeof(pdpi)) ||
        rq(base + PMAP_STORE_OFF + 0x28, &root) || !root) {
        puts("BASELINE_ABORT pmap read failed; no write");
        return 1;
    }
    uint64_t dmap = UINT64_C(0xffff800000000000) |
                    ((uint64_t)pml4i << 39) | ((uint64_t)pdpi << 30);
    uint64_t va = base + SYS_GETUID_OFF;
    uint64_t pml4e = 0, pdpe = 0, pde = 0;
    uint64_t slot = dmap + root + (((va >> 39) & 0x1ff) * 8);
    if (rq(slot, &pml4e) || !(pml4e & 1) || (pml4e & 0x80)) {
        puts("BASELINE_ABORT pml4 guard; no write");
        return 1;
    }
    slot = dmap + (pml4e & PHYS_MASK) + (((va >> 30) & 0x1ff) * 8);
    if (rq(slot, &pdpe) || !(pdpe & 1) || (pdpe & 0x80)) {
        puts("BASELINE_ABORT pdp guard; no write");
        return 1;
    }
    slot = dmap + (pdpe & PHYS_MASK) + (((va >> 21) & 0x1ff) * 8);
    if (rq(slot, &pde) || pde != EXPECTED_PDE ||
        !(pde & 1) || !(pde & 0x80) || !(pde & (UINT64_C(1) << 58)) ||
        (pde & 2)) {
        printf("BASELINE_ABORT pde=0x%016" PRIx64 "; no write\n", pde);
        return 1;
    }
    uint64_t modified = pde & ~(UINT64_C(1) << 58);
    uint64_t readback = 0;
    int w = kernel_copyin(&modified, slot, sizeof(modified));
    int r = rq(slot, &readback);
    printf("BASELINE_CLEAR write_rc=%d read_rc=%d value=0x%016" PRIx64 "\n",
           w, r, readback);
    if (w || r || readback != modified) {
        int rw = kernel_copyin(&pde, slot, sizeof(pde));
        int rr = rq(slot, &readback);
        printf("BASELINE_RESTORE write_rc=%d read_rc=%d value=0x%016" PRIx64 "\n",
               rw, rr, readback);
        return 3;
    }
    uint8_t bytes[8] = {0};
    int text_rc = kernel_copyout(va, bytes, sizeof(bytes));
    int restore_w = kernel_copyin(&pde, slot, sizeof(pde));
    int restore_r = rq(slot, &readback);
    printf("BASELINE_RESTORE write_rc=%d read_rc=%d value=0x%016" PRIx64 "\n",
           restore_w, restore_r, readback);
    if (restore_w || restore_r || readback != pde) {
        puts("BASELINE_RESTORE_FAILED; do not repeat");
        return 4;
    }
    printf("BASELINE_READ rc=%d bytes=", text_rc);
    for (unsigned i = 0; i < sizeof(bytes); ++i)
        printf("%02x", bytes[i]);
    putchar('\n');
    return text_rc ? 5 : 0;
}
