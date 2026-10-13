#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <ps5/kernel.h>

#define PMAP_STORE_OFF 0x3B48FF8
#define DMPML4I_OFF 0x3B49294
#define DMPDPI_OFF 0x3B49298
#define SYS_GETUID_TEXT_OFF 0x2A8D60
#define PHYS_MASK UINT64_C(0x000ffffffffff000)
#define PDE_2M_PHYS_MASK UINT64_C(0x000fffffffe00000)

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

static int entry(uint64_t table, unsigned index, uint64_t *out)
{
    return rq(table + (uint64_t)index * 8, out) || !(*out & 1);
}

int main(void)
{
    uint32_t fw = kernel_get_fw_version();
    printf("text-alias-probe: fw=0x%08x\n", fw);
    if (fw != 0x13600007U)
        return 2;

    const uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
    const uint64_t va = base + SYS_GETUID_TEXT_OFF;
    uint32_t pml4i, pdpi;
    uint64_t root, pml4e, pdpe, pde;
    if (ru32(base + DMPML4I_OFF, &pml4i) ||
        ru32(base + DMPDPI_OFF, &pdpi) ||
        rq(base + PMAP_STORE_OFF + 0x28, &root) || !root) {
        puts("text-alias-probe: root read failed");
        return 1;
    }
    uint64_t dmap = UINT64_C(0xffff800000000000) |
                    ((uint64_t)pml4i << 39) | ((uint64_t)pdpi << 30);
    if (entry(dmap + root, (va >> 39) & 0x1ff, &pml4e) ||
        entry(dmap + (pml4e & PHYS_MASK), (va >> 30) & 0x1ff, &pdpe) ||
        (pdpe & 0x80) ||
        entry(dmap + (pdpe & PHYS_MASK), (va >> 21) & 0x1ff, &pde) ||
        !(pde & 0x80)) {
        puts("text-alias-probe: page walk failed or layout changed");
        return 1;
    }
    uint64_t phys = (pde & PDE_2M_PHYS_MASK) | (va & 0x1fffff);
    uint64_t alias = dmap + phys;
    printf("text-alias-probe: pde=0x%016" PRIx64 " xotext=%u phys=0x%09" PRIx64 "\n",
           pde, (unsigned)((pde >> 58) & 1), phys);

    uint8_t bytes[8] = {0};
    int rc = kernel_copyout(alias, bytes, sizeof(bytes));
    printf("text-alias-probe: alias_read rc=%d bytes=", rc);
    for (unsigned i = 0; i < sizeof(bytes); ++i)
        printf("%02x", bytes[i]);
    putchar('\n');
    puts("text-alias-probe: complete (read only)");
    return rc ? 3 : 0;
}
