#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../include/attempt_receipt.h"

static int check(uint64_t stage, uint64_t hook, uint64_t arm,
                 uint64_t complete, uint64_t saved_lo, uint64_t saved_hi,
                 uint64_t live_lo, uint64_t live_hi, uint64_t saved_ist,
                 uint64_t live_ist)
{
    return attempt_receipt_valid(stage, hook, arm, complete, saved_lo,
                                 saved_hi, live_lo, live_hi, saved_ist,
                                 live_ist);
}

int main(void)
{
    const uint64_t s = ATTEMPT_STAGE_AP_COMPLETE;
    assert(check(s, 0, 7, 7, 1, 2, 1, 2, 3, 3));
    assert(!check(0, 0, 7, 7, 1, 2, 1, 2, 3, 3));
    assert(!check(s, 1, 7, 7, 1, 2, 1, 2, 3, 3));
    assert(!check(s, 0, 0, 0, 1, 2, 1, 2, 3, 3));
    assert(!check(s, 0, 7, 0, 1, 2, 1, 2, 3, 3));
    assert(!check(s, 0, 7, 8, 1, 2, 1, 2, 3, 3));
    assert(!check(s, 0, 7, 7, 0, 0, 0, 0, 3, 3));
    assert(!check(s, 0, 7, 7, 1, 2, 9, 2, 3, 3));
    assert(!check(s, 0, 7, 7, 1, 2, 1, 9, 3, 3));
    assert(!check(s, 0, 7, 7, 1, 2, 1, 2, 3, 9));
    puts("attempt receipt state checks: 10 passed");
    return 0;
}
