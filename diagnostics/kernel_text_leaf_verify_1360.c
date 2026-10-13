#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <ps5/kernel.h>

/* Independent, read-only post-dump check of every distinct text leaf. */
#define PMAP_STORE_OFF 0x3B48FF8
#define DMPML4I_OFF 0x3B49294
#define DMPDPI_OFF 0x3B49298
#define DATA_CAVE_OFF 0x2D39000
#define KH_OFF_USER 0x440
#define CAVE_STATE_OFF 0x600
#define PHYS_MASK UINT64_C(0x000ffffffffff000)
#define STAGE_AP_COMPLETE UINT64_C(0x41505f434f4d504c)

static int rq(uint64_t addr, uint64_t *out)
{
    *out = 0;
    return kernel_copyout(addr, out, sizeof(*out));
}

int main(void)
{
    if (kernel_get_fw_version() != 0x13600007U) {
        puts("LEAF_ABORT firmware");
        return 2;
    }
    struct timeval boot = {0};
    size_t boot_len = sizeof(boot);
    if (sysctlbyname("kern.boottime", &boot, &boot_len, NULL, 0) ||
        boot_len != sizeof(boot)) {
        puts("LEAF_ABORT boot");
        return 1;
    }
    uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
    uint64_t end = KERNEL_ADDRESS_DATA_BASE;
    uint64_t stage = 0, hook = 0, root = 0;
    uint32_t pml4i = 0, pdpi = 0;
    if (end <= base || end - base > 0x8000000 ||
        rq(base + DATA_CAVE_OFF + KH_OFF_USER, &stage) ||
        rq(base + DATA_CAVE_OFF + CAVE_STATE_OFF, &hook) ||
        stage != STAGE_AP_COMPLETE || hook ||
        kernel_copyout(base + DMPML4I_OFF, &pml4i, sizeof(pml4i)) ||
        kernel_copyout(base + DMPDPI_OFF, &pdpi, sizeof(pdpi)) ||
        rq(base + PMAP_STORE_OFF + 0x28, &root) || !root) {
        puts("LEAF_ABORT state or pmap");
        return 1;
    }
    uint64_t dmap = UINT64_C(0xffff800000000000) |
                    ((uint64_t)pml4i << 39) | ((uint64_t)pdpi << 30);
    uint64_t large = 0, small = 0, pages = 0;
    for (uint64_t va = base; va < end;) {
        uint64_t pml4e = 0, pdpe = 0, leaf = 0;
        uint64_t slot = dmap + root + (((va >> 39) & 0x1ff) * 8);
        if (rq(slot, &pml4e) || !(pml4e & 1) || (pml4e & 0x80))
            goto failed;
        slot = dmap + (pml4e & PHYS_MASK) + (((va >> 30) & 0x1ff) * 8);
        if (rq(slot, &pdpe) || !(pdpe & 1) || (pdpe & 0x80))
            goto failed;
        slot = dmap + (pdpe & PHYS_MASK) + (((va >> 21) & 0x1ff) * 8);
        if (rq(slot, &leaf) || !(leaf & 1))
            goto failed;
        uint64_t span = UINT64_C(0x200000);
        if (leaf & 0x80) {
            ++large;
        } else {
            slot = dmap + (leaf & PHYS_MASK) + (((va >> 12) & 0x1ff) * 8);
            if (rq(slot, &leaf) || !(leaf & 1))
                goto failed;
            span = UINT64_C(0x1000);
            ++small;
        }
        if (!(leaf & (UINT64_C(1) << 58)) || (leaf & 2)) {
            printf("LEAF_FAIL va=0x%016" PRIx64 " entry=0x%016" PRIx64 "\n",
                   va, leaf);
            return 3;
        }
        uint64_t step = span - (va & (span - 1));
        if (step > end - va) step = end - va;
        if (step & 0xfff) goto failed;
        pages += step / 4096;
        va += step;
        continue;
failed:
        printf("LEAF_ABORT walk va=0x%016" PRIx64 "\n", va);
        return 4;
    }
    printf("LEAF_OK boot=%jd.%06ld text=0x%016" PRIx64
           " end=0x%016" PRIx64 " pages=%" PRIu64
           " large=%" PRIu64 " small=%" PRIu64 "\n",
           (intmax_t)boot.tv_sec, (long)boot.tv_usec,
           base, end, pages, large, small);
    return 0;
}
