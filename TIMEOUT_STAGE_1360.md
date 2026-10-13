# 13.60 timeout-stage diagnostic

The 13 October clean-route run entered Rest Mode, woke, and reported that its
resume hook fired. The recovered console log ended with `AP completion timed
out; hook restored`. It did not record which of the two pending stages was last
observed. There is no evidence of AP completion or higher hypervisor access.

`dist/byepervisor_reborn_1360_timeout_stage_unverified.elf` changes only the
final timeout notification, after `uninstall_hook()` returns success and file
logging resumes. It now appends the last polled stage value. The armed hook,
AP payload, timing, and Rest Mode path are unchanged from the no-hot-log build.
The last value distinguishes `STAGE_HOOK_FIRED` (`0x484f4f4b5f464952`) from
`STAGE_AP_ENTERED` (`0x41505f454e544552`) at timeout. It cannot prove that the
AP never entered, because a transient stage could have been missed by polling.

This is an offline-built diagnostic, not a demonstrated fix. Its exact SHA-256
is in `dist/SHA256SUMS`. It has not been launched on a PS5. A further run
requires a person at the console to observe Rest Mode and recovery, followed
by retrieval and preservation of the appended stage log. Stop after any freeze,
shutdown, missing Rest Mode transition, or hook-cleanup failure; do not resend
on that boot.
