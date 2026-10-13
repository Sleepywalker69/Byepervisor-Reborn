#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <ps5/kernel.h>

#define DATA_CAVE_OFF 0x2D39000
#define PMAP_STORE_OFF 0x3B48FF8
#define DMPML4I_OFF 0x3B49294
#define DMPDPI_OFF 0x3B49298
#define SYS_GETUID_TEXT_OFF 0x2A8D60
#define KH_OFF_USER 0x440
#define CAVE_STATE_OFF 0x600
#define PHYS_MASK UINT64_C(0x000ffffffffff000)
#define EXPECTED_PDE UINT64_C(0x040000001dc001a1)
#define STAGE_AP_COMPLETE UINT64_C(0x41505f434f4d504c)

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

static int restore(uint64_t slot, uint64_t original)
{
    uint64_t readback = 0;
    int write_rc = kernel_copyin(&original, slot, sizeof(original));
    int read_rc = rq(slot, &readback);
    printf("single-pde-xom-probe: restore write_rc=%d read_rc=%d value=0x%016"
           PRIx64 "\n", write_rc, read_rc, readback);
    return write_rc || read_rc || readback != original;
}

int main(void)
{
    uint32_t fw = kernel_get_fw_version();
    printf("single-pde-xom-probe: fw=0x%08x\n", fw);
    if (fw != 0x13600007U)
        return 2;

    const uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
    const uint64_t va = base + SYS_GETUID_TEXT_OFF;
    uint64_t stage, hook_state, root, pml4e, pdpe, pde;
    uint32_t pml4i, pdpi;
    if (rq(base + DATA_CAVE_OFF + KH_OFF_USER, &stage) ||
        rq(base + DATA_CAVE_OFF + CAVE_STATE_OFF, &hook_state) ||
        stage != STAGE_AP_COMPLETE || hook_state != 0 ||
        ru32(base + DMPML4I_OFF, &pml4i) ||
        ru32(base + DMPDPI_OFF, &pdpi) ||
        rq(base + PMAP_STORE_OFF + 0x28, &root) || !root) {
        puts("single-pde-xom-probe: stage, state, or root guard failed; no write");
        return 1;
    }
    uint64_t dmap = UINT64_C(0xffff800000000000) |
                    ((uint64_t)pml4i << 39) | ((uint64_t)pdpi << 30);
    uint64_t pml4_slot = dmap + root + (((va >> 39) & 0x1ff) * 8);
    if (rq(pml4_slot, &pml4e) || !(pml4e & 1)) {
        puts("single-pde-xom-probe: PML4 guard failed; no write");
        return 1;
    }
    uint64_t pdp_slot = dmap + (pml4e & PHYS_MASK) + (((va >> 30) & 0x1ff) * 8);
    if (rq(pdp_slot, &pdpe) || !(pdpe & 1) || (pdpe & 0x80)) {
        puts("single-pde-xom-probe: PDP guard failed; no write");
        return 1;
    }
    uint64_t pde_slot = dmap + (pdpe & PHYS_MASK) + (((va >> 21) & 0x1ff) * 8);
    if (rq(pde_slot, &pde) || pde != EXPECTED_PDE) {
        printf("single-pde-xom-probe: PDE guard failed value=0x%016" PRIx64
               "; no write\n", pde);
        return 1;
    }

    uint64_t modified = pde & ~(UINT64_C(1) << 58);
    uint64_t readback = 0;
    int write_rc = kernel_copyin(&modified, pde_slot, sizeof(modified));
    int read_rc = rq(pde_slot, &readback);
    printf("single-pde-xom-probe: clear write_rc=%d read_rc=%d value=0x%016"
           PRIx64 "\n", write_rc, read_rc, readback);
    if (write_rc || read_rc || readback != modified) {
        if (restore(pde_slot, pde))
            puts("single-pde-xom-probe: RESTORE FAILED; do not repeat");
        return 3;
    }

    uint8_t bytes[8] = {0};
    int text_rc = kernel_copyout(va, bytes, sizeof(bytes));
    printf("single-pde-xom-probe: text_read rc=%d bytes=", text_rc);
    for (unsigned i = 0; i < sizeof(bytes); ++i)
        printf("%02x", bytes[i]);
    putchar('\n');
    if (restore(pde_slot, pde)) {
        puts("single-pde-xom-probe: RESTORE FAILED; do not repeat");
        return 4;
    }
    puts("single-pde-xom-probe: complete; original PDE restored");
    return text_rc ? 5 : 0;
}
