#include <arpa/inet.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <ps5/kernel.h>

/* Single-client, bounded 13.60 memory capture. Host validates every frame. */
#define PORT 9003
#define MAX_CHUNK 65536U
#define MAX_DATA_BYTES UINT64_C(0x8000000)
#define PHYS_MASK UINT64_C(0x000ffffffffff000)
#define DATA_CAVE_OFF 0x2D39000
#define CAVE_STATE_OFF 0x600
#define CAVE_SAVE_OFF 0x620
#define KH_OFF_USER 0x440
#define PMAP_STORE_OFF 0x3B48FF8
#define DMPML4I_OFF 0x3B49294
#define DMPDPI_OFF 0x3B49298
#define IDT_ARRAY_OFF 0x3BB4300
#define COMMON_TSS_OFF 0x3BB6FE0
#define HOOK_IST_OFF 0x4c
#define STAGE_AP_COMPLETE UINT64_C(0x41505f434f4d504c)

enum { SEG_TEXT = 1, SEG_DATA = 2 };
enum { STATUS_OK = 0, STATUS_BAD_REQUEST = 1, STATUS_MAP = 2,
       STATUS_READ = 3, STATUS_RESTORE = 4, STATUS_STAGE = 5 };

struct __attribute__((packed)) info_frame {
    char magic[8];
    uint32_t version, firmware;
    uint64_t boot_sec, boot_usec, text_base, data_base;
    uint64_t text_length, data_length, root, stage;
};
struct __attribute__((packed)) request_frame {
    char magic[4];
    uint32_t segment;
    uint64_t offset;
    uint32_t length;
};
struct __attribute__((packed)) response_frame {
    char magic[4];
    uint32_t status;
    uint64_t offset;
    uint32_t length, crc32;
    uint64_t detail;
};
_Static_assert(sizeof(struct info_frame) == 80, "info frame size");
_Static_assert(sizeof(struct request_frame) == 20, "request frame size");
_Static_assert(sizeof(struct response_frame) == 32, "response frame size");

static uint8_t chunk[MAX_CHUNK];
static uint64_t g_root, g_dmap, g_base, g_data, g_data_end;
static uint64_t g_boot_sec, g_boot_usec;

static int rq(uint64_t addr, uint64_t *out)
{
    *out = 0;
    return kernel_copyout(addr, out, sizeof(*out));
}

static int stage_ok(void)
{
    uint64_t stage = 0, hook = 0;
    return !rq(g_base + DATA_CAVE_OFF + KH_OFF_USER, &stage) &&
           !rq(g_base + DATA_CAVE_OFF + CAVE_STATE_OFF, &hook) &&
           stage == STAGE_AP_COMPLETE && hook == 0;
}

/* Returns a present leaf, its slot, and its naturally aligned span. */
static int leaf(uint64_t va, uint64_t *slot_out,
                uint64_t *entry_out, uint64_t *span_out)
{
    uint64_t entry = 0;
    uint64_t slot = g_dmap + g_root + (((va >> 39) & 0x1ff) * 8);
    if (rq(slot, &entry) || !(entry & 1) || (entry & 0x80))
        return 1;
    slot = g_dmap + (entry & PHYS_MASK) + (((va >> 30) & 0x1ff) * 8);
    if (rq(slot, &entry) || !(entry & 1))
        return 2;
    if (entry & 0x80) {
        *slot_out = slot; *entry_out = entry; *span_out = UINT64_C(0x40000000);
        return 0;
    }
    slot = g_dmap + (entry & PHYS_MASK) + (((va >> 21) & 0x1ff) * 8);
    if (rq(slot, &entry) || !(entry & 1))
        return 3;
    if (entry & 0x80) {
        *slot_out = slot; *entry_out = entry; *span_out = UINT64_C(0x200000);
        return 0;
    }
    slot = g_dmap + (entry & PHYS_MASK) + (((va >> 12) & 0x1ff) * 8);
    if (rq(slot, &entry) || !(entry & 1))
        return 4;
    *slot_out = slot; *entry_out = entry; *span_out = UINT64_C(0x1000);
    return 0;
}

static int restore_leaf(uint64_t slot, uint64_t original)
{
    uint64_t actual = 0;
    int w = kernel_copyin(&original, slot, sizeof(original));
    int r = rq(slot, &actual);
    return w || r || actual != original;
}

static uint32_t checksum(const uint8_t *buf, uint32_t len)
{
    uint32_t crc = UINT32_C(0xffffffff);
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= buf[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & -(crc & 1));
    }
    return ~crc;
}

static int socket_read_all(int fd, void *buf, size_t len)
{
    uint8_t *p = buf;
    while (len) {
        ssize_t n = recv(fd, p, len, 0);
        if (n <= 0) return 1;
        p += n; len -= (size_t)n;
    }
    return 0;
}

static int socket_write_all(int fd, const void *buf, size_t len)
{
    const uint8_t *p = buf;
    while (len) {
        ssize_t n = send(fd, p, len, 0);
        if (n <= 0) return 1;
        p += n; len -= (size_t)n;
    }
    return 0;
}

static int preflight(void)
{
    if (kernel_get_fw_version() != 0x13600007U)
        return 1;
    struct timeval boot = {0};
    size_t boot_len = sizeof(boot);
    if (sysctlbyname("kern.boottime", &boot, &boot_len, NULL, 0) ||
        boot_len != sizeof(boot) || boot.tv_sec <= 0)
        return 2;
    g_boot_sec = (uint64_t)boot.tv_sec;
    g_boot_usec = (uint64_t)boot.tv_usec;
    g_base = KERNEL_ADDRESS_TEXT_BASE;
    g_data = KERNEL_ADDRESS_DATA_BASE;
    if (!g_base || g_data <= g_base || g_data - g_base > MAX_DATA_BYTES ||
        (g_base & 0xfff) || (g_data & 0xfff))
        return 3;
    uint64_t saved_lo = 0, saved_hi = 0, saved_ist = 0;
    uint64_t live_lo = 0, live_hi = 0, live_ist = 0;
    uint32_t pml4i = 0, pdpi = 0;
    if (!stage_ok() ||
        rq(g_base + DATA_CAVE_OFF + CAVE_SAVE_OFF, &saved_lo) ||
        rq(g_base + DATA_CAVE_OFF + CAVE_SAVE_OFF + 8, &saved_hi) ||
        rq(g_base + DATA_CAVE_OFF + CAVE_SAVE_OFF + 16, &saved_ist) ||
        rq(g_base + IDT_ARRAY_OFF + 16, &live_lo) ||
        rq(g_base + IDT_ARRAY_OFF + 24, &live_hi) ||
        rq(g_base + COMMON_TSS_OFF + HOOK_IST_OFF, &live_ist) ||
        saved_lo != live_lo || saved_hi != live_hi || saved_ist != live_ist ||
        kernel_copyout(g_base + DMPML4I_OFF, &pml4i, sizeof(pml4i)) ||
        kernel_copyout(g_base + DMPDPI_OFF, &pdpi, sizeof(pdpi)) ||
        rq(g_base + PMAP_STORE_OFF + 0x28, &g_root) || !g_root)
        return 4;
    g_dmap = UINT64_C(0xffff800000000000) |
             ((uint64_t)pml4i << 39) | ((uint64_t)pdpi << 30);
    for (uint64_t va = g_base; va < g_data;) {
        uint64_t slot = 0, entry = 0, span = 0;
        if (leaf(va, &slot, &entry, &span) || !(entry & (UINT64_C(1) << 58)) ||
            (entry & 2) || span > UINT64_C(0x200000))
            return 5;
        uint64_t step = span - (va & (span - 1));
        va += step < g_data - va ? step : g_data - va;
    }
    uint64_t cap = g_data + MAX_DATA_BYTES;
    for (uint64_t va = g_data; va < cap;) {
        uint64_t slot = 0, entry = 0, span = 0;
        if (leaf(va, &slot, &entry, &span)) {
            if (va == g_data) return 6;
            g_data_end = va;
            return 0;
        }
        if (entry & (UINT64_C(1) << 58)) return 7;
        uint64_t step = span - (va & (span - 1));
        va += step < cap - va ? step : cap - va;
    }
    return 8; /* No observed end before the cap; refuse a "full" claim. */
}

static uint32_t capture(uint32_t segment, uint64_t offset,
                        uint32_t length, uint64_t *detail)
{
    uint64_t first = (segment == SEG_TEXT ? g_base : g_data) + offset;
    uint32_t done = 0;
    while (done < length) {
        uint64_t va = first + done, slot = 0, entry = 0, span = 0;
        int walk_rc = leaf(va, &slot, &entry, &span);
        if (walk_rc || span > UINT64_C(0x200000)) {
            *detail = va;
            return STATUS_MAP;
        }
        uint64_t room = span - (va & (span - 1));
        uint32_t take = room < length - done ? (uint32_t)room : length - done;
        if (segment == SEG_TEXT) {
            if (!(entry & (UINT64_C(1) << 58)) || (entry & 2)) {
                *detail = va; return STATUS_MAP;
            }
            uint64_t modified = entry & ~(UINT64_C(1) << 58);
            uint64_t actual = 0;
            int w = kernel_copyin(&modified, slot, sizeof(modified));
            int r = rq(slot, &actual);
            if (w || r || actual != modified) {
                *detail = va;
                return restore_leaf(slot, entry) ? STATUS_RESTORE : STATUS_MAP;
            }
            int copy_rc = 0;
            for (uint32_t i = 0; i < take; i += 256) {
                uint32_t n = take - i < 256 ? take - i : 256;
                if (kernel_copyout(va + i, chunk + done + i, n)) {
                    copy_rc = 1; break;
                }
            }
            int restore_rc = restore_leaf(slot, entry);
            if (restore_rc || copy_rc) {
                *detail = va;
                return restore_rc ? STATUS_RESTORE : STATUS_READ;
            }
        } else {
            if (entry & (UINT64_C(1) << 58)) {
                *detail = va; return STATUS_MAP;
            }
            for (uint32_t i = 0; i < take; i += 4096) {
                uint32_t n = take - i < 4096 ? take - i : 4096;
                if (kernel_copyout(va + i, chunk + done + i, n)) {
                    *detail = va + i; return STATUS_READ;
                }
            }
        }
        done += take;
    }
    return STATUS_OK;
}

int main(void)
{
    int rc = preflight();
    if (rc) {
        printf("DUMP_ABORT preflight=%d\n", rc);
        return rc;
    }
    printf("DUMP_PREFLIGHT boot=%" PRIu64 ".%06" PRIu64
           " text=0x%016" PRIx64 " data=0x%016" PRIx64
           " data_end=0x%016" PRIx64 "\n",
           g_boot_sec, g_boot_usec, g_base, g_data, g_data_end);
    fflush(stdout);
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) return 10;
    int one = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) ||
        listen(server, 1)) {
        close(server); return 11;
    }
    puts("DUMP_READY port=9003");
    fflush(stdout);
    int client = accept(server, NULL, NULL);
    if (client < 0) { close(server); return 12; }
    struct timeval timeout = { .tv_sec = 120, .tv_usec = 0 };
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    struct info_frame info = {0};
    memcpy(info.magic, "K136DMP1", 8);
    info.version = 1;
    info.firmware = 0x13600007U;
    info.boot_sec = g_boot_sec;
    info.boot_usec = g_boot_usec;
    info.text_base = g_base;
    info.data_base = g_data;
    info.text_length = g_data - g_base;
    info.data_length = g_data_end - g_data;
    info.root = g_root;
    info.stage = STAGE_AP_COMPLETE;
    if (socket_write_all(client, &info, sizeof(info))) goto out;
    for (;;) {
        struct request_frame request = {0};
        if (socket_read_all(client, &request, sizeof(request))) break;
        if (!memcmp(request.magic, "QUIT", 4)) break;
        struct response_frame response = {0};
        memcpy(response.magic, "RES1", 4);
        response.offset = request.offset;
        uint64_t size = request.segment == SEG_TEXT ? info.text_length : info.data_length;
        if (memcmp(request.magic, "REQ1", 4) ||
            (request.segment != SEG_TEXT && request.segment != SEG_DATA) ||
            request.length == 0 || request.length > MAX_CHUNK ||
            (request.offset & 0xfff) || (request.length & 0xfff) ||
            request.offset > size || request.length > size - request.offset) {
            response.status = STATUS_BAD_REQUEST;
        } else if (!stage_ok()) {
            response.status = STATUS_STAGE;
        } else {
            uint64_t detail = 0;
            response.status = capture(request.segment, request.offset,
                                      request.length, &detail);
            response.detail = detail;
            if (response.status == STATUS_OK) {
                response.length = request.length;
                response.crc32 = checksum(chunk, request.length);
            }
        }
        if (socket_write_all(client, &response, sizeof(response))) break;
        if (response.status != STATUS_OK) break;
        if (socket_write_all(client, chunk, response.length)) break;
    }
out:
    close(client);
    close(server);
    puts("DUMP_EXIT");
    return 0;
}
