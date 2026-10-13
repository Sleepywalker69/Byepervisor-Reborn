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
#define TEXT_DATA_DELTA 0xCC0000

static int rq(uint64_t addr, uint64_t *out)
{
    *out = 0;
    return kernel_copyout(addr, out, sizeof(*out));
}

int main(void)
{
    uint32_t fw = kernel_get_fw_version();
    if (fw != 0x13600007U) {
        printf("SURVEY_ABORT fw=0x%08x\n", fw);
        return 2;
    }
    uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
    uint32_t pml4i = 0, pdpi = 0;
    uint64_t root = 0;
    if (kernel_copyout(base + DMPML4I_OFF, &pml4i, sizeof(pml4i)) ||
        kernel_copyout(base + DMPDPI_OFF, &pdpi, sizeof(pdpi)) ||
        rq(base + PMAP_STORE_OFF + 0x28, &root) || !root) {
        puts("SURVEY_ABORT pmap read failed");
        return 1;
    }
    struct timeval boot = {0};
    size_t boot_len = sizeof(boot);
    int boot_rc = sysctlbyname("kern.boottime", &boot, &boot_len, NULL, 0);
    if (boot_rc || boot_len != sizeof(boot)) {
        printf("SURVEY_ABORT boot read rc=%d len=%zu\n", boot_rc, boot_len);
        return 1;
    }
    uint64_t dmap = UINT64_C(0xffff800000000000) |
                    ((uint64_t)pml4i << 39) | ((uint64_t)pdpi << 30);
    printf("SURVEY_BEGIN fw=0x%08x text=0x%016" PRIx64
           " data_delta=0x%x root=0x%016" PRIx64
           " boot_sec=%jd boot_usec=%ld\n",
           fw, base, TEXT_DATA_DELTA, root,
           (intmax_t)boot.tv_sec, (long)boot.tv_usec);
    for (unsigned off = 0; off < TEXT_DATA_DELTA; off += 0x200000) {
        uint64_t va = base + off;
        uint64_t pml4e = 0, pdpe = 0, pde = 0;
        uint64_t pml4_slot = dmap + root + (((va >> 39) & 0x1ff) * 8);
        if (rq(pml4_slot, &pml4e) || !(pml4e & 1)) {
            printf("SURVEY_HOLE off=0x%06x level=pml4\n", off);
            break;
        }
        uint64_t pdp_slot = dmap + (pml4e & PHYS_MASK) + (((va >> 30) & 0x1ff) * 8);
        if (rq(pdp_slot, &pdpe) || !(pdpe & 1) || (pdpe & 0x80)) {
            printf("SURVEY_HOLE off=0x%06x level=pdp\n", off);
            break;
        }
        uint64_t pde_slot = dmap + (pdpe & PHYS_MASK) + (((va >> 21) & 0x1ff) * 8);
        if (rq(pde_slot, &pde) || !(pde & 1)) {
            printf("SURVEY_HOLE off=0x%06x level=pd\n", off);
            break;
        }
        printf("SURVEY_LEAF off=0x%06x slot=0x%016" PRIx64
               " entry=0x%016" PRIx64 " ps=%u rw=%u xotext=%u nx=%u\n",
               off, pde_slot, pde, !!(pde & 0x80), !!(pde & 2),
               !!(pde & (UINT64_C(1) << 58)), !!(pde & (UINT64_C(1) << 63)));
    }
    puts("SURVEY_END");
    return 0;
}
