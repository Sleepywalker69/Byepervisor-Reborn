#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <ps5/kernel.h>

#define PMAP_STORE_OFF 0x3B48FF8
#define DMPML4I_OFF 0x3B49294
#define DMPDPI_OFF 0x3B49298
#define SYS_GETUID_TEXT_OFF 0x2A8D60
#define PHYS_MASK UINT64_C(0x000ffffffffff000)

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

static int step(uint64_t addr, unsigned index, const char *name,
                uint64_t *entry)
{
    uint64_t slot = addr + (uint64_t)index * 8;
    if (rq(slot, entry) != 0) {
        printf("%s: read failed at 0x%016" PRIx64 "\n", name, slot);
        return 1;
    }
    printf("%s: entry=0x%016" PRIx64 " present=%u rw=%u xotext=%u ps=%u\n",
           name, *entry, (unsigned)(*entry & 1),
           (unsigned)((*entry >> 1) & 1),
           (unsigned)((*entry >> 58) & 1),
           (unsigned)((*entry >> 7) & 1));
    return (*entry & 1) ? 0 : 1;
}

int main(void)
{
    uint32_t fw = kernel_get_fw_version();
    printf("text-pte-probe: fw=0x%08x\n", fw);
    if (fw != 0x13600007U)
        return 2;

    const uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
    const uint64_t va = base + SYS_GETUID_TEXT_OFF;
    uint32_t pml4i, pdpi;
    uint64_t pml4, pml4e, pdpe, pde, pte;
    if (ru32(base + DMPML4I_OFF, &pml4i) ||
        ru32(base + DMPDPI_OFF, &pdpi) ||
        rq(base + PMAP_STORE_OFF + 0x28, &pml4) || !pml4) {
        puts("text-pte-probe: root read failed");
        return 1;
    }
    uint64_t dmap = UINT64_C(0xffff800000000000) |
                    ((uint64_t)pml4i << 39) | ((uint64_t)pdpi << 30);
    printf("text-pte-probe: dmap indexes=%u,%u root=0x%016" PRIx64
           " va=0x%016" PRIx64 "\n", pml4i, pdpi, pml4, va);
    if (step(dmap + pml4, (va >> 39) & 0x1ff, "pml4e", &pml4e)) return 1;
    if (step(dmap + (pml4e & PHYS_MASK), (va >> 30) & 0x1ff,
             "pdpe", &pdpe)) return 1;
    if (pdpe & 0x80) { puts("text-pte-probe: leaf=pdpe"); return 0; }
    if (step(dmap + (pdpe & PHYS_MASK), (va >> 21) & 0x1ff,
             "pde", &pde)) return 1;
    if (pde & 0x80) { puts("text-pte-probe: leaf=pde"); return 0; }
    if (step(dmap + (pde & PHYS_MASK), (va >> 12) & 0x1ff,
             "pte", &pte)) return 1;
    puts("text-pte-probe: leaf=pte (read only)");
    return 0;
}
