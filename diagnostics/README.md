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

## Bounded 64 KiB region

`kernel_text_region_1360.c` captures `text+0x2a0000` through `+0x2affff` on
the same boot. It obtains `kern.boottime`, checks the AP/hook state, then walks
all sixteen 4 KiB addresses before changing any mapping. Every address must
resolve to the exact measured 2 MiB PDE; an absent or different leaf aborts
before a write. It clears one PDE bit, copies the fixed 64 KiB region in 256
byte chunks, restores and checks the PDE, then emits framed page records.
This candidate stops on a failed leaf or read and never tries to skip an
unverified hole. It is deliberately confined to this one PDE and boot.

`parse_kernel_text_region.py` requires the complete ordered stream, matching
boot-time records, successful clear/restore readbacks, and exact equality of
page 8 with the independent 4 KiB capture. It reports a SHA-256 for each
page and the region. Five synthetic parser checks pass. The saved ELF,
host stream, raw partial image, postcheck, and manifest are in this directory.
An independent read-only page walk after the region capture returned the
original protected PDE.

For IDA, open `captures/kernel-text-region-1360.bin` as a **raw x86-64
binary** and set its loading address to `0xffffffffddce0000`. The file spans
exactly `0xffffffffddce0000` through `0xffffffffddceffff`; it has no ELF
headers, sections, relocations, symbols, or bytes outside this window. The
console reported boot time `2026-10-13T11:15:09.471453Z`, and the host file
was created at `2026-10-13T11:52:58Z`. These are provenance observations,
not proof of an independent console identity. No conclusion about patch
sites should be made from a missing portion of the kernel image.

## Milestone 3: read coverage and restoration survey

The read-only `text_mapping_survey_1360.c` found five 2 MiB `XOTEXT` leaves
between 4 KiB-mapped text edges. `text_span_samples_1360.c` temporarily
cleared one measured PDE at a time to capture a 4 KiB page from each of the
four untested 2 MiB leaves. All four reads completed; each PDE was restored
and read back before the next page. A separate mapping survey afterward was
byte-identical to the original 829-byte survey.

`text_boundary_pte_survey_1360.c` read all 704 PTEs covering the first and
last 4 KiB-mapped text spans. All 704 were present, `XOTEXT`, read-only, and
NX-clear on this boot, with continuous physical page numbering. It did not
read their protected page contents. `text_boundary_samples_1360.c` then
captured one page at each edge, restoring the exact modified PTE after each.
The first parent PDE's accessed bit changed from `0x03` to `0x23` during that
read; the two text PTEs themselves returned to their original values. The
read-only postcheck records both. A revised boundary PTE survey permits only
that dynamic accessed bit in the parent PDE guard and produced a byte-identical
704-entry list after the samples. Treat parent accessed bits separately from
the XOTEXT bit that these diagnostics changed.

These samples now cover each observed text mapping *type* and every 2 MiB
leaf, but they are not a complete text dump or proof that every page can be
read. All results share the reported console boot time
`2026-10-13T11:15:09.471453Z` and pmap root `0x21894000`. A fresh-boot test
without the AP stage is still needed to isolate the stage's causal effect;
no text-write experiment has run.
