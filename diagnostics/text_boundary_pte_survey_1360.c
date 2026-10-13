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
#define FIRST_PDE UINT64_C(0x0000000021897003)
#define LAST_PDE UINT64_C(0x000000002189d023)
#define FIRST_PAGES 448
#define LAST_PAGES 256

static uint64_t entries[FIRST_PAGES + LAST_PAGES];

static int rq(uint64_t addr, uint64_t *out)
{
    *out = 0;
    return kernel_copyout(addr, out, sizeof(*out));
}

static int read_pde(uint64_t dmap, uint64_t root, uint64_t va,
                    uint64_t expected, uint64_t *out)
{
    uint64_t pml4e, pdpe, pde;
    uint64_t slot = dmap + root + (((va >> 39) & 0x1ff) * 8);
    if (rq(slot, &pml4e) || !(pml4e & 1) || (pml4e & 0x80))
        return 1;
    slot = dmap + (pml4e & PHYS_MASK) + (((va >> 30) & 0x1ff) * 8);
    if (rq(slot, &pdpe) || !(pdpe & 1) || (pdpe & 0x80))
        return 2;
    slot = dmap + (pdpe & PHYS_MASK) + (((va >> 21) & 0x1ff) * 8);
    /* The CPU can set the parent PDE's accessed bit while reading a page. */
    if (rq(slot, &pde) || ((pde ^ expected) & ~UINT64_C(0x20)))
        return 3;
    *out = pde;
    return 0;
}

int main(void)
{
    uint32_t fw = kernel_get_fw_version();
    if (fw != 0x13600007U) {
        printf("BOUNDARY_ABORT fw=0x%08x\n", fw);
        return 2;
    }
    uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
    uint32_t pml4i = 0, pdpi = 0;
    uint64_t root = 0;
    if (kernel_copyout(base + DMPML4I_OFF, &pml4i, sizeof(pml4i)) ||
        kernel_copyout(base + DMPDPI_OFF, &pdpi, sizeof(pdpi)) ||
        rq(base + PMAP_STORE_OFF + 0x28, &root) || !root) {
        puts("BOUNDARY_ABORT pmap read failed");
        return 1;
    }
    struct timeval boot = {0};
    size_t boot_len = sizeof(boot);
    if (sysctlbyname("kern.boottime", &boot, &boot_len, NULL, 0) ||
        boot_len != sizeof(boot) || boot.tv_sec <= 0) {
        puts("BOUNDARY_ABORT boot read failed");
        return 1;
    }
    uint64_t dmap = UINT64_C(0xffff800000000000) |
                    ((uint64_t)pml4i << 39) | ((uint64_t)pdpi << 30);
    uint64_t first = 0, last = 0;
    int a = read_pde(dmap, root, base, FIRST_PDE, &first);
    int b = read_pde(dmap, root, base + 0xbc0000, LAST_PDE, &last);
    if (a || b || (base & 0x1fffff) != 0x40000) {
        printf("BOUNDARY_ABORT pde guard first_rc=%d last_rc=%d base=0x%016" PRIx64 "\n",
               a, b, base);
        return 1;
    }
    uint64_t first_table = dmap + (first & PHYS_MASK);
    uint64_t last_table = dmap + (last & PHYS_MASK);
    for (unsigned i = 0; i < FIRST_PAGES; ++i) {
        if (rq(first_table + (64 + i) * 8, &entries[i])) {
            printf("BOUNDARY_ABORT first index=%u\n", i);
            return 1;
        }
    }
    for (unsigned i = 0; i < LAST_PAGES; ++i) {
        if (rq(last_table + i * 8, &entries[FIRST_PAGES + i])) {
            printf("BOUNDARY_ABORT last index=%u\n", i);
            return 1;
        }
    }
    printf("BOUNDARY_BEGIN fw=0x%08x text=0x%016" PRIx64
           " root=0x%016" PRIx64 " boot_sec=%jd boot_usec=%ld count=%u\n",
           fw, base, root, (intmax_t)boot.tv_sec, (long)boot.tv_usec,
           FIRST_PAGES + LAST_PAGES);
    for (unsigned i = 0; i < FIRST_PAGES; ++i)
        printf("PTE off=0x%06x value=0x%016" PRIx64 "\n", i * 4096, entries[i]);
    for (unsigned i = 0; i < LAST_PAGES; ++i)
        printf("PTE off=0x%06x value=0x%016" PRIx64 "\n",
               0xbc0000 + i * 4096, entries[FIRST_PAGES + i]);
    printf("BOUNDARY_END count=%u\n", FIRST_PAGES + LAST_PAGES);
    return 0;
}
