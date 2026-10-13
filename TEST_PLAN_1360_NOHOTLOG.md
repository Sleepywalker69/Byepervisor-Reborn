# 13.60 no-hot-log diagnostic candidate

**Live result, 13 October 2026:** The single supervised launch reached the
host `Hook installed; automatic Rest Mode in 3 seconds` line. The user reported
the PS5 frozen before Rest Mode. No AP or resume result was obtained. Do not
relaunch this exact ELF; the steps below document the attempted test.

This is one supervised run of `dist/byepervisor_reborn_1360_autorest_nohotlog_unverified.elf`,
SHA-256 `376cafd8ef3549574c339997f67b7b75e315b9b6b8d5ab6dc7a5e8f40deb5e69`.
It is an unverified crash-isolation candidate, not a proven hypervisor bypass.
The preceding logged build froze before displaying its `Armed` notice. The
recovered file contained that notice's complete log line but did not identify
whether `fsync`, notification delivery, or the armed hook stalled.

## Before the run

1. Confirm the PS5 is at the home screen on firmware 13.60 and the normal
   kernel exploit and elfldr are active. Have a person present to watch the
   screen and wake the console if it enters Rest Mode.
2. Preserve the existing `/data/ps5-s3-1360/byepervisor-reborn-1360.log`
   through read-only FTP first; this build appends to it.
3. Compare the local ELF to `dist/SHA256SUMS`. Do not send either earlier ELF.

## One run

1. Send this exact ELF once through elfldr on port 9021 and preserve host output.
2. If it reports an error before `Hook installed`, stop. Do not enter Rest Mode
   manually or resend on that boot.
3. If it reports `Hook installed`, the payload should show one `Armed` notice,
   wait three seconds, and request Rest Mode. Do not select Rest Mode manually.
4. If the light reaches steady amber, wait about one minute and wake the PS5
   once. Record the light, screen, and any hook/AP/cleanup notice.
5. After the console is stable, retrieve the appended stage log once by FTP;
   preserve its size, hash, and exact contents.

## Stop conditions

If the console freezes, shuts down, repairs storage, does not enter Rest Mode,
or reports a hook cleanup failure, stop. Do not resend either ELF on that boot.
No notification alone establishes AP completion or lasting hypervisor access.
