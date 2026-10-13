#ifndef BYEPERVISOR_POSTWAKE_LAUNCH_H
#define BYEPERVISOR_POSTWAKE_LAUNCH_H

#include <stdint.h>

/* Called only after AP completion, hook cleanup and completion-ID write. */
int postwake_launch_configured_elf(uint64_t expected_arm, uint64_t expected_base,
                                    uint64_t expected_root);

#endif
