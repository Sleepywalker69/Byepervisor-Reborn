#ifndef BYEPERVISOR_Y2JB_ARM_FILE_H
#define BYEPERVISOR_Y2JB_ARM_FILE_H

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Y2JB may append post-wake status after these three lines. The arm prefix
   remains the same attempt-specific authority used by postwake_launch.c. */
static inline int y2jb_arm_file_matches(FILE *file, const char *title,
                                        uint64_t arm, uint64_t base,
                                        uint64_t root)
{
    char line[256], heading[64], expected[128];
    if (!file || !title)
        return 0;
    if (snprintf(heading, sizeof heading, "Title: %s\n", title) <= 0 ||
        snprintf(expected, sizeof expected,
                 "arm: %016" PRIx64 " base=%016" PRIx64 " root=%016" PRIx64 "\n",
                 arm, base, root) <= 0)
        return 0;
    return fgets(line, sizeof line, file) &&
           strcmp(line, "BPR Y2JB V3 receipt test\n") == 0 &&
           fgets(line, sizeof line, file) && strcmp(line, heading) == 0 &&
           fgets(line, sizeof line, file) && strcmp(line, expected) == 0;
}

#endif
