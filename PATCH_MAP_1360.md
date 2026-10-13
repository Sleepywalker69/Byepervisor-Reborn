# 13.60 kernel patch-map working notes

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

## Gate before a downstream patch

For each proposed behavior change, identify the owning function and callers
in the local IDA database, record exact preimage and intended replacement,
show the consumer that needs the change, test one reversible change under
firmware/boot/stage/mapping guards, and independently re-read the result and
restoration. The first post-AP one-byte write was an inert NOP-area test and
does not satisfy those semantic checks for `mprotect`, fSELF, fPKG, or CFI.
AP causality is still unproved; the user asked not to repeat the failed
pre-HV read on this console merely to settle that question.
