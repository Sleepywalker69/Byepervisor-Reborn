#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <ps5/kernel.h>

/* Read-only inventory for bounded text/data capture planning. */
#define PMAP_STORE_OFF 0x3B48FF8
#define DMPML4I_OFF 0x3B49294
#define DMPDPI_OFF 0x3B49298
#define DATA_CAVE_OFF 0x2D39000
#define KH_OFF_USER 0x440
#define PHYS_MASK UINT64_C(0x000ffffffffff000)
#define WINDOW_SIZE UINT64_C(0x200000)
#define MAX_IMAGE_SPAN UINT64_C(0x8000000)

static int rq(uint64_t address, uint64_t *out)
{
    *out = 0;
    return kernel_copyout(address, out, sizeof(*out));
}

int main(void)
{
    if (kernel_get_fw_version() != 0x13600007U) {
        puts("MAP_ABORT firmware");
        return 2;
    }
    struct timeval boot = {0};
    size_t boot_len = sizeof(boot);
    if (sysctlbyname("kern.boottime", &boot, &boot_len, NULL, 0) ||
        boot_len != sizeof(boot) || boot.tv_sec <= 0) {
        puts("MAP_ABORT boot time");
        return 1;
    }
    uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
    uint64_t data = KERNEL_ADDRESS_DATA_BASE;
    uint64_t root = 0, stage = 0;
    uint32_t pml4i = 0, pdpi = 0;
    if (!base || data <= base || data - base > MAX_IMAGE_SPAN ||
        kernel_copyout(base + DMPML4I_OFF, &pml4i, sizeof(pml4i)) ||
        kernel_copyout(base + DMPDPI_OFF, &pdpi, sizeof(pdpi)) ||
        rq(base + PMAP_STORE_OFF + 0x28, &root) || !root ||
        rq(base + DATA_CAVE_OFF + KH_OFF_USER, &stage)) {
        puts("MAP_ABORT base or pmap");
        return 1;
    }
    uint64_t dmap = UINT64_C(0xffff800000000000) |
                    ((uint64_t)pml4i << 39) | ((uint64_t)pdpi << 30);
    uint64_t start = base & ~(WINDOW_SIZE - 1);
    uint64_t end = (base + MAX_IMAGE_SPAN + WINDOW_SIZE - 1) & ~(WINDOW_SIZE - 1);
    printf("MAP_BEGIN boot=%jd.%06ld text=0x%016" PRIx64
           " data=0x%016" PRIx64 " root=0x%016" PRIx64
           " stage=0x%016" PRIx64 " start=0x%016" PRIx64
           " end=0x%016" PRIx64 "\n",
           (intmax_t)boot.tv_sec, (long)boot.tv_usec,
           base, data, root, stage, start, end);
    fflush(stdout);
    unsigned windows = 0, gaps = 0;
    for (uint64_t va = start; va < end; va += WINDOW_SIZE) {
        uint64_t pml4e = 0, pdpe = 0, pde = 0;
        uint64_t slot = dmap + root + (((va >> 39) & 0x1ff) * 8);
        if (rq(slot, &pml4e) || !(pml4e & 1) || (pml4e & 0x80)) {
            printf("MAP_GAP level=4 va=0x%016" PRIx64 "\n", va);
            ++gaps;
            continue;
        }
        slot = dmap + (pml4e & PHYS_MASK) + (((va >> 30) & 0x1ff) * 8);
        if (rq(slot, &pdpe) || !(pdpe & 1)) {
            printf("MAP_GAP level=3 va=0x%016" PRIx64 "\n", va);
            ++gaps;
            continue;
        }
        if (pdpe & 0x80) {
            printf("MAP_1G va=0x%016" PRIx64 " entry=0x%016" PRIx64 "\n",
                   va, pdpe);
            ++windows;
            continue;
        }
        slot = dmap + (pdpe & PHYS_MASK) + (((va >> 21) & 0x1ff) * 8);
        if (rq(slot, &pde) || !(pde & 1)) {
            printf("MAP_GAP level=2 va=0x%016" PRIx64 "\n", va);
            ++gaps;
            continue;
        }
        if (pde & 0x80) {
            printf("MAP_2M va=0x%016" PRIx64 " entry=0x%016" PRIx64 "\n",
                   va, pde);
            ++windows;
            continue;
        }
        unsigned present = 0, xotext = 0, rw = 0;
        int first_missing = -1;
        uint64_t first = 0, last = 0;
        uint64_t table = dmap + (pde & PHYS_MASK);
        for (unsigned i = 0; i < 512; ++i) {
            uint64_t pte = 0;
            if (rq(table + i * 8, &pte)) {
                printf("MAP_ABORT pte_read va=0x%016" PRIx64 " index=%u\n",
                       va, i);
                return 3;
            }
            if (i == 0) first = pte;
            if (i == 511) last = pte;
            if (pte & 1) {
                ++present;
                xotext += !!(pte & (UINT64_C(1) << 58));
                rw += !!(pte & 2);
            } else if (first_missing < 0) {
                first_missing = (int)i;
            }
        }
        printf("MAP_4K va=0x%016" PRIx64 " pde=0x%016" PRIx64
               " present=%u xo=%u rw=%u first_missing=%d"
               " first=0x%016" PRIx64 " last=0x%016" PRIx64 "\n",
               va, pde, present, xotext, rw, first_missing, first, last);
        ++windows;
    }
    printf("MAP_END windows=%u gaps=%u\n", windows, gaps);
    return 0;
}
