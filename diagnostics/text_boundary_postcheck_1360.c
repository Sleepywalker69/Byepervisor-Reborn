#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <ps5/kernel.h>

#define PMAP_STORE_OFF 0x3B48FF8
#define DMPML4I_OFF 0x3B49294
#define DMPDPI_OFF 0x3B49298
#define PHYS_MASK UINT64_C(0x000ffffffffff000)

static int rq(uint64_t addr, uint64_t *out)
{
    *out = 0;
    return kernel_copyout(addr, out, sizeof(*out));
}

int main(void)
{
    uint32_t fw = kernel_get_fw_version();
    uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
    uint32_t pml4i = 0, pdpi = 0;
    uint64_t root = 0;
    struct timeval boot = {0};
    size_t boot_len = sizeof(boot);
    if (fw != 0x13600007U ||
        kernel_copyout(base + DMPML4I_OFF, &pml4i, sizeof(pml4i)) ||
        kernel_copyout(base + DMPDPI_OFF, &pdpi, sizeof(pdpi)) ||
        rq(base + PMAP_STORE_OFF + 0x28, &root) || !root ||
        sysctlbyname("kern.boottime", &boot, &boot_len, NULL, 0) ||
        boot_len != sizeof(boot)) {
        puts("POSTCHECK_ABORT metadata read failed");
        return 1;
    }
    uint64_t dmap = UINT64_C(0xffff800000000000) |
                    ((uint64_t)pml4i << 39) | ((uint64_t)pdpi << 30);
    printf("POSTCHECK_BEGIN fw=0x%08x text=0x%016" PRIx64
           " root=0x%016" PRIx64 " boot_sec=%jd boot_usec=%ld\n",
           fw, base, root, (intmax_t)boot.tv_sec, (long)boot.tv_usec);
    const uint32_t offsets[2] = {0, 0xbc0000};
    for (unsigned index = 0; index < 2; ++index) {
        uint64_t va = base + offsets[index];
        uint64_t pml4e = 0, pdpe = 0, pde = 0, pte = 0;
        uint64_t slot = dmap + root + (((va >> 39) & 0x1ff) * 8);
        int r1 = rq(slot, &pml4e);
        if (r1 || !(pml4e & 1)) {
            printf("POSTCHECK_ABORT pml4 index=%u rc=%d\n", index, r1);
            return 1;
        }
        slot = dmap + (pml4e & PHYS_MASK) + (((va >> 30) & 0x1ff) * 8);
        int r2 = rq(slot, &pdpe);
        if (r2 || !(pdpe & 1)) {
            printf("POSTCHECK_ABORT pdp index=%u rc=%d\n", index, r2);
            return 1;
        }
        slot = dmap + (pdpe & PHYS_MASK) + (((va >> 21) & 0x1ff) * 8);
        int r3 = rq(slot, &pde);
        if (r3 || !(pde & 1) || (pde & 0x80)) {
            printf("POSTCHECK_ABORT pde index=%u rc=%d value=0x%016" PRIx64 "\n",
                   index, r3, pde);
            return 1;
        }
        uint64_t pde_slot = slot;
        slot = dmap + (pde & PHYS_MASK) + (((va >> 12) & 0x1ff) * 8);
        int r4 = rq(slot, &pte);
        printf("POSTCHECK_LEAF index=%u off=0x%06x pde_slot=0x%016" PRIx64
               " pde=0x%016" PRIx64 " pte_slot=0x%016" PRIx64
               " pte=0x%016" PRIx64 " pte_rc=%d\n",
               index, offsets[index], pde_slot, pde, slot, pte, r4);
        if (r4)
            return 1;
    }
    puts("POSTCHECK_END");
    return 0;
}
