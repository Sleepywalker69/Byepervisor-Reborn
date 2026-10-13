# Unmodified upstream 13.60 baseline

**Live result, 13 October 2026:** One direct elfldr launch reached the host
`Hook installed, enter sleep and resume!` line. A user screenshot shows both
`Detected regular model (0x840f60)` and `Enter sleep and resume!` notifications.
The user reported that the PS5 appeared frozen before Rest Mode and then
powered itself off. A subsequent bounded port-9021 connection timed out.
No S3 transition, AP result, VMCB
readback, or higher-access check was observed. Do not relaunch this ELF; the
sequence below records the attempted run.

This separate ELF was built from the exact `PS5Dev/Byepervisor-Reborn` upstream
commit [`ea04f7132b837087f552c57261cb800481a3a910`](https://github.com/PS5Dev/Byepervisor-Reborn/commit/ea04f7132b837087f552c57261cb800481a3a910).
No upstream source edits were made. The build-generated `break_hv.bin` is
untracked in the detached build checkout; `git diff --exit-code` found no
tracked source changes.

- Artifact: `dist/byepervisor_reborn_upstream_ea04f71_1360.elf`
- Length: 177,608 bytes
- SHA-256: `b1010ca68cc57690983346a029af1d1b43cf5e942f301af844a7f6e569b4d9a4`
- Toolchain: local PS5 Payload SDK v0.44, `make`
- Format: x86-64 PIE ELF, three `PT_LOAD` segments

The binary is a comparison against the edited fork's two pre-Rest-Mode
freezes. Upstream's claim of 13.60 support does not establish that this exact
build will work on this console. This version changes kernel text mappings
before arming its hook. After a successful resume it reads kernel text, and
its original code contains an unconditional write to `ktext(8)` on its
nonzero-cookie path. Its success notification is not proof of an independently
verified AP-completion or VMCB readback.

## Supervised one-run sequence (attempted)

1. Start the usual 13.60 kernel exploit and a LAN-accessible elfldr on port
   9021. Keep a person at the PS5 screen.
2. Verify the exact ELF against `dist/SHA256SUMS`; send it **once** through
   elfldr. Preserve the host stream.
3. If the host reports `Hook installed, enter sleep and resume!` and the PS5
   remains responsive, select Rest Mode manually. Wait for steady amber for
   about one minute, then wake it once.
4. Record the last screen notice and host line, whether the light reached
   steady amber, and whether the PS5 returned to the home screen.
5. If it freezes, powers off, or needs storage repair, stop. Never resend this
   ELF on the same boot: the upstream state path can make a second invocation
   behave differently.

The host's hook-installed print confirms only that the installer returned to
the caller. It does not confirm the following notification, S3 transition,
AP execution, or persistent hypervisor access.

The preserved host stream is `byepervisor-upstream-ea04f71-launch-host.log`,
338 bytes, SHA-256
`dae6205ae8e2f6d0a9312a3f09f0df920243d5b727f64c786c028d884f5ee29a`.
The screen image establishes that the notification displayed; neither it nor
the host stream identifies the exact instruction where the console stopped.
