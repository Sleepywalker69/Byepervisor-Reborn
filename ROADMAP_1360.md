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
2. **Bounded region capture.** Generalize the page walker to validate each
   mapped leaf before reading, skip or stop at holes, and retain per-page
   hashes and firmware/boot provenance. Grow only after the first capture is
   checked. Produce an analyzable kernel image or a clearly documented partial
   image for IDA; do not infer symbols from an opaque PUP component.
3. **Separate capability checks.** Verify read-only access and restoration
   across the relevant kernel-text regions, then make any write experiment
   reversible with exact preimage and post-write checks. Check whether the
   access depends on the AP stage rather than assuming causality from a
   post-wake success. Keep the VMCB stage and later text-page changes as
   distinct results.
4. **13.60 patch map and downstream chain.** Derive firmware-specific patch
   sites from the captured code and compare with existing HEN/kstuff needs.
   Add one checked change at a time, then a completion signal that the
   autoloader can observe after wake. Only after that should Y2JB schedule
   later payloads. Linux requires its own much larger hardware and boot path.

The immediate goal is milestone 1. It provides a real 13.60 code sample and a
repeatable transport/checking method while retaining rollback evidence.
