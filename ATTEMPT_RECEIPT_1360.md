# Attempt-specific post-wake receipt candidate

This branch adds an arm ID to the 13.60 Byepervisor userland ELF. The ID is
derived from the current time and PID. The payload refuses to arm unless the
two reserved cave qwords are zero, writes the arm ID with readback, and prints
`BPR_ARM_V1 id=...` only after the resume hook installs. After AP completion,
it writes the same ID to a completion qword only if hook cleanup succeeds.
The separate read-only `diagnostics/attempt_status_1360.c` checks firmware,
boot time, AP stage, zero hook state, saved/live gate and IST, and equality of
the nonzero arm and completion IDs. It emits `BPR_STATUS_V2`.

The receipt qwords are at cave offsets `0x640` and `0x648`, beyond khook's
`0x500`-byte scratch and below the AP payload at `0x1000`. Both were zero in
the saved post-AP mapped-data capture from boot `1791893784.399222`. This is
one boot's observation, so the live ELF checks for zero before writing.

The hook/receipt candidate remains **offline only**. A separate status ELF was
sent once on the older AP-completed boot: it returned `ok=0`, with zero arm and
completion IDs despite the AP stage marker. That is the expected stale-attempt
rejection, not a successful new attempt. The arm line, post-wake completion
receipt, and survival of this candidate have not yet been exercised. The
earlier successful ELF remains on
the `kernel-text-capture-1360` branch. Do not treat a `BPR_STATUS_V1` line or
an old AP marker alone as proof that this attempt completed. The Y2JB branch
has parser tests but is not wired into `main.js`; no autoload package has been
created or installed from this candidate.

Audit: `BPR_ARM_V1` follows a zero-check and readback of both receipt qwords,
successful `kh_install`, saved original gate/IST, `kh_trigger`, and the armed
hook-state write. The completion qword is written only in the AP-complete
branch after `uninstall_hook` returns success. That function disarms, restores
and reads back the gate/IST, then clears hook state. The independent reader
requires AP-complete, zero hook state, matching nonzero IDs, matching gate/IST,
firmware 13.60, and a valid boot timestamp. The host compares that timestamp
and ID with values recorded before Rest Mode. A failed install may leave the
arm qword nonzero, deliberately preventing a same-boot retry; it cannot produce
a passing completion receipt.

Offline checks: `gcc -std=c11 -Wall -Wextra -Werror
tests/test_attempt_receipt.c` passed 10 state cases, including missing,
mismatched, and failed completion. Y2JB's Node parser passed 23 cases, including
stale boot and malformed output. SDK v0.44 rebuilt the hook ELF (182,792 bytes,
SHA-256 `936694d1a75141f427b88073c7e25b46e45b4b50791cac36fad6febc8d468a6e`)
and revised status ELF (111,016 bytes, SHA-256
`b8ff06a4e812cfaf6308d088faca103bd759f71a06bd2236c5266a9227aaa6c1`).
The SDK root is `../sdk-v0.44/ps5-payload-sdk` from this checkout. An initial
attempt using `/opt/ps5-payload-sdk` failed because that path does not exist
in this WSL setup; the source build then passed with the local SDK path.
The existing hook mock also passed four install, rollback, trigger-failure,
and restore cases against the current `khook.c`.

The controlled-run harness is `diagnostics/run_attempt_receipt_1360.py`. It
requires a fresh log/receipt path, hashes the exact input ELF, timestamps and
saves the sole arm ID as soon as it arrives, and keeps the socket open across
Rest Mode. A separate `diagnostics/boot_identity_1360.elf` reads only firmware
and `kern.boottime` from userland before the launch. Its SDK build passed
`-Wall -Wextra -Werror`; 110,944 bytes, SHA-256
`cd902582695ee28ec28f2e565cbbf5401596b6489756e17cbc2926656cdd4d2d`.
No console run used this ELF yet.
