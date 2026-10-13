# From 13.60 AP completion to a usable post-HV chain

The 13 October 2026 run on one regular PS5 reached `STAGE_AP_COMPLETE` after
the fork's 16-VMCB validation and write/readback loop. A separate read found
the marker and restored IDT gate/IST. A guarded one-PDE change enabled an
eight-byte kernel-text read and was restored. These observations establish a
working research foothold, not a complete 13.60 jailbreak.

## What earlier projects added after HV access

- [Byepervisor for 1.xx–2.xx](https://github.com/PS5Dev/Byepervisor) describes
  two independent routes. Its older jump-table route disables NPT and GMET on
  one core; its preferred QA-flag route changes the HV's page-table setup on
  Rest Mode resume. Both then change guest kernel page-table permissions so
  kernel text can be dumped and patched. The project adds firmware-specific
  patch tables, a kernel execution path, and a 2.50 HEN. Its README warns that
  its unbounded kernel dump can panic at unmapped memory.
- [Byepervisor-Reborn](https://github.com/PS5Dev/Byepervisor-Reborn) targets a
  different S3 AP-start window on 13.xx. Upstream changes text page-table
  permissions and demonstrates a small text read, but ships no HEN patches.
  This fork removed upstream's broad text-mapping loop while isolating crashes.
- [ps5-linux-loader](https://github.com/ps5-linux/ps5-linux-loader) shows a
  substantially larger second-stage route: firmware offsets, S3 handoff,
  kernel and HV shellcode, hardware setup, and a Linux bootloader. Its
  published firmware and hardware support is not evidence that its payload
  works on 13.60.

## Next milestones

1. **Verified text capture — achieved on one boot.** Captured one known 4 KiB
   13.60 text page at `0xffffffffddce8000`: SHA-256
   `8291eeb4759a048f0c556273cfb9c591ff212684c5c9b76316c2f0891ec39599`.
   The host parser verified all 64 ordered records, the known eight bytes at
   `+0xd60`, and the ELF's restore report. A separate read-only page walk
   reproduced the original PDE. The source, ELF, capture, and manifest are in
   `diagnostics/`. This is one page from one console boot, not a kernel ELF.
2. **Full bounded text and mapped-data capture — achieved on one boot.** The first page
   walker validated all sixteen addresses before changing their common
   measured leaf; it stops at a missing or unexpected mapping or a failed
   read. The capture has per-page hashes, firmware, console boot time, pmap
   root, host time, exact overlap with milestone 1, and an independent PDE
   postcheck. The documented raw x86-64 partial image begins at
   `0xffffffffddce0000`, is 65,536 bytes, and has SHA-256
   `488482d4aba95751f86c7b1906709ee2fffefd6bd5ad4beab3fcfa8fb9846685`.
   That 64 KiB result was an intermediate gate. On boot `1791893784.399222`,
   a read-only map inventory established text base `0xffffffffd3b70000`, data
   base `0xffffffffd4830000`, and the first missing data page at
   `0xffffffffd9c00000`. A bounded TCP dumper transferred every page in those
   observed ranges into two continuous **local, non-Git** files: 13,369,344
   text bytes (SHA-256 `20da45a0910c990153fbfb7c94e16356867e0bd84f7c9fbf754a45a5dd129673`)
   and 87,883,776 mapped-data bytes (SHA-256
   `29a0fd3d0ff5290897981543dcc7c71e02105477597b7d4523ded09ffdfdc318`).
   All 204 text and 1,341 data chunks passed independent hash verification;
   the known 64 KiB text overlap matched, and read-only postchecks found all
   3,264 text pages protected again. The user reported a responsive console.
   The data bytes are a non-atomic live range ending at the first gap, not a
   verified ELF `.data` section, and these files are raw memory, not a
   reconstructed kernel ELF. No dump bytes are published to GitHub.
3. **Separate capability checks — post-AP read/write and restoration achieved
   on one boot; causal attribution open.** The full bounded capture read every
   observed text page and the independent leaf survey found all 3,264 pages
   protected afterward. A boot-guarded probe matched the exact 16-byte
   preimage at text offset `0x2a1690`, changed the NOP at `+0x2a1698` from
   `0x90` to `0x66`, read it back, restored `0x90`, verified the full 16-byte
   postimage, and restored/read back the original protected PDE. A separate
   64 KiB capture around that byte matched the earlier capture SHA-256
   `488482d4aba95751f86c7b1906709ee2fffefd6bd5ad4beab3fcfa8fb9846685`;
   a new read-only leaf survey again found all text pages XOTEXT and read-only.
   The user reported the home screen responsive. The failed pre-AP test cannot
   isolate AP causality. At the user's direction, do not repeat pre-HV reads
   on this console merely to resolve that attribution; keep it explicitly
   unproved while proceeding with post-HV work.
4. **13.60 patch map and downstream chain.** Derive firmware-specific patch
   sites from the captured code and compare with existing HEN/kstuff needs.
   Add one checked change at a time, then a completion signal that the
   autoloader can observe after wake. Only after that should Y2JB schedule
   later payloads. Linux requires its own much larger hardware and boot path.
   Initial offline progress is in `PATCH_MAP_1360.md`: the captured syscall
   table and `sys_getuid` jump stub cross-check each other, and several
   kstuff 13.60 trap/gadget offsets match the dump's instructions. The
   `mprotect`/ASLR entries are scoped debugger trap sites, not ready-made
   global patch bytes. No downstream functional patch or post-wake autoloader
   completion signal has been deployed.

The immediate goal is milestone 4: derive a 13.60 patch map from the captured
code, compare it with downstream HEN/kstuff needs, and build a checked
post-wake completion signal before autoloader integration. The operational
read/write gate is met on one post-AP boot. AP causality remains an open
research question and must not be inferred from this gate.

## Fresh-boot pre-AP comparison (13 October)

On a standalone Relapse and elfldr boot, a read-only preflight reported zero
Byepervisor stage and hook markers and a protected `sys_getuid` text PDE.
One guarded diagnostic cleared that PDE's XOTEXT bit and read it back, then
the host timed out before any text-read or restore report. The user reported
an unresponsive console and unexpected `ps5upload` activity after the send.
The diagnostic source does not launch other payloads. Its output does not
establish whether the text read started, which instruction failed, or whether
the PDE was restored. This is a failed and potentially confounded baseline;
do not repeat the same live probe. After recovery, first reconcile boot and
PDE state with read-only checks. The previous post-AP read successes and this
pre-AP failure suggest a stage-dependent difference but do not prove it.

After a new Relapse/elfldr boot, a read-only preflight confirmed zero stage
markers and a protected PDE. A guarded read-only alias walker then found no
present entry for the calculated direct-map alias of the target text bytes
and stopped before any content read. It made no mapping change and supplied
no pre-AP text bytes. The alias route is unavailable as written on that boot;
the causal comparison remains incomplete.
