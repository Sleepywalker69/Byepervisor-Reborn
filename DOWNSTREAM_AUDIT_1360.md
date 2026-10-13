# 13.60 downstream binary audit

Current status: see **V5–V8 offline reconciliation and remaining functional gates** below. Historical sections retain their original evidence ceilings. Live work is paused after the reported V7 kernel panic.

## Installed kstuff fPKG ELF, offline evidence

The installed file `/data/pldmgr/payloads/kstuff-fpkg/kstuff-fpkg-1.13-dr-test6.elf`
was retrieved read-only over FTP 2121 after the manual-mode receipt check.
It is 1,818,680 bytes, SHA-256
`b1f60c1ec1685904d396da16710dc9d5676bf0880abf74c65459c886010b9443`,
matching the earlier saved hash. The private copy and IDA database are under
`downstream-kstuff-audit`; neither is tracked here. ELF inspection reports
an x86-64 PIE with seven console shared-library dependencies and no ordinary
symbol table. The binary embeds SQLite code. Its exact source revision is
unverified, so the current public kstuff-lite source is a mechanism reference,
not proof of this build's behavior.

IDA's function at ELF offset `0x8e40` is the PPR installation routine.
At `0x8e5e` it calls a firmware-version reader; at `0x8e66` it masks the
result with `0xffff0000`, then executes `cmp ebx,0x12000000` and an unsigned
`jb` at `0x8e72`. The fall-through at `0x8e74` prints
`[ppr-patch] firmware 12+ support is not present in this build`, jumps to
the failure return, and never enters its lower-firmware profile selection.
The reported 13.60 value `0x13600007` becomes `0x13600000` after the mask,
which is above the cutoff. This is a firmware-specific control-flow proof
for this PPR installer, not merely an embedded-string inference.

The loader's PPR thread at `0x9820` calls this routine at `0x9a48`. Its
failure path reports `PPR patch installation failed. Plaintext PPR is
disabled.` This establishes that the installed ELF cannot supply a
successful firmware-13.60 *PPR patch through this routine*. It does not
establish that all of its resident kstuff features fail, or that no other
payload could provide fPKG behavior. No live load or fPKG test was made
for this audit.

## Public kstuff-lite source gate

The local public `EchoStretch/kstuff-lite` checkout at commit `48cd2c4`
contains a `fw13` PPR profile and a `case 0x1360` in
`ps5-kstuff/uelf/fpkg.c` (lines 165 and 203). But `get_ppr_profile()`
returns `NULL` for `FWVER > 0x1160` at line 102, before that switch. Its
adjacent comment explicitly says plaintext PPR interception should not be
armed above 11.60. The helper functions for XTS/CMAC indices, cleanup calls,
and missing-key handling return zero when no profile is available. Thus the
13.60 table entry is dormant in that source revision. A successful offline
build of it would not by itself enable 13.60 plaintext PPR. This source
finding is independent of the installed binary's different 12+ gate.

The 13.60 profile values and offset table may help locate functions for
research, but their presence does not validate the verifyImage and key
cleanup ABI on a retail 13.60 boot. The source comments call out the need
for static validation of the paired call sites against a matching image.

## Static checker and IDA cross-check on the private 13.60 capture

The source's unchanged `tools/validate_ppr_offsets.py` was run against the
private synthetic ELF assembled from the measured 13.60 text and mapped-data
ranges. A private wrapper supplies version key `13_60` because the synthetic
ELF filename is descriptive rather than `13.60.elf`; it does not change the
validator's checks. The result was `checks=PASS count=0`. The 268-byte
private result file `downstream-kstuff-audit/ppr-static-validate.txt` has
SHA-256 `2202fb58c76cfcaac92576b8451bbbdad3de9bdbe3e5b01e1e915490877c7601`;
the wrapper's SHA-256 is
`a2778995f1c53f45ca9b83c08b7089c65c3efcb7639d152ca5d01c0c8a1531b8`.
The synthetic ELF and wrapper stay local.

IDA separately identifies `ppfs_get_xts_index` at
`0xffffffffd4703920` and `ppfs_get_cmac_index` at
`0xffffffffd4703a80` by their embedded diagnostics and code. The common
`setup_a53io_pkg_keys` caller at `0xffffffffd3ee6370` calls them at
`0xffffffffd3ee6404` (`e8 17 d5 81 00`) and
`0xffffffffd3ee644f` (`e8 2c d6 81 00`) after its initial `-1`
key-index check. In cleanup, the 13.60 profile's `+0x12b` site is
`0xffffffffd3edb59b` (`e8 d0 85 82 00`) and calls a function labeled
`ppfs_put_cmac_index` by its diagnostic. The `+0x15b` site is
`0xffffffffd3edb5cb` (`e8 40 84 82 00`) and calls the analogous
`ppfs_put_xts_index` function. This extends the validator's check, which
only requires an E8 call at those two cleanup sites, with exact target
semantics from the captured code.

The pass is a static match for the expected instruction relationships on
one non-atomic capture. It does not validate live register contents,
secure-module responses, lifecycle handling, or the safety of enabling the
source's dormant 13.60 profile. No PPR interception or fPKG test ran.

A further offline trace of the same captured x86 code found that the
verifyImage no-key success block loads a packed qword from frame `-0x150`
and publishes its lower and upper 32-bit halves as the two key indices.
The clear-key missing-key trap's profiled `+0x0a` continuation joins its
common epilogue. These are selected **static ABI relationships**; they do
not make the whole upstream implementation or its A53 side 13.60-ready.

## Gate for downstream use

Do not count sending this ELF, its process startup, or the public
kstuff-lite 13.60 offset table as proof of fPKG/PPR support. A replacement
candidate needs a verified 13.60 PPR path with the source gate and ABI
reviewed, or an explicit narrower feature
goal, a module-ready receipt from the exact launched build, and a functional
test of that feature on a fresh manual-mode wake. Keep the current no-launch
post-wake default while selecting and testing one downstream module.

## First opt-in candidate run and fPKG install boundary

On an existing manual-mode 13.60 wake, the pre-launch Byepervisor receipt
passed with a matching attempt ID, AP-complete stage, cleared hook and
restored gate/IST. The observed opt-in kstuff-lite loader then started;
its separate debug reader attached to the resident module. PPR counters
were zero before any package operation. The same Byepervisor gate/IST
equality check failed after kstuff loaded. A bounded readback placed the
live gate at kstuff's 13.60 `add_rsp_iret` offset, with a nonzero IST6
consistent with its debugger setup. This is evidence of downstream
interposition on the shared gate/TSS slot; it does not prove the later
cleanup lifecycle is safe.

A private, structurally accepted homebrew test package was transferred
with a complete matching readback. One installer probe returned no
stdout before a host timeout. A separate read-only preflight reached
`main` and confirmed the package size; a read-only status probe linked
to `libSceAppInstUtil.sprx` emitted nothing. An unlinked probe printed
before `dlopen` of that library and did not print afterward within 30
seconds. The library load path is therefore the current observed stop,
though the reason is not established. Package installation and PPR
activity remain unproved. No automatic post-wake payload launch or
installer retry was added.

The observed kstuff-lite source routes `dynlib_load_prx` through
`handle_fself_syscall()`, which clears its fSELF caches and arms debug
register traps. Its `trap` counter marks a trap seen during that syscall;
its `emulated` counter marks only an emulated result. The rise in traps
with zero emulations during the AppInstUtil probes establishes
interception, not responsibility for the unresolved library load. A
fresh-boot control before loading kstuff is required to separate the
downstream hook from the native loader's behavior.

## Native installer and registered-title launch check

The silent AppInstUtil probes were missing a runtime dependency. The PS5
Payload SDK issue [#26](https://github.com/ps5-payload-dev/sdk/issues/26)
requires `libSceIpmi` before `libSceAppInstUtil` in the ELF dependency order.
A rebuilt read-only probe with that order reached `main`, initialized the
installer, queried the test content ID, and terminated normally. The first
correctly linked install request using `/data/...` returned `0x80b2150f`.
A read-only alias check then confirmed the package under `/user/data/...`.
One install request using that URI returned 0 and the expected content ID.
Its first status poll was `transferring`; the next poll returned
`0x80a30003`. A separate status query afterward returned
`rc=0 status=playable error=0 progress=100`. The installed title files and
one app.db content row were independently observed. The private package,
app.db and ELF artifacts remain off GitHub.

A one-shot, in-process `sceSystemServiceLaunchApp` call for the registered
test title ran on the same boot. Foreground user initialization and lookup
both returned 0, and the call used a 32-byte context with that user ID.
Sony's launcher returned `0x80f00800`, a negative result. No title marker
appeared and an immediate process snapshot did not show the test title.
The 110,912-byte launch probe SHA-256 was
`2345ee742214caed4600a884722a776da1e46b0b16c77331aa2cc5ed41202305`;
its 602-byte host log SHA-256 was
`890746d93e8577e32306468d12e9cedd03ed1175abbe3883841829e5df7c6878`.
This is a rejected launch, not a demonstrated homebrew run.

The post-launch kstuff reader still reported `ppr_plaintext_g6` traps,
profile, and applied as zero, and fPKG verify/clear as zero. Its 4,185-byte
log SHA-256 was
`1a9b0d4cea1272f5ce5c643d7ec610ac1d2b4b5f776b96c8f7d287dcb330f340`.
The native launcher rejection alone does not identify its cause. In
particular, package registration and the `playable` installer string do not
prove runtime authorization.

A second one-shot launch request used the same ABI for an already installed
homebrew folder app, Prospero Explorer (`PPSA99109`). Sony returned app ID
`0x201a`, and the next process snapshot showed a new `eboot.bin` process.
The 110,920-byte control ELF SHA-256 was
`48b99e24cf07eeea121729111dfc9bfb25a4961dc2d92aaf692608c1eb0d5ce6`;
its host log SHA-256 was
`d809941f5f307cae39d57b196f7fcfc56996080406b211f7fb435f1d57e14a8f`.
The launcher ABI works on this boot. The control app is a folder mount,
whereas the new test title is an `app.pkg` registered with `"fake":true`.
That difference leaves package structure, mounting, and fake-package
authorization as live explanations; the error alone does not select one.

The public [A53 PPR patcher](https://github.com/drakmor/ppr-patch/blob/main/README.md)
lists exact profiles only through 11.40. Its standalone fast installer is
therefore not a ready 13.60 dependency. The current kstuff opt-in source
gate and matching kernel offsets do not supply that A53 profile. The next
PPR gate is to identify the 13.60 A53 image and validate its full selector
and caller layout before a plaintext package-read test.

The public patcher's default status action ran once on 13.60. It obtained
the live retail A53 `releases/13.60` version through `GET_CONF`, then
reported `no exact PPR profile` and exited before any patch action. Three
bounded memory-read probes used the older physical layout address, older
physical L2-table address, and a late-firmware EL3 virtual address. Each
returned a valid response with status `0x20` and zero transferred bytes;
`GET_CONF` continued to work. The status value's precise meaning is not
established, and the probes do not show that every A53 address is blocked.
The retained 13.60 PUP has no plaintext A53 release marker in a bounded
whole-file scan. A matching A53 image or independently validated live
memory map remains necessary for a 13.60 profile.

The public `astrelsky/mp4rw` source at `cc8100f` uses the same DECI5S
`READ_MEMORY` command, physical and EL3-VA selectors, and alignment-based
access-size encoding as the diagnostic probe. It does not define the
observed `0x20` result's meaning. This source comparison found no verified
packet fix for the failed reads.

**Offline correction to the historical A53 anchor probe:** the three
`0x88100000`/`0x88440000`/`0x8a000000` requests above used the patcher's
ordinary read callback, which sets `mem_type=0x020000` (physical-to-EL3).
The `astrelsky/mp4rw` reference probes those values with `mp4_read()`, whose
header sets `mem_type=0x430000` (EL3 virtual). Therefore the three failures
do not test those candidate EL3 virtual addresses in the reference mode.
A separate EL3-virtual diagnostic read at `0x110000` also returned `0x20`
and zero bytes; it did not probe the three anchors. This does not establish
a current 13.60 map, a working read command, or the meaning of `0x20`.
No console retry was made after the V7 panic.

For this candidate's plaintext/no-auth design, the kernel passes synthetic
AES `0xff` and SHA `0xfe` indices toward A53. The reviewed A53 selector
source matches that complete pair and substitutes plaintext transfer and
terminal completion for stock AES-XTS/SHA3-CMAC work. The stock read path
is not shown to accept the synthetic pair. A validated 13.60 A53 selector
or an independently demonstrated equivalent is therefore a remaining
functional gate for this design. The public selector profiles reviewed here
end at 11.40. The saved x86 kernel text/mapped-data capture and the outer
decrypted PUP supplied no directly usable matching A53 ELF in bounded
checks. An alleged unreleased A53 exploit is not a reproducible dependency.

The private opt-in source also needs a failure-atomicity review before a
new plaintext request: its verifyImage emulation modifies output buffers
before two later kernel copies can fail, and those failure branches return
to the stock mailbox path without restoring every earlier write. This is
a source-level potential hazard, not an observed cause of V5/V6 failure.
The outstanding synthetic-pair metric is a global count, not per-title
ownership proof.

The same public example names three older EL3 virtual addresses as possible
A53 ELF headers. The old 13.60 probe requested four bytes at each of
`0x88100000`, `0x88440000`, and `0x8a000000` through the **physical**
selector after confirming the live A53 version. All three returned
`status=0x20` and zero bytes. The probe source and output remain private.
Those results do not test the example's virtual-address calls, and no
matching 13.60 image or current map has been established.

## Live 13.60 kernel-side PPR profile gate

The observed opt-in kstuff build exposes `control_ppr_plaintext_request`
through kekcall number 7. Within a per-thread PPR mount scope, `CHECK`
returns `EINVAL` before staging ownership when the firmware profile is
disabled, and `EPERM` for absent staging ownership after an active profile
passes. A one-shot probe entered version-8 scope kind 3, called `CHECK`
without writing staging data, then left the scope. It recorded
`kstuff_check=0 scope_enter=0 check=-1 scope_leave=0`. This is the
profile-enabled result on the live 13.60 boot and a successful scope exit.

The tested 110,728-byte ELF SHA-256 is
`c4f6c3b07d6f2917f9939f8174e9fbbde33bf94c3295a552d3ba628f1cefe0e0`;
its source is `diagnostics/ppr_profile_gate_probe_1360.c`. The private
128-byte host log SHA-256 is
`87d2952408465d667628fc5c6224acd43c9a8932f29b3744359aca936ff5257c`.
A subsequent independent kstuff reader still reported zero PPR G6 traps,
profile matches, applications, fPKG verify/clear events and outstanding
plaintext key pairs. The result proves the narrow kernel-side 13.60 profile
gate is reachable; it does not prove staging, A53 PPR changes, or fPKG launch.

## Corrected plaintext package live result

A privately built 603,009-byte test package for title `PPSA99099` has
SHA-256 `2689572d106b4fa83a866276fad03dc42caecef110cdba94b89b7a66cdd31666`.
It was built using the public `ps5upload-fpkg` plaintext/no-auth mode,
contains the expected `PPRPLAIN-NOAUTH!` superblock marker and mode
`0x0d`, and declares `applicationDrmType: standard`. The builder's 47
checks and an independent bounded check against kstuff's FIH/superblock
validator passed. The package itself and console logs remain private.

It was uploaded once and byte-verified. Native installation was submitted
once; an independent status read subsequently returned `playable`, zero
error and 100% progress. A single native launch request returned
`0x80940010` with `accepted=0`. It did not prove title execution.

Before that launch, the independent kstuff reader had zero PPR/fPKG counts.
Afterward, it reported `fpkg verify=1 verify_emu=1`,
`ppr_plaintext_g6 traps=2 profile=1 applied=2`, a matched verifyImage
return address, zero malformed-request count, and hook trace stage 9.
The wrapper labels stage 9 as the one-shot mount latch being armed; it
precedes the original mount return. The reader also reported one
outstanding synthetic key pair and zero clear-key events. This proves
the corrected package reached the 13.60 kernel-side PPR/fPKG path.
It does not establish A53 plaintext handling, a successful mount, or
playable homebrew. No repeat launch was attempted while the key-pair
lifecycle is unresolved.

A delayed read-only snapshot still showed one outstanding synthetic key
pair and no clear-key event. The source wrapper releases a retained pair
if its original mount returns failure; this persistent count is consistent
with a mount held by the system, but it is not an independent mount-success
receipt. The title's visible sandbox and PFS mountpoint directories were
empty. A separate read-only process/app-info probe identified the running
`eboot.bin` as `PPSA01651` YouTube on this boot. Public ps5upload
launcher code documents `0x80940010` as an already-running-app condition
on FW13.60. The next launch test needs a fresh boot, a zero-pair baseline,
and YouTube closed; no second launch was made on this boot.

A separate read-only app-info probe resolved the active `eboot.bin`
process to `PPSA01651` (YouTube), app ID 24. A private guarded launcher
then detected that title and exited before invoking the native launch
call. Its blocked branch passed on this boot; its clear/launch branch
has not been exercised. This isolates the `0x80940010` observation
from package-format acceptance, which must be evaluated on the next
fresh boot with YouTube closed.

## V5–V8 offline reconciliation and remaining functional gates

The fork's `attempt-receipt-1360` branch and remote matched `58a587a` before this documentation update. The private candidate remains based on `EchoStretch/kstuff-lite` `48cd2c4` with uncommitted source changes. A separately reviewed full-kstuff feature reference (`EchoStretch/kstuff`, `elfldr-compatability`, `d44a254`) shows coordinated resident syscall/trap dispatch, fake-key registration and clear, and PFS crypto emulation. It is not a 13.60 candidate. Raw captures, private sources, ELFs, logs, and databases remain outside this repository.

The archived full loaders and single embedded payloads were independently SHA-256 checked without changing the originals:

| Version | Full loader SHA-256 | Embedded payload SHA-256 | Status and narrow result |
| --- | --- | --- | --- |
| V5 | `8e3dac338637a75231eb43a2174a7c5535cd2c8691f25e36063ae90aa368721b` | `331d99dc44f7d0107250616a6c3ce6c274fc004e1b1427c2fdad69059d7b7418` | **Executed once.** Fresh-boot V3 receipt passed; 34 live ShellCore originals matched; inner returned failure and outer skipped follow-ons. |
| V6 | `91ef11f071eb05a51dda6048e42726715ad8862128df29e3fa97d891c092ca35` | `36734fbf09c79137b27dc0e38f9b94c6398ff6c03ecb3d7be058215929fa63ac` | **Executed once.** Same checkpoints and byte-identical host stream as V5; added inner `SC_PATCH_V1` line did not reach the host. |
| V7 | `3343e74b60fad68bb91f7a611ed0a0d1af84d2afd640a1df3ed9bbfcbc4402c6` | `5d8771b2d3ec1290757bbe0b55ee996ba9deb750cd182b1fb6d5c6e37bae1e50` | **Sent once.** Receipt passed, then host got only its initial outer/library line and timed out; the operator reported a kernel panic. No preimage or numeric return checkpoint was captured. Never rerun as a diagnostic. |
| V8 | `1fb6124e8f562c63120561fac3de5b35723956841217c9cdbb7d2f9a02d456c9` | `8daeb9d96d38bd6d759f1445e1af6ef5b020a19241925b88f3a43efd9a49831d` | **Built/offline only.** Compiled 34-row verification passed. It has not been sent or executed. |

Control-flow review narrows V5/V6's ordinary nonzero inner return after the successful preimage gate to `patch_shellcore()` or its dynamic hook installation. It cannot select a page lock, patch copy/readback, live import/GOT lookup, wrapper mapping, or GOT-install failure. The saved ShellCore ELF has all seven required on-disk NIDs and PLT relocations; that does not validate runtime dynlib structures, GOT values, or import targets. V6's `SC_PATCH_V1` write ran inside the inner process after syscall/IDT changes, so its absence from host output cannot prove the branch was skipped. V7's initial `SceLncUtil/getAppStatus` line is an outer/library observation, not evidence that inner entry or the preimage gate ran. Its missing early checkpoint and reported panic do not identify a fault instruction.

Aligned embedded payloads differ at 1,014 bytes for V5/V6, 23,984 for V6/V7, and 242 for V6/V8. V7 also shifted embedding by `0x6d`; V8's smaller difference does not prove safety. Only the present V8 source state is retained as a source tree. The V5–V7 source snapshots are not separately archived, so their exact historical source-tree hashes cannot be supplied from the current checkout. The current V8 `ps5-kstuff/main.c` SHA-256 is `e2b76e376deea66d38762ccf4ad44575d996c663a3a758ba92eecde1434ee968`; the current V8 full loader and payload hashes match the table.

The source attempts reverse readback-verified restoration of touched ShellCore sites on patch failure and has a separate GOT rollback path. Neither restores prior IDT/TSS, cloned syscall tables, or crypt changes when the inner function returns failure. There is no independent V5/V6 post-return readback of ShellCore sites, GOT, IDT/TSS or resident health. A responsive home screen is a liveness observation only.

| Remaining gate | What is proved | What would falsify it / next bounded observation |
| --- | --- | --- |
| Load and health | Earlier opt-in observation build accepted a reader; V5/V6 failed and V7 panicked. | A nonzero inner return or unstable console falsifies load. Require an exact-build host return, resident check/reader snapshot, and independent post-load state on a fresh boot. |
| ShellCore patch and hook transaction | 34 original bytes matched live; corrected two-byte row compiled. Seven imports exist on disk. | Any applied or restored byte mismatch, invalid live import/GOT, or stale base. Require current-boot preimages, per-site applied readback, hook/GOT target readback and failure restoration readback. |
| PPR lifecycle | Earlier opt-in run reached kernel-side profile, fake-package verify and G6 traps. | A missing exact scope/hook return, failed mount, or pair not released. Capture original mount return, hook result, synthetic-pair ownership, clear/unmount, and mounted-content read in one bounded test. Stage 9 alone is not mount success. |
| A53 13.60 | Retail `releases/13.60` identified; historical-address reads returned status `0x20`, zero bytes. | A verified preimage/selector mismatch or unavailable read. Decode `0x20`; obtain exact 13.60 image or independently justified current map before any A53 change. |
| Functional fPKG | Test package installed and reported playable; a folder-app launch control succeeded. | Rejected launch, missing mount/read, no test-title process or visible execution, or leaked key pair. Final known-good 13.60 homebrew fPKG requires independent install, mount/read, process or on-screen run, and cleanup evidence. |

**Pause:** no console contact, image upload, Rest Mode cycle, ELF send or live test until the operator explicitly resumes. The console's recovery state after V7 is unknown. The first resumed interaction must establish that state before selecting one hash-pinned test with a fresh attempt receipt, durable log, checkpoint, stop condition and recovery plan. An automated payload chain is not a qualification path.

**Offline A53 diagnostic preparation.** A separate private resident LAN probe has been built and host-tested, but never sent to a PS5. It distinguishes physical (`0x020000`) and EL3 virtual (`0x430000`) SDBGP reads, records the full read result fields, permits repeated serial requests on one boot, and stops on malformed transport responses. The host can preserve one bounded page at a time with per-chunk logs and hashes. This is an observation tool using the existing MP4 route, not a 13.60 A53 exploit or a validated full-memory dumper. The copied transport still relies on transient x86 kernel/mailbox writes. Neither the source, token, ELF, nor raw captures are in this public fork. Its first live request requires explicit resumption, observed console recovery, a fresh current-boot receipt, exact binary hash and one stop-gated test. A package-trigger route remains a separate hypothesis; a dummy package does not itself establish A53 code execution.
