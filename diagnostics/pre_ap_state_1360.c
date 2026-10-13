#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <ps5/kernel.h>

/* Read-only preflight for a fresh boot before the HV stage is launched. */
#define DATA_CAVE_OFF 0x2D39000
#define CAVE_STATE_OFF 0x600
#define KH_OFF_USER 0x440
#define PMAP_STORE_OFF 0x3B48FF8
#define DMPML4I_OFF 0x3B49294
#define DMPDPI_OFF 0x3B49298
#define IDT_ARRAY_OFF 0x3BB4300
#define COMMON_TSS_OFF 0x3BB6FE0
#define HOOK_IST_OFF 0x4c
#define PHYS_MASK UINT64_C(0x000ffffffffff000)
#define SYS_GETUID_OFF 0x2a8d60

static int rq(uint64_t address, uint64_t *out)
{
    *out = 0;
    return kernel_copyout(address, out, sizeof(*out));
}

int main(void)
{
    uint32_t fw = kernel_get_fw_version();
    if (fw != 0x13600007U) {
        printf("PREAP_ABORT fw=0x%08x\n", fw);
        return 2;
    }
    struct timeval boot = {0};
    size_t boot_len = sizeof(boot);
    if (sysctlbyname("kern.boottime", &boot, &boot_len, NULL, 0) ||
        boot_len != sizeof(boot) || boot.tv_sec <= 0) {
        puts("PREAP_ABORT boot time read failed");
        return 1;
    }
    uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
    uint64_t cave = base + DATA_CAVE_OFF;
    uint64_t stage = 0, hook = 0, gate_lo = 0, gate_hi = 0, ist = 0;
    uint64_t root = 0;
    uint32_t pml4i = 0, pdpi = 0;
    int s = rq(cave + KH_OFF_USER, &stage);
    int h = rq(cave + CAVE_STATE_OFF, &hook);
    int g1 = rq(base + IDT_ARRAY_OFF + 16, &gate_lo);
    int g2 = rq(base + IDT_ARRAY_OFF + 24, &gate_hi);
    int t = rq(base + COMMON_TSS_OFF + HOOK_IST_OFF, &ist);
    int m1 = kernel_copyout(base + DMPML4I_OFF, &pml4i, sizeof(pml4i));
    int m2 = kernel_copyout(base + DMPDPI_OFF, &pdpi, sizeof(pdpi));
    int m3 = rq(base + PMAP_STORE_OFF + 0x28, &root);
    printf("PREAP_STATE fw=0x%08x text=0x%016" PRIx64
           " boot_sec=%jd boot_usec=%ld stage_rc=%d stage=0x%016" PRIx64
           " hook_rc=%d hook=0x%016" PRIx64
           " gate_rc=%d,%d gate=0x%016" PRIx64 ",0x%016" PRIx64
           " ist_rc=%d ist=0x%016" PRIx64
           " pmap_rc=%d,%d,%d root=0x%016" PRIx64 "\n",
           fw, base, (intmax_t)boot.tv_sec, (long)boot.tv_usec,
           s, stage, h, hook, g1, g2, gate_lo, gate_hi, t, ist,
           m1, m2, m3, root);
    if (m1 || m2 || m3 || !root)
        return 1;

    uint64_t va = base + SYS_GETUID_OFF;
    uint64_t dmap = UINT64_C(0xffff800000000000) |
                    ((uint64_t)pml4i << 39) | ((uint64_t)pdpi << 30);
    uint64_t pml4e = 0, pdpe = 0, pde = 0;
    uint64_t slot = dmap + root + (((va >> 39) & 0x1ff) * 8);
    int r1 = rq(slot, &pml4e);
    if (r1 || !(pml4e & 1)) {
        printf("PREAP_ABORT pml4 rc=%d value=0x%016" PRIx64 "\n", r1, pml4e);
        return 1;
    }
    slot = dmap + (pml4e & PHYS_MASK) + (((va >> 30) & 0x1ff) * 8);
    int r2 = rq(slot, &pdpe);
    if (r2 || !(pdpe & 1) || (pdpe & 0x80)) {
        printf("PREAP_ABORT pdp rc=%d value=0x%016" PRIx64 "\n", r2, pdpe);
        return 1;
    }
    slot = dmap + (pdpe & PHYS_MASK) + (((va >> 21) & 0x1ff) * 8);
    int r3 = rq(slot, &pde);
    printf("PREAP_PDE va=0x%016" PRIx64 " slot=0x%016" PRIx64
           " read_rc=%d value=0x%016" PRIx64
           " present=%u ps=%u rw=%u xotext=%u\n",
           va, slot, r3, pde, !!(pde & 1), !!(pde & 0x80),
           !!(pde & 2), !!(pde & (UINT64_C(1) << 58)));
    return r3 != 0;
}
