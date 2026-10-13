# 13.60 post-wake kernel-text diagnostics

These small diagnostic sources and the retained single-PDE ELF were used after
the 13 October 2026 `STAGE_AP_COMPLETE` observation on one regular-model PS5.
They are separate from the S3 payload. They do not install a hook or request
Rest Mode.

## Observed sequence

1. `text_pte_probe.c` walked the live kernel pmap using its direct-map alias.
   The `sys_getuid` text VA had a 2 MiB leaf PDE
   `0x040000001dc001a1` (`xotext=1`, `rw=0`). The original first version of
   this probe used the physical pmap root as a virtual address and stopped at
   a failed read; the checked-in source contains the correction.
2. `text_alias_probe.c` derived the corresponding physical address and tried
   an eight-byte direct-map read. `kernel_copyout` returned `-1`.
3. `single_pde_xom_probe.c` required exact firmware `0x13600007`, an
   `AP_COMPLETE` stage marker, zero hook-state flag, and the exact PDE above.
   It cleared only bit 58 of that PDE, read eight bytes of `sys_getuid` text,
   then restored and read back the original PDE. The read returned
   `55 48 89 e5 48 8b 87 40`. A separate read-only page walk afterward
   reproduced the original PDE and flags exactly. The console stayed
   responsive.

The single-PDE ELF embeds the measured PDE value from this particular boot.
Its guard should refuse a different layout; it is not a portable 13.60 patch.
The tested ELF is `single_pde_xom_probe_1360_current_boot.elf`; its SHA-256
is in `SHA256SUMS`. Build from SDK v0.44 with:

```sh
$PS5_PAYLOAD_SDK/bin/prospero-clang -Wall -Werror -o single_pde_xom_probe.elf single_pde_xom_probe.c
```

This establishes one controlled kernel-text read and restoration. It does not
isolate whether the HV stage was necessary for that read, establish text
writes, or turn this into a persistent kernel dump or patch facility. No
post-wake baseline with the same one-PDE change was run before AP completion.

## Bounded 4 KiB capture

`kernel_text_page_1360.c` uses the same exact firmware, AP-stage, hook-state,
and measured-PDE guards. It clears only the `XOTEXT` bit in that PDE, reads
one 4 KiB page in sixteen 256-byte chunks, restores and reads back the PDE,
then prints a framed hex capture. It does not request Rest Mode or leave a
text-mapping change installed. The saved ELF is
`kernel_text_page_1360_current_boot.elf`; this name reflects its exact
boot-specific PDE guard.

`parse_kernel_text_page.py` rejects incomplete or reordered data records,
missing restore evidence, and a mismatch at the previously read `sys_getuid`
bytes. Run `python -m unittest test_parse_kernel_text_page.py` from this
directory for its synthetic parser checks. `captures/` contains the console
stream, parsed 4,096-byte page, manifest, and an independent read-only PDE
postcheck. These establish one captured page and restored guest PDE on the
observed boot. They do not prove that all 13.60 text pages are readable, that
the HV stage caused this access, or that writes to text are possible.
