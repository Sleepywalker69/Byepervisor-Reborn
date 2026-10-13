#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "y2jb_arm_file.h"

static int matches(const char *body, const char *title, uint64_t arm)
{
    FILE *file = tmpfile();
    assert(file);
    assert(fwrite(body, 1, strlen(body), file) == strlen(body));
    rewind(file);
    int result = y2jb_arm_file_matches(file, title, arm,
                                       UINT64_C(0xffffffff98880000),
                                       UINT64_C(0x1c6d4000));
    fclose(file);
    return result;
}

int main(void)
{
    const char *arm =
        "BPR Y2JB V3 receipt test\n"
        "Title: PPSA01651\n"
        "arm: 6ace4524b8022059 base=ffffffff98880000 "
        "root=000000001c6d4000\n";
    assert(matches(arm, "PPSA01651", UINT64_C(0x6ace4524b8022059)));
    char appended[256];
    snprintf(appended, sizeof appended, "%spost-wake JS: resumed\n"
             "status: passed boot=1791902982.554552\n", arm);
    assert(matches(appended, "PPSA01651",
                   UINT64_C(0x6ace4524b8022059)));
    assert(!matches(arm, "PPSA01652", UINT64_C(0x6ace4524b8022059)));
    assert(!matches(arm, "PPSA01651", UINT64_C(0x6ace4524b8022058)));
    assert(!matches("BPR Y2JB V3 receipt test\nTitle: PPSA01651\n",
                    "PPSA01651", UINT64_C(0x6ace4524b8022059)));
    assert(!matches("BPR Y2JB V3 receipt test\n"
                    "Title: PPSA01651\n"
                    "arm: 6ace4524b8022059 base=ffffffff98880000 "
                    "root=000000001c6d5000\n",
                    "PPSA01651", UINT64_C(0x6ace4524b8022059)));
    return 0;
}
