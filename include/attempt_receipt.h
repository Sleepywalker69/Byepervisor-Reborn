#ifndef BYEPERVISOR_ATTEMPT_RECEIPT_H
#define BYEPERVISOR_ATTEMPT_RECEIPT_H

#include <stdint.h>

/* Reserved qwords in the 13.60 data cave, beyond khook's 0x500-byte scratch. */
#define CAVE_ARM_ID_OFF      0x640
#define CAVE_COMPLETE_ID_OFF 0x648

/* Opt-in 13.60 call probe. Four qwords beyond the receipt, kept as evidence. */
#define CAVE_CALL_SRC_OFF   0x660
#define CAVE_CALL_DST_OFF   0x668
#define CAVE_CALL_RET_OFF   0x670
#define CAVE_CALL_DONE_OFF  0x678
#define CALL_SRC_SENTINEL   UINT64_C(0xc0dec0de1360a53a)
#define CALL_DONE_SENTINEL  UINT64_C(0x43414c4c5f444f4e)

#define ATTEMPT_STAGE_AP_COMPLETE UINT64_C(0x41505f434f4d504c)

static inline int attempt_receipt_valid(uint64_t stage, uint64_t hook,
                                        uint64_t arm, uint64_t complete,
                                        uint64_t saved_gate_lo,
                                        uint64_t saved_gate_hi,
                                        uint64_t live_gate_lo,
                                        uint64_t live_gate_hi,
                                        uint64_t saved_ist,
                                        uint64_t live_ist)
{
    return stage == ATTEMPT_STAGE_AP_COMPLETE && hook == 0 &&
           arm != 0 && arm == complete &&
           (saved_gate_lo != 0 || saved_gate_hi != 0) &&
           saved_gate_lo == live_gate_lo &&
           saved_gate_hi == live_gate_hi && saved_ist == live_ist;
}

#endif
