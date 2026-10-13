# 13.60 kernel patch-map working notes

Milestone 4 scope update (13 October): full Kstuff is a feature reference,
while kstuff-lite contains the available 13.60 loader/FSELF/NPDRM paths. The
first downstream target should be a complete scoped capability with its
consumer and failure behavior, not a permanent copy of one debugger trap byte.
No candidate below has yet been applied as a functional kernel patch. The
attempt-specific post-wake receipt is still awaiting its first controlled
console run; downstream writes remain gated on it.

### Complete kstuff-lite path to examine after receipt

The local `1.11-opt` source is a substantial resident payload, not a small
standalone patch. `ps5-kstuff-ldr/src/main.c` embeds `ps5-kstuff/payload.bin`
and contains the image/mount loader. `ps5-kstuff/main.c` selects firmware
`1360`, rejects a second resident instance through `kekcall`, installs its
exception machinery, and applies firmware-specific ShellCore patches.
`ps5-kstuff/uelf/main.c` dispatches scoped FSELF and FPKG traps;
`ps5-kstuff/uelf/fpkg.c` selects the firmware-13 PPR profile, including 13.60.
For the user's requested full implementation, this complete loader and its
resident handlers are the relevant downstream unit. A single copied PPR or
`mprotect` byte would not reproduce that behavior. Before a post-wake load,
the chain must confirm this exact binary is not already resident and obtain a
module-ready result beyond a completed ELF socket send. Actual fPKG or child
process behavior remains unproved here.

The bundled elfldr server's `accept` loop calls `on_connection` and then
returns to `accept`; its `elfldr_spawn` returns a PID after arranging the new
process. This source shape permits a separate status connection while the HV
process is still running, but it does not establish that elfldr survives Rest
Mode or that the guest payload reached its own ready state. Those are live
checks for the controlled attempt.

These observations use the verified local capture from console boot
`1791893784.399222`: text base `0xffffffffd3b70000`, data base
`0xffffffffd4830000`, text SHA-256
`20da45a0910c990153fbfb7c94e16356867e0bd84f7c9fbf754a45a5dd129673`,
and mapped-data SHA-256
`29a0fd3d0ff5290897981543dcc7c71e02105477597b7d4523ded09ffdfdc318`.
The raw files and IDA database remain local and are not part of this repo.
For the local kstuff-lite 13.60 table, `kdata_base = text_base + 0xcc0000`;
the negative `DEF` deltas below are converted to text offsets using that
anchor. A byte match validates an address in this capture, not the behavior
of a future patch or another firmware build.

| Candidate | 13.60 text/data offset | Captured bytes or data | Source use and current finding |
| --- | ---: | --- | --- |
| `sys_getuid` | text `+0x2a8d60` | `55 48 89 e5 48 8b 87 40 01 00 00` | Fork symbol offset. IDA defines a 25-byte routine that reads a credential value through the thread pointer and returns zero. Independently anchored by the syscall table below. |
| `sysents` | data `+0x1b6e50` | entry 24 `sy_call` at `+0x1b72d8` is `0xffffffffd3dd5330` | Kstuff table candidate. The pointed-to text stub at `text+0x265330` begins `e9 2b 3a 04 00` and jumps to `sys_getuid` at `text+0x2a8d60`. This independently supports the table layout and anchor on this boot. |
| `mprotect_fix_start` | text `+0x2cbfed` | `0f 85 c0 10 00 00` (`jne text+0x2cd0b3`) | Kstuff's debugger changes RIP to `start+6` for its instrumented `mprotect20()` call. IDA places the branch in a larger function referencing `[KERNEL] mprotect: page table exhausted`. This is a branch-skip candidate for that narrow call path, not a validated global patch. |
| `aslr_fix_start` | text `+0x38fa98` | `48 8b 3b 44 0f b6 7d a8` | Kstuff uses a trap here to set an app wire flag for `/app0/eboot.bin`, then resumes at `start-0xe`. The captured instruction bytes match the table address; function ownership and side effects need more IDA work. |
| `syscall_cfi_table_jmp_int3` | text `+0x266e40` | `e9 9b 32 e3 ff cc cc cc` | Kstuff uses this as a target in cloned syscall tables. The address is a jump stub followed by three `int3` bytes; the actual CFI behavior and target compatibility have not been exercised on this post-HV boot. |
| `cr0_load` | text `+0x2b3bd4` | `mov rax,[rdi+0x58]; pop rbp; ret` | Kstuff debugger gadget location matches disassembly. |
| `cr0_clear_store` | text `+0x6f812d` | `and rax,~8; jmp text+0x6f7ff0` | Kstuff gadget location matches disassembly. |
| `cr0_write_ret` | text `+0x73167d` | `mov cr0,rax; pop rbp; ret` | Kstuff gadget location matches disassembly. |
| `store_rax_rdi` | text `+0xacbb5e` | `mov [rdi],rax; pop rbp; ret` | Kstuff gadget location matches disassembly. |

The older Byepervisor 2.50 patch table includes a broad set of syscall,
capability, CFI, panic, and loader changes, and its patch loop does not check
`kernel_copyin` results. Its offsets and replacement bytes are not portable
to 13.60. Kstuff's 13.60 `mprotect` and ASLR entries are debugger trap
addresses that alter RIP only in specific flows; converting them into
permanent global patches would change the scope of their behavior.

## PPR/fPKG source coordinates, pending semantic review

The local kstuff-lite `offsets/13_60.h` also supplies the following
data-relative coordinates. Translating each by `+0xcc0000` lands inside the
captured text. The first two and `cleanup_keys` begin with plausible function
prologues or a jump wrapper; the other entries are interior instruction
addresses. These byte matches help navigate IDA, but do not establish that a
static patch at any entry would reproduce kstuff's thread-scoped behavior.

| kstuff name | Text offset | First captured bytes |
| --- | ---: | --- |
| `ppr_pfs_get_xts_index` | `+0xb93920` | `55 48 89 e5 41 57 41 56` |
| `ppr_pfs_get_cmac_index` | `+0xb93a80` | `55 48 89 e5 41 57 41 56` |
| `ppr_pfs_get_xts_return` | `+0x376409` | `85 c0 74 37 48 8d 3d 02` |
| `ppr_pfs_get_cmac_return` | `+0x376454` | `85 c0 74 2b 48 8d 3d b7` |
| `ppr_pfs_cleanup_keys` | `+0x36b470` | `55 48 89 e5 5d e9 06 00` |
| `ppr_pfs_clear_key_missing` | `+0x2886a1` | `bb fe ff ff ff e8 65 4d` |
| `sceSblServiceMailbox_lr_verifyImage` | `+0x287203` | `44 8b a5 d4 fe ff ff 89` |
| `ppr_pfs_verify_image_no_key_success` | `+0x28755e` | `c4 63 fb f0 b5 b0 fe ff` |

The source's `fpkg.c` selects an explicit firmware-13 profile and uses these
addresses in a coordinated PPR key and image-verification path. A candidate
kernel patch needs the whole call and failure flow, not just one listed byte.

## Gate before a downstream patch

For each proposed behavior change, identify the owning function and callers
in the local IDA database, record exact preimage and intended replacement,
show the consumer that needs the change, test one reversible change under
firmware/boot/stage/mapping guards, and independently re-read the result and
restoration. The first post-AP one-byte write was an inert NOP-area test and
does not satisfy those semantic checks for `mprotect`, fSELF, fPKG, or CFI.
AP causality is still unproved; the user asked not to repeat the failed
pre-HV read on this console merely to settle that question.
