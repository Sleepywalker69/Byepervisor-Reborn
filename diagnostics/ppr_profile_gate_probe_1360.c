#include <stdint.h>
#include <stdio.h>

#define KEKCALL_CHECK 0xffffffff00000027ULL
#define KEKCALL_PPR_CONTROL 0x700000027ULL
#define PPR_MAGIC 0x505052504c41494eULL
#define PPR_VERSION 8ULL
#define SCOPE_PPR_MOUNT 3ULL
#define CONTROL_CHECK 3ULL
#define CONTROL_SCOPE_ENTER 11ULL
#define CONTROL_SCOPE_LEAVE 12ULL

static int64_t kekcall(uint64_t a, uint64_t b, uint64_t c,
                       uint64_t d, uint64_t e, uint64_t f, uint64_t g)
{
    register uint64_t r10 __asm__("r10") = d;
    register uint64_t r8 __asm__("r8") = e;
    register uint64_t r9 __asm__("r9") = f;
    uint64_t ret;
    unsigned char is_error;
    __asm__ __volatile__(
        "syscall"
        : "=a"(ret), "=@ccc"(is_error), "+r"(r10), "+r"(r8), "+r"(r9)
        : "a"(g), "D"(a), "S"(b), "d"(c)
        : "rcx", "r11", "memory");
    return is_error ? -(int64_t)ret : (int64_t)ret;
}

static int64_t control(uint64_t mode, uint64_t arg0, uint64_t arg2)
{
    return kekcall(PPR_MAGIC, mode, arg0, 0, arg2, 0,
                   KEKCALL_PPR_CONTROL);
}

int main(void)
{
    setbuf(stdout, NULL);
    puts("PPR_PROFILE_GATE entered");
    int64_t check = kekcall(0, 0, 0, 0, 0, 0, KEKCALL_CHECK);
    printf("PPR_PROFILE_GATE kstuff_check=%lld\n", (long long)check);
    if (check != 0)
        return 2;

    int64_t entered = control(CONTROL_SCOPE_ENTER,
                              PPR_VERSION, SCOPE_PPR_MOUNT);
    printf("PPR_PROFILE_GATE scope_enter=%lld\n", (long long)entered);
    if (entered != 0)
        return 3;

    /* No staging data is written. A profile-enabled handler reaches the
       ownership check and returns EPERM; a disabled profile returns EINVAL. */
    int64_t profile = control(CONTROL_CHECK, 0, 0);
    int64_t left = control(CONTROL_SCOPE_LEAVE,
                           PPR_VERSION, SCOPE_PPR_MOUNT);
    printf("PPR_PROFILE_GATE check=%lld scope_leave=%lld\n",
           (long long)profile, (long long)left);
    return profile == -1 && left == 0 ? 0 : 4;
}
