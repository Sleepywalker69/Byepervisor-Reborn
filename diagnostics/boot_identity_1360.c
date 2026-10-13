#include <inttypes.h>
#include <stdio.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <ps5/kernel.h>

/* Userland-only boot identity; no kernel memory reads or writes. */
int main(void)
{
    struct timeval boot = {0};
    size_t size = sizeof(boot);
    const unsigned fw = kernel_get_fw_version();
    if (fw != 0x13600007U ||
        sysctlbyname("kern.boottime", &boot, &size, NULL, 0) ||
        size != sizeof(boot) || boot.tv_sec <= 0 ||
        boot.tv_usec < 0 || boot.tv_usec >= 1000000) {
        puts("BPR_BOOT_V1 ok=0");
        return 1;
    }
    printf("BPR_BOOT_V1 ok=1 fw=%08x boot=%jd.%06ld\n",
           fw, (intmax_t)boot.tv_sec, (long)boot.tv_usec);
    return 0;
}
