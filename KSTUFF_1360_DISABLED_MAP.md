# FW 13.60 kstuff gates and offset audit

This is an offline audit of the local `EchoStretch/kstuff-lite` source at
`48cd2c4` and one saved retail 13.60 capture. It distinguishes a table
entry, captured bytes, control-flow evidence, and live behavior. The exact
source of a different installed `kstuff-fpkg-1.13-dr-test6.elf` remains
unverified. The capture is one non-atomic boot and is not published here.

## Entire kernel offset table: byte coverage

The 13.60 table has **78 listed entries**. The private offline checker
verified the saved kernel text and mapped-data SHA-256 before resolving every
`DEF` expression relative to `kdata = text + 0xcc0000`. It mapped **64**
entries into captured text, **13** into captured mapped data, and **one**
(`p_sysent`) as a structure-field offset rather than an image address.
None landed outside the capture. All **11**
`sceSblServiceMailbox_lr_*` return-site entries have a preceding direct
`call` that resolves to the table's `sceSblServiceMailbox` address. The
eight PPR/fPKG sites are present in the text capture; earlier validation
also matched their paired call targets. Several debugger gadgets decode
as the expected first instructions.

This is complete **byte coverage of the published 13.60 table**, not proof
that every symbol has the right owner, ABI, stack layout, or runtime effect.
In particular, an all-zero first window at `gdt_array` is only a captured
byte observation. Runtime CPU ownership, context registers, per-CPU stride,
cleanup and cross-boot stability are not settled by an in-range address.
The resident source defines `GDT(i)` but has no active use of that macro;
its zero window alone is therefore not a demonstrated blocker for the
current loader path. `TSS(i)` is used for IST reads and writes.

A further private offline audit checked the 16 table-indexed TSS records in
the saved 13.60 mapped-data capture at the source's `0x68` stride. Every
record had a populated canonical high-half ring-0 stack field and IST4
field; the IST3 and IST7 fields targeted by kstuff were zero in that
capture. This supports the table/stride interpretation for that one boot.
It does **not** prove the current live values, CPU ownership, write safety,
or restoration after a kstuff attempt.

## Feature gates and disabled paths

| Path | Current source behavior on 13.60 | Evidence still needed before relying on it |
| --- | --- | --- |
| Plaintext PPR/fPKG | Upstream `get_ppr_profile()` returns `NULL` above 11.60 even though it contains a 13.60 profile. A separate exact-13.60 opt-in source patch and observation build were compiled. A narrow live check reached that profile and later saw PPR G6 activity. | Complete mount result, plaintext read behavior, synthetic key-pair release, and actual title execution. The 13.60 A53 side has no verified profile. |
| A53 PPR patch | No 13.60 profile in the reviewed [public A53 patcher](https://github.com/drakmor/ppr-patch); its exact profile archive ends at 11.40. Earlier bounded live reads returned zero bytes with status `0x20`; its meaning was not resolved. | A matching 13.60 A53 image or verified live memory map, then selector, caller, preimage and readback validation. The kernel capture alone cannot supply this. |
| fSELF, NPDRM and generic syscall interception | The 13.60 kernel offset table and resident handlers are present; there is no top-level 13.60 disable switch for these paths. `NPDRM_PORTING` is a commented diagnostic compile mode, not the production path. | Exercise each relevant consumer with separate status/cleanup checks. A byte-mapped mailbox return site does not validate request structures or emulation. |
| `FIRMWARE_PORTING` | Not enabled in the normal build. It relaxes unsupported-firmware and unresolved-symbol failures and adds porting hooks. | Do not use it as a workaround; resolve any missing symbol or layout explicitly. |
| `KSTUFF_OBS` | Optional instrumentation; enabled in the observed 13.60 candidate to expose counters. | Associate each counter change with an exact operation and module build; counters alone do not prove fPKG launch. |
| `USE_INT3_SYSCALL_HOOK` | Defined as `1` in the resident loader; it is active, not a disabled feature. | Check IDT/TSS ownership and downstream cleanup separately from the pre-kstuff Byepervisor gate equality. |
| Automount | The loader skips scanning and mounting when `/data/.kstuff_noautomount` exists. | Inspect this switch on a test boot before attributing a missing mount to offsets. Do not conflate automount with PPR emulation. |
| ShellCore retail patches | The 13.60 table is selected by the full loader and is not disabled. The loader also installs a separate dynamic PPR hook. | Match the exact on-disk and live ShellCore build, patch preimages, and hook lifecycle. One table entry below was corrected in an offline candidate that has not been tested live. |

The `#if 0` blocks in the optional GDB stub and r0gdb sources are not
automatically required for the resident kstuff feature path. Testkit and
devkit ShellCore tables are distinct and were not checked against this
retail console.

## On-disk ShellCore capture and patch-table finding

Read-only FTP copied `/system/vsh/SceShellCore.elf` from this retail 13.60
console: **35,337,588 bytes**, SHA-256
`15606a2cb87b73b091ed2121b37424ea8826d301ba6d07ce0a508da4979ea090`.
It contains readable x86 code. ELF program-header mapping put all **34**
13.60 retail patch entries in file-backed executable bytes; none already
equaled its replacement bytes. The private per-site map records the original
bytes, replacement, file offset and short disassembly. This is an **on-disk**
comparison. A later read-only ps5debug capture on the manual-mode post-HV
boot matched the running ShellCore's bytes at all 34 patch addresses. The
captured context also verified the corrected two-byte original `0f 86` at
`0x36ae22`. These reads were small and non-atomic; the debugger's transport
does not independently prove memory validity. The dynamic ShellCore PPR hook
is a separate path.

One source entry is a concrete mismatch:

| Table site | Original instruction | Published replacement | Result of decoding those bytes |
| --- | --- | --- | --- |
| `0x36ae22`, PS5 PKG Installer Patch 2 | `0f 86 f1 00 00 00` → `jbe 0x36af19` | one byte `eb` | `eb 86` → `jmp 0x36adaa`, backward to a different address |

The PS4 sibling and other near-branch entries use two bytes `90 e9`.
Substituting `90 e9` at this 13.60 site decodes as `nop; jmp 0x36af19`,
preserving the original branch target while making it unconditional. A
**source-only candidate patch** is in
`downstream/kstuff-lite-shellcore-1360-branch-candidate.patch` and passes
`git apply --check` against the local source. It is not applied to the
running console, and no rebuilt loader has been live-tested with it. This
finding concerns future kstuff/ShellCore activity; it does **not** explain
the earlier marker-image power-off, when this table was not newly deployed.

A second offline pass decoded the original and overlaid instructions for all
34 retail entries. It found 16 branches preserving their original target,
four whose condition is removed in favor of fallthrough, two redirects to
code caves patched by the same table, and one intentional displacement edit
inside a branch. Ten entries are non-branch semantic changes. The sole
unexpected branch target is `0x36ae22` above. This checks encoding and local
branch targets only; it cannot validate the intended feature behavior or
the loaded module's bytes.

An opt-in observation/mount-result full loader was rebuilt privately with
that source-only table correction. A separate offline compiled-binary check
parsed all 34 retail table rows from its payload ELF and verified the
corrected `0x36ae22` row contains two bytes, `90 e9`; the matching payload
occurs once inside the full loader. A later guarded full-loader test stopped
before the ShellCore patch loop because its remote module-info call failed.
The outer loader reported the failure and skipped its follow-on work; the
operator confirmed the home screen remained responsive. It did **not**
establish loaded kstuff or fPKG support. A revised source candidate uses a
target-process buffer for that call and verifies compiled patch/preimage
tables offline. Its single live run stopped at `info_map`, still before the
ShellCore patch loop, with the home screen responsive. Source review then
found that this preflight ran before kstuff installed its remote-syscall
facility. A third private candidate moves the guard after that setup but
before ShellCore writes; it passes offline table checks and has not run.

That third candidate passed remote allocation and buffer setup on a fresh
post-HV boot, then stopped at `dynlib_get_info_ex` before ShellCore writes.
The operator again confirmed a responsive home screen. The source passed
handle zero directly, while its own module-listing code uses handles from
`dynlib_get_list`. A fourth private candidate enumerates module handles,
checks two live ShellCore byte anchors, then checks all 34 sites before
patching. Its compiled tables pass offline verification; it has not run.

## Next qualification gates

1. Preserve the present no-extra-payload YouTube mode while the post-wake
   failure is isolated. Run a fresh manual-mode wake only with an operator
   present and require an attempt-specific AP receipt.
2. For the next guarded kstuff loader, repeat the current-boot receipt
   check, then test the remote module-info path after the syscall facility
   is installed. The guard now runs after some kernel changes, so a failed
   guard does not imply an unchanged system. Require a loader success log
   and functional readback before calling kstuff loaded; socket
   transmission alone is insufficient.
3. Verify scoped PPR mount, title execution and key cleanup independently.
   The A53 13.60 image/profile is a separate unresolved input. Do not call
   the byte map complete fPKG, HEN, or Linux support.

Raw kernel and ShellCore images, private offset maps, and IDA databases
remain local and are not part of this repository. No 13.60 A53 image was
obtained.

The reviewed public A53 tooling uses `/dev/mp4/dump` and DECI5S
`READ_MEMORY`, but its published profile and code-execution examples cover
older firmware. A [recent transport report](https://github.com/drakmor/ppr-patch/issues/2)
fixes a specific 9.40–11.20 aperture/sequence issue and reports an 11.20
test; it does not verify the same fix or an A53 memory map on 13.60. The
next 13.60 acquisition step is to identify a valid live memory aperture and
interpret the observed `0x20` response before attempting a bounded read,
not to reuse old A53 addresses as if they were mapped on this firmware.
