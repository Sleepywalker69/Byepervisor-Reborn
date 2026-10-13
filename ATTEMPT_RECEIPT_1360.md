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

## First live attempt and V3 revision

The first controlled V2 attempt reached arm ID `6ace30b47751d056` before
automatic Rest Mode. A separate post-wake read reported AP-complete, zero hook,
matching completion ID, and restored gate/IST. The strict boot-time comparison
failed: `kern.boottime` changed from `1791897694.667760` to
`1791897726.214153`. No later payload launched. The original host socket reset
during Rest Mode; the separate post-wake status socket worked. This shows that
`kern.boottime` is unsuitable as a fixed Rest Mode identity on this console.
The host runner now records a socket error without losing its arm receipt.

V3 keeps `kern.boottime` for diagnostics and compares the pre-Rest arm ID,
kernel text base, and kernel pmap root with an independent post-wake read.
The arm line is `BPR_ARM_V2`; the reader is `attempt_status_v3_1360.elf` and
emits `BPR_STATUS_V3`. The fresh-boot zero checks and AP/cleanup checks remain.
The kernel-instance values are intended to survive S3; they still require a
new live comparison before the chain can pass. The revised hook ELF is 182,784
bytes, SHA-256 `4e303c1a5d409013637bded728602103ab39f4948d3f5888d7e9a5d98c0ab810`;
the V3 status ELF is 111,024 bytes, SHA-256
`3b17d7e24c44b2341e3a65b133f74f052d95540ba453becd58378aaa2f35e4a5`.
The exact V3 hook ELF is also retained locally as
`diagnostics/byepervisor_reborn_attempt_receipt_v3_1360.elf`; the V2 live ELF
remains as `diagnostics/byepervisor_reborn_attempt_receipt_1360.elf` with its
original hash. Neither versioned binary is staged for Git.
The 10 C state cases, four hook-contract cases, 23 V2 parser cases, and 25 V3
parser cases passed offline. At this point in the record, both new ELFs
remained untested on the console.
`tests/test_run_attempt_receipt.py` also passed a local forced-socket-reset
case: it verified that the arm ID, text base, and pmap root are saved before
the reset and that the final reason is recorded.

## V3 controlled live receipt, 13 October

On fresh standalone Relapse/elfldr boot `1791898356.667496`, the exact V3
hook ELF (SHA-256 `4e303c1a5d409013637bded728602103ab39f4948d3f5888d7e9a5d98c0ab810`)
printed `BPR_ARM_V2 id=6ace3357104ff054 base=ffffffff998a0000
root=000000001d6f4000` after the hook-installed line and before automatic Rest
Mode. The 505-byte local host log SHA-256 is
`ce996af1df551490ff6df861f352c7d1d94c680e1c9ea3df5ab1187e8f5e83cd`.
The runner's 433-byte JSON receipt SHA-256 is
`be60f8afcc3c7896f41532dde4883a17b29c31f2a34cc96c797eb96458d6853d`;
it records the ELF hash, arm values and timestamp, and the S3 socket reset
(`socket_error:10054`).

After the user's wake, the independent 111,024-byte V3 status ELF returned
`ok=1`, firmware `13600007`, boot `1791898416.675424`, the same base/root,
AP-complete stage, zero hook state, the same nonzero arm/completion ID, and
`gate=1 ist=1`. The 203-byte status log SHA-256 is
`6c877ddc29c222394dc0ee7b021cc79dfd5618327e5e0a75b5b026a815fa0779`.
Y2JB's V3 parser accepted the saved arm log, JSON receipt, and independent
status log. The user confirmed the home screen remained responsive. This
passes the one-attempt receipt gate. The changed `kern.boottime` is consistent
with the earlier Rest Mode observation and is informational only.

No post-wake Y2JB chain, YouTube survival check, or downstream kstuff-lite
payload ran in this test. The V3 receipt is necessary for those next gates;
it does not by itself establish that any of them work.
