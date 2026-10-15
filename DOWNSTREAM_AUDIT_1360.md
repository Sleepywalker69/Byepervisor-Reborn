# 13.60 downstream binary audit

Current status: see **V5–V8 offline reconciliation and remaining functional gates** below. Historical sections retain their original evidence ceilings. Live work later resumed for bounded A53 observation and stopped on a process-read error; V7 was not rerun.

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

**Historical pause after V7 (superseded by the explicit 14 October resumption below):** no console contact, image upload, Rest Mode cycle, ELF send or live test until the operator explicitly resumes. The console's recovery state after V7 is unknown. The first resumed interaction must establish that state before selecting one hash-pinned test with a fresh attempt receipt, durable log, checkpoint, stop condition and recovery plan. An automated payload chain is not a qualification path.

**Offline A53 diagnostic preparation as of 13 October.** A separate private resident LAN probe had been built and host-tested, but had not been sent to a PS5. It distinguishes physical (`0x020000`) and EL3 virtual (`0x430000`) SDBGP reads, records the full read result fields, permits repeated serial requests on one boot, and stops on malformed transport responses. The host can preserve one bounded page at a time with per-chunk logs and hashes. This is an observation tool using the existing MP4 route, not a 13.60 A53 exploit or a validated full-memory dumper. The copied transport still relies on transient x86 kernel/mailbox writes. Neither the source, token, ELF, nor raw captures are in this public fork. Its first live request requires explicit resumption, observed console recovery, a fresh current-boot receipt, exact binary hash and one stop-gated test. A package-trigger route remains a separate hypothesis; a dummy package does not itself establish A53 code execution.

## First A53 toolbox live result (14 October 2026)

The operator explicitly resumed live work. A fresh current-boot V3 receipt passed its firmware, matching arm/completion, AP-complete, zero-hook and gate/IST checks. The private toolbox build `6b2f7944a1fe` (ELF SHA-256 `e3619272a372ba8d7f28b546a71a901fd790a45654e8dc7bb0a6092ef2be2256`) was **sent and executed once**. It reached its authenticated LAN listener; a `GET_CONF` command returned the retail `releases/13.60` version. Those checkpoints establish this toolbox and version command on one boot, not A53 memory access or an fPKG feature.

One 4-byte SDBGP read at the historical `0x88100000` anchor used the **EL3 virtual** selector `0x430000`, unlike the earlier physical-mode anchor attempt. The transport completed and the response envelope parsed, but the A53 read result was `status=0x20`, `transferred=0`, with no bytes. The status meaning is not established. No second address, page capture, A53 write, kstuff loader, or package test followed. `STATS` returned three transactions and three opens, and `STOP` returned `STOPPED`. The outer logging socket then reset without a final payload-return line. The operator reported the PS5 responsive at the home screen after the read and after STOP; that does not prove clean rollback. Raw logs and the toolbox remain private.

The published A53 code-execution proof of concept uses the MP4/DECI5S/SDBGP route, then resolves firmware-specific A53 virtual targets by direct virtual read or a physical page-table walk. It validates target bytes before patching, triggers through `GET_CONF`, and restores them. Its published target table stops before 13.60. Our working `GET_CONF` checkpoint satisfies only the version/transport part of that path. A successful 13.60 A53 read and exact image/preimages, or an independently validated alternative, remain open gates before a PPR selector can be considered. No fPKG mount or title execution was tested in this session.

## Further A53 transport and map evidence (14 October 2026)

A second, separate **executed** session used the same pinned toolbox and a fresh current-boot V3 receipt. A 64-byte EL3-virtual read at the prior historical anchor returned `status=0x20`, zero transferred bytes and no data, despite a parsed response and successful `GET_CONF`. This rejects only the narrow theory that the earlier four-byte access size caused that anchor's failure. The toolbox answered `STOPPED`, printed its exit checkpoint and closed its outer stream normally; the operator reported a responsive home screen and the elfldr port later accepted a connection. There was no A53 read success or code execution.

Offline inspection of the saved 13.60 x86 kernel identified its MP4 MDSR log-header checks. A private, bounded live header probe matched all expected signatures for two current-boot x86-visible log buffers. A distinct private capture payload then **executed once** after another passing V3 receipt. The host validated every chunk's CRC and contiguity, stable headers and clean exit before preserving the two logs privately. They contain the A53/MP4 boot layout, including a PSP-staged A53 ELF image segment at the historical anchor's numeric **physical** address and loaded EL3/EL0 mappings. Thus the historical anchor is present in the current boot's layout, but the failed EL3-virtual read did not target that segment as a verified virtual address. Earlier physical-selector reads also failed. The logs do not decode `0x20`, expose the A53 ELF bytes, or establish a usable SDBGP read. Raw logs, exact boot pointers and probe source/ELFs remain private.

An **offline source audit** of Michele-M-Media/HYPERHEN at commit `6f9b36eb2adc2fbaaf96366ede05cf0ca94870ce` found that its bundled A53/PPR target table ends at firmware 11.40. Its own integration document states that the supplied core has no exact 13.x A53/PPR profile and that 13.x end-to-end validation remains open. For 13.00, 13.20 and 13.60, the PPR bridge checks a Hypervisor readiness file and returns success while explicitly skipping unsupported PPR installation, before A53 transport setup and profile lookup. The vendored Byepervisor stage writes that file after its HV completion path. This is an HV gate and a PPR skip, **not** a published new 13.60 A53 exploit or functional fPKG proof. HYPERHEN was not built or run in this investigation. Its 13.60 ShellCore/Hypervisor compatibility data must not be conflated with an A53 profile.

A later **executed**, single-read session used the current boot's logged EL3 **virtual** base for the loaded segment, a fresh matching V3 receipt, and the same pinned toolbox. `GET_CONF` succeeded; the 64-byte SDBGP virtual read echoed the chosen address and request, but returned `status=0x20`, zero bytes. The toolbox answered `STOPPED`, printed its exit checkpoint and closed normally; port 9021 accepted a later connection. This rules out the specific explanation that earlier `0x20` results came only from using the physical staging address as a virtual address. It does not decode the status or establish a general 13.60 A53 access ban. No A53 write, kstuff load or fPKG test followed. Exact boot pointers and raw logs remain private.

The captured boot map also motivated a distinct, bounded **x86 MP4 aperture** hypothesis for the PSP-staged A53 ELF. A first private probe stopped before reading because it compared x86 physical and G6 addresses as though they were the same space. A revised one-shot probe checked page-table mappings and exact MDSR magic at two already verified offsets, then read only 16 bytes at the staged-image offset. The control checks matched, but the target returned sixteen `ff` bytes rather than an ELF header. This was an **executed failed hypothesis**; the host stream closed and port 9021 remained reachable, but no image bytes, A53 exploit, or fPKG result were obtained. The reason for the all-ones value is unknown. The original and revised private binaries/logs were preserved; no aperture scan followed.

## A53 image transfer boundary (offline review, 14 October 2026)

The archived public [mp4rw example](https://github.com/astrelsky/mp4rw/blob/cc8100fa711a8b55be602b777b25d2178ab044bd/example/main.c) reads candidate A53 memory through MP4/DECI5S, checks for ELF magic, and then creates `/mnt/usb0/mp4/a53.elf`. That path is an output file made by the dumper, not evidence of a pre-existing A53 image exposed by FTP. FTP could transfer such a file **after** a successful dump. The example does not establish that its older-firmware memory path works on 13.60.

Our bounded 13.60 A53 reads returned `status=0x20` and zero bytes; a separate x86 aperture header read returned all `ff`. Selected FTP directory listings and outer PUP extraction did not yield a plaintext A53 ELF. These results do not rule out every acquisition route, but no A53 image has been obtained. Do not equate filesystem access or x86 kernel read/write with A53 memory access, and do not run the older dumper unchanged as a 13.60 qualification test.

## A53 command matrix and MMU metadata (executed, 14 October 2026)

After fresh current-boot V3 receipts, private read-only toolbox revisions sent a bounded command matrix. Seven physical/EL3-virtual reads varied known current-boot regions and 1/4/8-byte widths; two later EL0-virtual reads used a third selector from the public `mp4rw` interface. Every read produced a matched SDBGP response envelope with raw `status=0x20`, zero transferred bytes and no memory data. The precise meaning of `0x20` remains unverified. Each resident session reached a clean STOP; that is not a full rollback or console-health proof.

A separate SDBGP `GET_MMU` query **executed** and returned 1,344 bytes of mapping metadata, reconstructed across six bounded chunks with matching full-response fingerprints. The parsed response has 28 records labeled EL1. This is real A53 command output, but it is neither private-memory content nor an exhaustive A53 map. A `GET_SYSHUB_TLB` request returned an unsupported command response. The raw map, boot pointers, logs, tokens and toolbox binaries remain private.

Because reads of identified mapped regions fail across those selectors and widths while `GET_MMU` succeeds, a broad private-address sweep is not currently an evidence-backed acquisition method. The next gate is one validated positive memory read or an independently justified alternate acquisition route. A53 image/profile extraction, 13.60 PPR patching, kstuff load health, mount, synthetic-key cleanup and homebrew fPKG execution remain unproved.

A subsequent **executed** same-boot comparison used a hash-pinned private toolbox to snapshot selected x86 MP4 state before a valid `GET_MMU`, after it, and after one denied four-byte physical `READ_MEMORY`. The metadata response matched the prior 1,344-byte fingerprint. The read returned a matched `0x20`/zero-byte result. Every sampled post-command field was equal except the expected request sequence increment. The sampled state/flags labels included aliases of the same underlying words, and all snapshots occurred after `FINISH`; this cannot distinguish transient host state or prove the denial originates in A53 firmware. The toolbox answered `STOPPED` and its outer stream exited normally. No host flag was patched, no A53 byte was acquired, and no fPKG function was established. Private pointers, raw logs, source and ELF remain outside this fork.

Follow-up **offline** disassembly of the saved 13.60 x86 kernel shows that the `FINISH` path unconditionally clears the two sampled MP4 state/flag words before V5's next `STATE` command. Their zeros therefore cannot test whether the metadata query and denied read differed while active. This does not decode the SDBGP status or justify changing a kernel flag.

One bounded **executed** follow-up captured selected x86 MP4 words after each response event but before the normal `FINISH` cleanup. A valid `GET_MMU` and a matched `READ_MEMORY` result with status `0x20`/zero bytes had equal sampled state, flags, counters and selected work fields; the request sequence advanced. The toolbox then completed `FINISH` and STOP normally. These samples do not cover earlier transients or all driver fields, and they do not identify the source of `0x20`. Skipping `FINISH` or forcing a success value has no evidence-backed basis. No A53 private byte or fPKG feature was proved.

A separate, single-read **executed** comparison copied the complete matched `READ_MEMORY` result packet before and after normal `FINISH`. All 152 bytes were identical and the result status was `0x20` at both checkpoints. This rules out `FINISH` creating that status in the tested exchange. It does not locate the earlier producer or prove a general A53 access policy. No kernel patch was applied. A reversible kernel change needs a specific producer-side branch or field and original-byte/readback/rollback evidence before it can be a meaningful test.

## Offline IDA 9.4 map before further A53 testing

The saved 13.60 x86 text and contiguous mapped-data captures were loaded into an **isolated** IDA 9.4 database. Whole-range IDA byte readback matched both original captures. The capture covers 13.37 MB of text and 87.88 MB of mapped data; the latter is non-atomic and ends at the first mapping gap. It is not a full kernel-memory or A53-firmware dump. The original captures and database remain private and unchanged.

IDA initially recognized 18,822 functions spanning 73.4% of captured text. Conservative recovery of already decoded, aligned frame-prologue boundaries and IDA's follow-on analysis produced 27,857 functions spanning 96.3%. This is a function-boundary inventory, not complete semantic validation. The remaining unassigned text includes padding, assembly and tables that are recorded in the private map.

The MP4 ioctl dispatcher and coredump entry were mapped in IDA. The dispatcher has separate START, ALTER_STATE and FINISH branches; FINISH clears selected host state and work fields. All 29 saved-text occurrences of the SDBGP READ_MEMORY request word and all seven occurrences of its result word were packet-construction stores, not identified parse or denial branches. Two visible x86 result constructors use zero or a high-bit-coded error form and do not explain the observed literal `0x20`. The coredump code builds packets for more than one DECI5S endpoint, including the toolbox endpoint, but its shown fixed-target read uses a different memory selector from the failed private-memory request. Thus matching packet framing does not establish permission to read A53 private memory.

The earlier **executed** 152-byte response already held `status=0x20` before FINISH and was unchanged afterward. The source and meaning of that status remain open. The x86 capture contains A53 manager/storage code and diagnostic strings but yielded no validated plaintext AArch64 ELF header. A similarly named first-image routine copies storage sectors; its name is not evidence that the A53 firmware image was captured. No A53 image, 13.60 PPR profile, private-memory byte, healthy kstuff load, fPKG mount, synthetic-key cleanup or homebrew title execution was proved by this **offline** mapping. A new live read or kernel patch needs a specific, falsifiable producer-side hypothesis; another broad address sweep is not supported by current evidence. Private addresses, raw listings, captures, hashes tied to private binaries and the IDA database remain outside this fork.

## Separate A53 manager and storage routes (offline IDA follow-up)

The 13.60 x86 kernel registers distinct `a53mm`, `a53mmsys` and `a53io` character-device names. The `a53mm` ioctl path submits indirect commands through an A53 manager input ring and signals a controller register. This is a real interface separate from the MP4/SDBGP read path. Its open handler requires an active manager record for the calling process; command submission checks the manager state, ring capacity, lock ownership and completion state. The command vocabulary and returned data have not been validated as a private-memory read. Its presence does not mean MP4 is patched or that A53 firmware bytes can now be dumped.

The `a53mmsys` interface is credential-gated and handles manager status, page-table/privilege/shared-memory operations and a halted-ring debug snapshot. Those mapped operations have range and state checks; the debug snapshot concerns command metadata, not an A53 image. `a53io` is a separate storage/package controller route. Its direct SCF command handler applies a mode-dependent opcode/option allowlist before queue submission; package block read/decrypt and program-selection functions also exist. The selected program is a storage-controller program, not a recovered A53 firmware image. No inspected branch establishes arbitrary A53 private-memory access.

These **offline** findings identify the A53 manager command ring as the next path to classify, and A53IO's package path as a separate place to investigate PPR/key behavior. Before a live test, an exact read-only command, response format and expected byte-level result still need evidence. No console action or new payload occurred in this follow-up. No 13.60 A53 image/profile, private-memory read, kstuff health, mount, key cleanup or homebrew fPKG execution was proved.

Further static inspection of the A53MM indirect-buffer sender found a bounded ring selector, nonzero buffer address and length, a constructed three-word command and optional completion handle. The mapped function submits a command buffer; it is **not** a demonstrated private-memory reader. Manager-status and mapper metadata ioctls return host-side state, not A53 firmware bytes. The command vocabulary and a safe, byte-verifiable response remain the gate before any live use of this alternate path.

The offline process-lifecycle trace found that A53MM registers an eligible application record only after app-data and manager-state checks, then requires the matching process record when opening the interface. Its completion callback matches replies to a ring and reserved future. The saved x86 kernel does not disclose the caller-supplied indirect command vocabulary. A literal scan of locally saved executable files found no `a53mmsys` client marker; that does not exclude dynamically constructed names or unsaved/encrypted clients. This is a focused call-path map, **not** a claim that every A53-related function has been semantically mapped. The next useful input is an actual userland client, A53-side protocol/image, or a captured byte-level command/response pair. A stand-alone dummy interface would exercise host gates without proving A53 private access.

## Expanded A53 and package-key IDA census (offline)

A second pass indexed all A53-tagged printable strings in the **saved x86 kernel capture** and their IDA references, then separately indexed PPR, PPFS, KMS, SCF and SDBGP neighbors. It reached 443 A53-referencing x86 functions after excluding autogenerated-name false positives: 224 with A53MM tags, 180 with A53IO tags and 39 other references. Direct call edges, data references and unresolved indirect call sites are preserved privately. These counts measure lexical and call-graph coverage, **not** semantic validation of every function or any A53-resident code. The capture still has no verified A53 firmware image or 13.60 profile.

The saved data contains the filename string `a53.elf`, but IDA found no direct xref to it and an exact pointer scan of the saved ranges found no reference. A filename string is not an extracted image or an active acquisition path. Representative other A53 references mapped to ring DMA setup, SBL XTS dispatch and storage error handling, not a demonstrated private-memory reader. A supplied A53MM command buffer and A53-side response format remain missing from this local collection.

The PPFS path gives a more specific future **key lifecycle** check. Its G6 metadata setup can acquire XTS and CMAC KMS indexes for an A53IO package context. Its unmount and selected mount-failure paths call a cleanup routine that clears and releases those indexes, recording each successful cleanup separately. A separate PPFS decrypt path temporarily acquires an XTS index, submits an async A53IO decrypt request, checks completion and releases that index. This is offline x86 control flow. It neither identifies the earlier observation build's outstanding synthetic key pair nor proves a successful live mount, cleanup or title execution.

The next IDA work is limited to unresolved command-buffer producers, indirect dispatch and completion semantics that a real client binary, saved command trace or exact A53 image can disambiguate. A dummy `a53mmsys` client and blind private-address sweep are not supported by this evidence. No console test, kernel change or new payload was made for this census. fPKG acceptance still requires a healthy loaded kstuff, ShellCore patch readback/rollback, runtime PPR/import behavior, cleaned key pairs, a verified A53 13.60 mechanism or independently justified alternative, and actual known-good homebrew fPKG mount and execution.

A focused review of representative indirect-call hubs found a manager state formatter calling its print callback, privilege/mapper helper calls, and storage-controller operation-table calls. The A53IO async decrypt command and its completion branch can be connected by the same operation number in x86 code. That narrows future package-mount and key-cleanup observations but supplies neither a private A53 read nor proof of a successful decrypt. The remaining unknown indirect targets cannot be treated as discovered A53 entry points by count alone.

The **available saved x86 capture** has now been mapped in IDA at the relevant interface, transport, completion and PPFS key boundaries. It is not a full semantic map of every x86 function, every kernel page, or any A53-resident image. Further IDA work needs a new input: a real A53MM client or byte-level command trace, a verified exact-13.60 A53 image, or a positive preimage-checked private read. The next live qualification sequence must first resolve the guarded kstuff loader's inner failure and restoration evidence, then capture PPR/import, mount/decrypt and key cleanup on one controlled package attempt. Only a known-good homebrew fPKG that installs, mounts, reads, launches and actually runs establishes the end goal. No console operation occurred for this offline pass.

**Offline route correction:** a saved client binary or A53 image is not required to *attempt to observe* genuine A53MM traffic. The mapped x86 manager process record, input-ring writer and completion callback give candidate checkpoints for a bounded resident diagnostic: first measure owner/ring activity during one normal action; if real traffic appears, capture a size-limited submitted buffer and matching completion through a separately reviewed reversible tracer. This is a proposed x86 observation route, not a built or executed trace, known safe command, private A53 read or 13.60 image. The existing LAN toolbox already supports multiple commands per session and returned A53 `GET_MMU` metadata, while its sampled private-memory reads returned `0x20` and zero bytes. A53IO package submit/completion and PPFS key cleanup are a separate later observation path. No dummy client, blind address sweep, speculative kernel patch or fPKG qualification follows from this correction.

**Executed x86 observation update:** A private, bounded resident observer reached an authenticated ready state on retail 13.60 and sampled A53MM ownership and A53IO ring fields. At idle it found an initialized manager but no current owner or application record. A baseline of the x86-mapped A53IO ring was saved privately; it is not A53 private memory or a firmware image. The first observer listener later stopped accepting connections after a host disconnect, with no proven cause. A revised observer was built and executed once on a fresh current-boot receipt, then reported an active watch with zero read and accept errors. Its host client is prepared to capture a genuine owner process's readable map/header pages and bounded x86 ring changes during one normal app launch. No such launch-triggered client or command trace has yet been observed. These observations do not establish PPR behavior, a 13.60 A53 image/profile, kstuff health, fake-package mount, key cleanup or homebrew execution. Private binaries, raw ring bytes, logs, pointers and boot identifiers remain outside this fork.

### Native game launch A53MM observation (executed, partial)

A native PS5 game launch and two operator relaunches produced a genuine A53MM owner/application record and activity in its x86-mapped application input ring. IDA review of the retail 13.60 x86 kernel matched the observed repeating three-word records to the indirect-buffer submission path. The first two settled ring samples each contained 341 wrapped indirect-buffer records encoding seven 32-bit elements (28 bytes), referencing 25 readable process buffers; the addresses changed completely between the two process instances. The original host reader mistakenly treated that element count as a byte count and captured only seven-byte prefixes. This is a useful live command-source lead and confirms that the earlier idle owner result was specific to the idle state.

A bounded host trace over the already resident read-only observer then followed a ring-generation reset and new owner during the next launch. It saved 34 ring snapshots and stopped normally at its 256-buffer-read cap. For the new owner it read 204 seven-byte and five eight-byte prefixes from descriptors that encode 28- and 32-byte buffers respectively; eight other process reads returned errors. The same address held different later values, and many ring snapshots raced with writes, so these bytes are **not an atomic or complete record of what A53 received**. The x86 ring status was sampled, but no matching completion payload or response status has been verified. The observer reported zero watch and listener errors after the capped trace. No A53MM command was submitted by our tooling, no kernel patch or A53 private-memory read was made, and no A53 firmware image was recovered. The next discriminating gate is a byte-level submitted-buffer and matching completion pair, then client identity and A53-side semantics; the trace alone does not establish a 13.60 PPR profile or fPKG support. Raw buffers, addresses, ring captures, logs, private tooling and boot identities remain off this branch.

### Native executable process capture (executed, analysis partial)

A guarded read-only observer listed the running native game's executable process mappings. One mapping matched the executable segment size in its mounted SELF's embedded ELF header. Page-limited reads captured that full loaded x86 text segment; two reads initially failed but succeeded in bounded later checks, and the resumable capture completed with process ownership checks. Three other mapped ELF file-data segments were captured separately. This is stronger input for identifying the userland A53MM command producer than the earlier kernel-only map, but it does not establish which instruction submitted any observed command.

The remaining ELF load segment contained the original dynamic metadata but was not mapped in the complete process VM-map response. A private synthetic ELF for analysis therefore contains only the four captured load segments, omits imports and dynamic metadata, and includes runtime writable state. It is **not** a complete decrypted game executable. A bounded literal scan of the captured ranges found no explicit A53MM device-name string; that result does not exclude generated names, indirect imports or code in other modules. No A53 firmware bytes, 13.60 A53/PPR profile, healthy kstuff load, fake-package mount or homebrew fPKG execution was proved. The game bytes, synthetic ELF, private tooling, exact pointers, receipts and logs remain outside this fork.


### Separate decrypted native game reference (offline)

A second native title's decrypted eboot was inspected privately as a caller-side reference. Its ELF program segments, dynamic string/symbol tables and relocation metadata are present, but its advertised section table lies beyond the dumped file; analysis used a separate header-normalized copy while preserving the original. Its import list includes ordinary game and content libraries, with no named A53/MMsys dependency or literal. Five small app modules likewise named no such dependency. Sony NID-encoded imports and system-library indirection limit this negative result. This title was separate from the game whose A53MM x86 traffic was traced, and no simultaneous command/completion pair was captured. The full dump, modules and analysis copy remain private. This finding does not establish an A53 image/profile, healthy kstuff load, fake-package mount, or homebrew fPKG execution.


### Loaded A53MM client wrapper located (executed, bounded)

On a separate native game launch, a fresh current-boot receipt and the existing read-only observers identified the active A53MM owner and listed its loaded x86 modules. The eboot's map span matched the decrypted ELF's mapped load-segment span. A corrected host parser treated one 13.60 dynlib field as a segment pointer after an earlier size-field assumption failed; both attempts retained their logs. An execute-only system-library read succeeded for a small content library and found no direct A53MM ioctl literal there. A capped scan completed 19 loaded system-library code ranges before stopping on a process-read error in the twentieth, without retry. The completed `libkernel` code contained wrappers for the kernel-mapped A53MM indirect-submit and future-wait ioctls. IDA review cross-checked those wrappers and their syscall path. This identifies an x86 userland API surface, **not** the game call site, submitted buffer at call time, matched completion, A53 private image, or 13.60 PPR mechanism. The temporary reader reported `STOPPED` and its port closed; its outer exit line was lost on a connection reset. Offline review of the loaded wrappers showed that their byte-length argument is shifted right by two before the ioctl; the ring count therefore denotes 32-bit elements. Three saved process pages contain longer later-state examples, but still do not prove command-time bytes. The historical host trace and raw logs are preserved, and a corrected host-only reader has been syntax checked offline without running on the console. No package or fPKG test occurred. Raw module bytes, process addresses, receipt, logs, analysis ELF and IDA database remain private.

### Syscall-boundary tracer trial (executed, unsuccessful)

A private, bounded x86 syscall tracer was built to capture an A53MM submit buffer at ioctl entry. Its listener reached an authenticated current-boot ready state. A brief attach/register-read/detach check returned success, but no A53 ioctl was captured in that check. An arm-before-launch variant detected a native game's A53MM owner and attached; its first subsequent stop was unexpected, and the game froze. The operator manually closed it and the home screen remained responsive. A revised variant logged an initial `SIGTRAP` and detached with signal zero before issuing any syscall-trace request; the game still froze and was manually closed. Its detach success response did not establish game health.

These attempts captured **no** call-time A53MM buffer, matching completion, A53 private bytes, or 13.60 A53/PPR profile. The exact cause of the freezes is unresolved, so the ptrace route is suspended pending offline analysis. Do not infer fPKG support from the listener, attach check, or socket responses. Private ELF files, source, raw logs, process details and boot identifiers remain outside this fork.

### Kernel-side A53MM recorder preparation (built, not sent)

A private generic recorder core compiles as a freestanding x86-64 FreeBSD object and passes host simulations of submit-buffer capture, future/wait pairing, bounded truncation, pass-through and capacity drops. A read-only verifier matches the A53MM ioctl slot, jump stub and target in the **saved** 13.60 kernel capture. A separate PS5-loadable recorder ELF and rollback ELF have now been **built**. The recorder hook's machine code passed host simulations; the loadable ELFs have not run on a PS5. The candidate includes live firmware/preimage checks and a bounded automatic stub restore path. At the attempted handoff, elfldr and FTP were not listening, so neither ELF was sent or executed. The candidate would observe caller bytes before forwarding genuine ioctls; it does not issue an A53 command or read private A53 memory.

The loadable candidate still needs a fresh current-boot receipt, live placement/preimage/permission checks, actual hook and restore readback, a durable captured log, and evidence that the game remains healthy. Its zeroed kernel-data candidate is not a proven reserved allocation; live corruption or a crash remains possible. The existing kstuff loader has unresolved health and rollback gates and is not assumed to supply this mechanism. A future-wait return is also not an A53 completion payload. No call-time buffer, A53 image/profile, PPR result, fPKG mount or homebrew execution was proved by this offline preparation. The private source, ELFs, raw captures, exact pointers and logs remain outside this fork.

### Older A53 firmware recovery as a protocol reference (offline)

The public [a53-RE project](https://github.com/seregonwar/a53-RE) provides DWARF-derived A53 controller layouts and generated semantic source from an older reference image. Its source-tree path names a release label, but the inspected repository does not establish retail 13.60 identity or provide a verified 13.60 A53 image. A four-byte command-header layout and an indirect-buffer location structure are consistent with aspects of the saved 13.60 x86 ring framing. This makes them useful **candidate schemas** for interpreting a future call-time capture. The generated source also points to indirect-buffer handling and package bind/decrypt code for comparison. None of this validates 13.60 A53 semantics, supplies a PPR exploit/profile, or demonstrates fPKG execution; older addresses and patches are not transferable.

### pLinux5-NG kernel-memory reference (offline)

The public [pLinux5-NG project](https://github.com/pLabs5/pLinux5-NG) was inspected at commit `3a7ea6f6c874773641b21d20f95f5df6f648ac40`. Its development offset tool reads kernel memory with SDK `kernel_copyout`, derives the x86 kernel direct map from the kernel pmap, checks the first text mapping's XOTEXT state, and records actual lengths while dumping bounded mapped text/data ranges. It also probes fixed hypervisor physical candidates through the direct map after the S3 escape. This is useful as an independent kernel-memory and page-table reference, but it is not an A53 private-memory reader or a full memory census. Its current high-firmware offset selection contains 13.20 and no 13.60 entry; the README explicitly calls support work in progress. Its device-firmware dump targets WiFi. Our existing 13.60 kernel text/data capture remains limited to the mapped ranges actually read. This reference adds no executed 13.60 A53, PPR, kstuff, or fPKG evidence.

### First kernel-side recorder trial (sent; operator reported KP)

After a fresh V3 state receipt passed, the exact private recorder ELF was sent once through elfldr. The host received no preflight or armed checkpoint before its bounded timeout; the durable host output file is empty. The operator reported a kernel panic, and subsequent service connections timed out. The ELF must not be resent. No A53 call bytes, hook restoration, or fPKG behavior were observed.

Offline source comparison identified a strong but unconfirmed cause: pLinux5-NG checks execute-only kernel-text mappings before reading and changes their page permissions before patching, whereas the private recorder attempts to read its text stub before such a check and later writes text without a guarded permission transition. Earlier 13.60 leaf surveys found the relevant text spans execute-only/read-only, and the current Byepervisor fork leaves text mappings untouched. With no console checkpoint or crash trace, the exact faulting instruction remains unknown. Further live recorder work is stopped pending an offline design that gates every text access and proves reversible page-permission and hook cleanup. This incident establishes no A53 private-memory access, PPR profile, or fPKG execution.

### Protected-text recorder revision (offline and built, not sent)

A separate private v2 recorder and recovery ELF now compile with the pinned PS5 SDK. The original v1 binaries remain preserved. The revision checks the current direct-map/page-table relationship and the complete target range before each kernel-text access. It accepts only an expected execute-only, read-only text leaf, makes a bounded permission change with PTE readback, verifies byte preimages and write readback, and restores the exact original leaf entry. An on-console checkpoint file is opened before kernel access and fsynced around risky transitions. The sender requires an explicit post-panic resume option. Host page-table and unchanged hook tests passed; manifest hashes match the built artifacts.

This is a source-level correction to a plausible v1 failure path, not a diagnosis of the kernel panic or a live safety result. Cross-core translation/instruction coherence, the live five-byte jump change, unreserved record storage, hook health and cleanup remain unproved. Neither v2 ELF was sent or executed. Live recorder testing remains stopped until an explicit resumption and a single controlled current-boot attempt with durable checkpoints and independent restoration checks. No A53 image, 13.60 PPR profile, fPKG mount or title execution follows from this build. The private source, ELFs, exact pointers and raw logs remain outside this fork.

### External 13.60 HV/A53 repository review (offline)

The public [HV-Bypass-13.60 repository](https://github.com/NotSoTypicalGamer/HV-Bypass-13.60) was reviewed at commit `0cc133c423276df2fecdb367990928c5417b5265`. Its 13.60 offset header matches this fork's header, but its bundled Byepervisor source still performs broad kernel-text permission changes and an unconditional text write; this fork's guarded receipt path remains the relevant local candidate. The repository's A53MM audit is explicitly strings-only. A later A53 note reports low ROM read values but also reports failed A53 image/DRAM reads, writes that did not land, and an incomplete direct mailbox attempt. The included A53 reader does not validate the full response envelope or exact transfer length. Those claims do not establish A53 code execution, a 13.60 PPR profile, or fPKG support.

The reported 28-entry `GET_MMU` map agrees on selected rows with our independently validated, receipt-gated 13.60 metadata capture. This corroborates a mapping observation, not private image bytes. The functional gates remain a healthy kstuff load, applied ShellCore patch/rollback and import/PPR evidence, key-pair cleanup, exact A53 evidence or a justified alternative, then an actual homebrew fPKG mount and execution.

### Strict low-ROM cross-check (sent and executed)

After a fresh passing current-boot V3 receipt, an archived private V2 toolbox ELF (SHA-256 `9aab8b7317687541ab280012a97c6da0e4a31603a2e42e853984220d86c355c9`) was sent once. Its authenticated `VERSION` returned retail 13.60. One `READ PA 0x0 8` returned a matching response envelope, transport result 0, status 0, stage 6, eight transferred bytes, and `00 1a 05 00 10 00 01 00`. These are the little-endian bytes of the value reported by the external repository for PA 0. `STOP` was acknowledged, the outer payload exited, and elfldr/FTP ports remained reachable. The private raw stream and fsynced command log are retained outside this fork.

This independently confirms **one eight-byte A53 physical low-ROM read on 13.60** through the strict toolbox. It does not validate the external project's broader assertion of A53 code execution. It supplies no A53 DRAM/image bytes, writable primitive, PPR profile, kstuff health, key cleanup, fPKG mount, or title execution. The private v2 kernel-side A53MM recorder remains unexecuted; its live text-hook and storage risks are not resolved by this read.

A second fresh-receipt test with that same ELF captured one 4 KiB PA `0x1000` low-ROM page in 64 bounded reads. Offline reconciliation of all saved responses confirmed exact addresses, command types, zero status, full transfers, consecutive response IDs and sequences, and byte-for-byte agreement with the saved page. Its first eight bytes matched the external report's second value. `STOP` and outer exit were observed; elfldr and FTP remained reachable afterward. An analysis-only wrapper was loaded by IDA Pro 9.4, which matched the full page bytes at address `0x1000`, checked RX mapping, and decoded the first two AArch64 instructions. The private ROM page, synthetic ELF, IDA database, raw logs and receipts remain off GitHub. This is **one verified ROM page**, not a complete ROM or the DRAM-staged `a53.elf`; the fPKG gates above are unchanged.

A separate offline IDA operand export found direct calls from that page into PA `0x2000–0x2fff`. A third fresh-receipt test with the same archived toolbox captured that one adjacent 4 KiB low-ROM page through 64 bounded reads. Saved-log verification found zero status, full transfers, matched per-command metadata, acknowledged `STOP` and outer exit. It is another **verified low-ROM page**, not a private A53 image or broad map. Raw bytes and analysis files remain private.

To answer whether reads can be batched like the earlier kernel capture, a separate private toolbox candidate exposed the transport's existing multi-command SDBGP framing as a bounded read-only LAN command. Its strict build and host mock passed. On a fresh same-boot V3 receipt, the exact candidate ELF was **sent and executed once**. One packet containing 16 physical read commands of four bytes each returned the exact 64-byte prefix of the already verified PA `0x1000` page; the transport request counter rose by one. `STOP` and outer exit were observed. This proves a **single packet with 16 validated reads for that low-ROM range**. The tested packet does not grant access to A53 DRAM, `a53.elf`, or a full memory sweep. The private candidate ELF embeds an authentication token and remains off GitHub. The outstanding kstuff/PPR/key/fPKG functional gates are unchanged.

### Bounded low physical-page map and IDA alias review (executed/offline)

Further fresh-receipt sessions captured one more 4 KiB page and then used a separately built, read-only batch candidate to capture 188 additional complete pages. The saved responses were checked for matching metadata, zero status and full transfer page by page. The resulting private contiguous range is PA `0x1000–0xbffff` (191 pages, 782,336 bytes). PA `0x0` still has only the separate eight-byte result; the intervening bytes were not filled in. The first request at PA `0xc0000` returned `status=0x20` and zero bytes, so the run stopped without retrying or skipping ahead. This observation does not establish a physical ROM end or an arbitrary A53 memory-read capability.

An analysis-only ELF loaded the complete range in IDA Pro 9.4, and IDA verified its bytes. A second private IDA view used the previously observed 13.60 `GET_MMU` physical-to-virtual aliases to resolve direct calls between captured controller-code regions. That map was recorded on another boot, so it is a static analysis hypothesis for this capture. IDA finds a controller/ring call graph and bounds-checking routines, but the capture still has substantial unmapped callees and does not identify the producer of the private-memory `status=0x20`. Sparse data and zero pages were not treated as firmware functions. No verified `a53.elf` header, exact 13.60 PPR profile, or package-auth bypass was recovered from this low range.

The raw pages, source, token-bearing payloads, IDA databases and detailed logs remain private. No further console command was sent during the IDA analysis. The functional gates remain a healthy kstuff load, ShellCore patch/readback and rollback, live imports/PPR behavior, key-pair cleanup, exact 13.60 A53 evidence or a demonstrated alternative, and actual mount and execution of a known-good PS5 homebrew fPKG.

### Focused A53 command-copy audit (offline)

The captured controller code contains a ring-copy helper that checks source availability but receives no destination size. An offline review of its 30 identified direct call sites checked header-derived word counts against local stack areas. An apparent overwrite in two command handlers was ruled out after evaluating the exact ARM carry/zero flags: their branch accepts only one or two words, which fit. One generic helper still has a conditional capacity concern if an upstream caller can supply more than eight words, but the captured code has no direct call edge or table entry establishing such reachability, and the source ring may reject the length. This is a hypothesis to resolve, **not** a discovered 13.60 exploit. Exact addresses, disassembly, raw bytes and working files remain private.

The next static gate is the exact 13.60 dispatch/caller constraint for that helper and the corresponding source-ring length rule. The required dispatch data lies outside the verified low physical capture; an older A53 source tree cannot replace it. No payload or live action followed this audit, and none of the PPR, kstuff or fPKG functional gates changed.

A cross-check of the saved 13.60 x86 A53MM indirect-buffer sender shows that its **entry count check** accepts nine words, so that particular host gate does not itself enforce an eight-word maximum. It does not prove an A53 handler will accept that header, select the conditional helper, or copy those words. Manager and ring gates also remain unqualified for this case. The hypothesis stays offline and unexploited.

### Targeted A53 dispatch-data access check (offline mapping; sent/executed read)

The saved 13.60 A53 boot log locates the missing dispatch data within loaded IO-controller text. That identifies a specific mapped target, but the log came from an earlier boot and supplies no table bytes. After a fresh passing current-boot HV receipt, the existing hash-pinned private toolbox executed and issued **one** aligned eight-byte physical read covering the mapped table start. Its response envelope matched the request, but SDBGP returned `status=0x20`, zero transferred, and no bytes. STOP was acknowledged and the outer payload exited. The private plan, receipt, raw stream, and fsynced command log are retained off this fork.

The table and the conditional copy helper's caller restrictions remain unknown. This test does not decode the status, prove the prior mapping held on the test boot, reveal a 13.60 A53 exploit or PPR profile, or advance the kstuff/fPKG functional gates. Further adjacent address guesses through this selector are not supported by the result. An exact 13.60 caller edge or validated alternate image/trace path is needed before another exploit hypothesis can be tested.

Further offline comparison identifies the conditional helper's **likely role** as a generic MM-controller pipeline allocator: its argument registers, header-count copy, state-size conversion and allocator return test match the older recovered implementation. That older source is not a 13.60 symbol map. The failed mapped-data read targeted a table used by a separate visible dispatcher; its bytes alone would not establish a call to the generic helper. The required proof is a 13.60 branch or callback edge, its count/ring preconditions, and an attributable runtime entry observation for a genuine command. No additional console command followed this comparison.

An offline AArch64 address-materialization scan found no helper-address or helper-page reference in the captured executable alias ranges; it cannot inspect the uncaptured high controller image, indirect pointers, or runtime-computed edges. Exact low-code windows also had no copies in the saved x86 captures or original/outer-decrypted PUP files. Because those windows do not identify the high image, this is a narrow negative search, not image absence proof. Matching-build caller/callback bytes and a separately safe A53-side entry observation are still needed before any claim that a command reaches the helper. No exploit or downstream fPKG capability is established.

The earlier observation that the x86 indirect-buffer sender accepts nine elements is **not** a nine-word outer-header witness. Exact 13.60 sender instructions store that length minus one in the third descriptor word while the first word's header-count field is fixed at two. If the generic helper processes that outer header, its copy from this producer is bounded to two words. The actual A53 caller edge, possible inner-buffer processing and other command producers remain unknown. This correction leaves the conditional concern unproved and supplies no reason to issue a crafted command.

The saved boot-log layout also corrects the earlier x86 aperture interpretation. The tested x86 offset is associated with reserved controller data; the PSP-staged A53 ELF is listed in a separate G6 range. Its 16 `ff` bytes were therefore not a validated ELF-staging read. The log does not provide a verified linear x86 alias for the missing high controller text. The next useful client-side evidence is a genuine command-time A53MM buffer/header and matching completion, acquired with an observer whose installation and rollback are independently safe. That trace would still need a matching A53 caller edge or A53-side entry observation to prove this helper ran.

The existing recorder core is host-tested but has no validated 13.60 executable kernel allocation or installer. Its V2 live candidate still edits a five-byte text jump and uses unreserved storage. A device-switch data-pointer observer is only a design until executable placement, indirect-call behavior, pointer restoration and code lifetime are verified on 13.60. No replacement ELF or live capture is established by this review.

A new offline pass on the saved 13.60 kernel identified allocator and page-protection function candidates by their callers, arguments and page-table control flow. The A53MM device registration passes its static device-switch record to device creation; the generic ioctl dispatcher later reads that record and makes a five-argument indirect call through its ioctl member. Exact captured bytes and direct edges passed a private hash-pinned checker. This supports the data-slot observer design in the saved binary. It does not supply a kernel-call primitive, a live executable allocation, an atomic compare-and-swap, or safe reclamation after an in-flight call. The recorder remains uninstalled.

An offline correction to this fork's unused `kh_chain_call` helper adds a conditional return gadget so a target reached through the existing hook chain has the expected x86-64 stack alignment. Eight host contract cases passed, including the full S3 handler prefix and hook image capacity; the required gadget bytes match the saved 13.60 capture. At that checkpoint this had **not** called a kernel function on PS5. The demonstrated ring-0 path is the S3 wake hook; the hook is restored after AP completion. The optional post-wake test path changes the global debug-exception gate while other cores are active and is not qualified as a safe call service. At that checkpoint a short, bounded call-result smoke had to precede any allocator call; an allocator's deeper VM/page operations need separate context and stack analysis. Private host-tested compare-and-swap source now covers the exact expected-value install/rollback logic, but it is not linked to a PS5 payload or installed. Executable allocation, live slot writability, atomic redirection, restoration and wrapper lifetime remain open.

An opt-in `CALL_PROBE_1360=1` build now places one eight-byte copy through the saved 13.60 `memcpy` candidate (`text+0x2590b0`) after the existing AP carrier setup in the S3 wake chain. It requires exact firmware `0x13600007`, zero preimages for four reserved cave qwords, and readback of the source sentinel before arming. After AP completion it restores the hook, writes the attempt receipt, reads source/destination/return/done qwords and records them in the stage log. `diagnostics/call_probe_status_1360.c` independently checks those qwords with the V3 receipt, current boot identity and restored gate/IST. The opt-in ELF (SHA-256 `362eb19e6e1d5f18b24fe42c420ad2f6f3a4e72b7e587b62ea70cca0f35c922f`) and reader ELF (SHA-256 `40303a01e6cedf9d74cbab203a5dc36751699fd914c8f929c50103805171ea14`) **were built offline and were unsent at that checkpoint**. No allocator call, A53MM hook, or fPKG behavior is established. At that checkpoint the user-reported Relapse+Byepervisor boot was kept for read-only work; a changed S3 wake-hook attempt needed a fresh Relapse-only boot because the receipt/state guards refuse to re-arm an already completed attempt.

A later private **executed, read-only** diagnostic checked the A53MM
device-switch slot after a current-boot V3 receipt. Its original pointer
matched the saved 13.60 relocation, and the slot's guest page was present
and writable. This narrows the installer preimage gate but does not
demonstrate the dispatch, a compare-and-swap, executable allocation,
rollback or a working recorder. Raw addresses and logs remain private.

HyperHEN's bundled syscall-`0x11` `kexec` routine belongs to its legacy
1.xx–4.51 engine, not its 13.60 launch path. The saved 13.60 syscall
entry is marked absent, and the old exact indirect-jump gadget had no
instruction-aligned match in the analyzed text. It is a source reference,
not a post-wake 13.60 caller. MM PS5 API Mapper informed a separate,
bounded read-only diagnostic that was **sent once and executed** after
a valid current-boot receipt. The bus list ended consistently at 123
nodes; its filtered matches included `sbl0` (`Oberon PSP`) and `mp40`.
Thirty-two names were unreadable or invalid, and no bus method or A53MM
ioctl was invoked. This live x86 topology observation does not prove an
A53 helper entry or close the missing kernel caller, executable allocation,
atomic recorder installation, A53 image/profile or fPKG execution gates.

The saved 13.60 allocator's observed `0x102` flags permit waiting; its
page-shortage branch can call `vmwait` before retrying. That call is not
qualified for the S3 #DB ROP stack by the then-unsent leaf `memcpy` probe.
A separate read-only, receipt-gated carrier check was **sent once and
executed**. It found the expected physical page mapped with NX clear,
but the byte comparison against the pinned AP code failed at its first
byte. The current bytes were not captured, so the cause is unknown.
No retained callable service, executable allocation, A53MM dispatch,
atomic pointer swap or recorder installation was demonstrated. The
next S3 attempt required a full restart to a fresh Relapse-only boot;
manually clearing the completed receipt would bypass its safety guard.

### S3 ring-0 leaf-call qualification (sent/executed, bounded)

A fresh 13.60 Relapse-only boot received the exact opt-in Byepervisor ELF SHA-256 `362eb19e6e1d5f18b24fe42c420ad2f6f3a4e72b7e587b62ea70cca0f35c922f` once. It armed and entered Rest Mode automatically. Its initial socket timed out before wake, so the arm output alone was not used as completion proof. After wake, the pinned read-only status ELF SHA-256 `40303a01e6cedf9d74cbab203a5dc36751699fd914c8f929c50103805171ea14` executed once and matched the current-attempt V3 receipt, restored gate/IST, copied sentinel, destination return pointer and done marker. A separate read-only log reader executed once and recovered a durable AP-complete cleanup and exactly one matching call-probe row. This functionally proves the bounded `memcpy` call/return and result storage for this S3 attempt. Private raw streams and boot-specific identifiers are retained outside the fork.

The exact tested opt-in ELF contains a private build path in debug metadata and is not published. Neither `kmem_malloc` nor a recorder ELF was sent. The S3 #DB scratch stack does not yet qualify the allocator's observed waiting path; the normal-context call, executable page allocation/readback/entry, A53MM live dispatch, atomic compare-and-swap/rollback, wrapper lifetime, 13.60 A53 path, PPR/key lifecycle, and known-good fPKG mount and actual title execution remain open.

### Post-wake kernel call context (offline)

Pinned 13.60 text/data and IDA review located the ordinary syscall dispatch call/return on a kernel thread stack. The dispatcher checks its indirect target against 934 eight-byte CFI stubs. In the saved state, an out-of-range target reaches the fatal check path. Syscall `0x11`'s existing stub is shared by hundreds of PS5/PS4 entries, so redirecting that stub is not an isolated caller. A table-unreferenced stub is a candidate to study, not ownership or safe execution proof. The old HyperHEN legacy caller remains unsuitable for direct transplant.

The retained **executed** 13.60 kstuff observation payload was compared byte-for-byte with local function slices: its `r0gdb_kfncall`, `r0gdb_kmalloc` and block-allocation loop match, and its call sites reach the 384 small-call loop and 4 MiB request. This strengthens the historical path attribution but does not recover an allocation address, page readback or clean uninstall. The path uses regular `malloc` and the global r0gdb debugger setup, while separate setup/cleanup probes failed on 13.60. A53MM `d_ioctl` would provide an ordinary syscall-thread callback only after a validated executable wrapper has been staged independently. No safe post-wake arbitrary caller, allocation test, recorder install, PPR/A53 mechanism or fPKG execution is established by this offline pass.

A saved-binary search also traced syscall 165's `I386_SET_IOPERM` command into the identified allocator on the ordinary syscall stack. It calls with a fixed 0x8000 size and waiting flags only when a thread I/O bitmap is absent, then retains the result and changes TSS state. Its return is a status code, and there is no small rollback path in that helper. It is therefore a corroborating call edge, not a safe allocation-test entry. No `sysarch` call was executed during this review.

### Normal-stack marker and firmware archive cross-checks (offline)

A narrower saved-byte candidate for ordinary syscall-stack execution uses an existing no-argument, static syscall record and an existing CFI-approved leaf that returns a fixed error. It would require a one-byte call-pointer change, exact same-boot preimages, readback, one invocation, compare-guarded restoration and a durable checkpoint. This candidate was **not built or tested** and temporarily changes that syscall for all callers. It provides no allocator or A53MM execution proof.

The public `zecoxao/ps5-pup-unpacker` was run offline against the already saved outer-decrypted 13.60 PUP. It reproduced the earlier `kernel.bin` byte-for-byte and exposed two readable exFAT images. A separate read-only directory walk enumerated 769 files in `system_b` and 2,444 in `system_ex_b`. No directory entry named `a53.elf` or an obvious A53 firmware image was found. Selected `.elf`-named files in `system_b` are still SELF containers; the unpacker does not decrypt them or the opaque PUP kernel entry. This corrects the earlier statement that the nested images were uniformly inaccessible. Full images, listings and extracted files remain private. No A53 profile, PPR lifecycle or fPKG function is established.

An independent offline comparison with the public `terrafirma2021/ps5-13.60-kernel-dump` live RAM archive found all 13,369,344 saved x86 text bytes identical to our captured text range. Its data differs across 5,382 overlapping 4 KiB pages, and its public zero-filled gap omits 128 mapped, nonzero pages present in our saved range. Both are partial live captures with different virtual bases; neither is a complete kernel image, and neither is the opaque `kernel.bin` extracted from the PUP. This comparison provides a useful text identity check, not a new executable or A53 capability. No console action occurred.

### 15 October key-page review and stopped live preflight

The PSDevWiki ROM/EAP key page gives a plausible EAP KBL format lead for two named components in the saved 13.60 PUP. It does not state a validated 13.60 cipher boundary, IV, integrity test or A53 image recovery path. No key was tried or matched to the opaque PUP kernel, Oberon secure-loader containers, exFAT SELFs or a staged A53 image. PPR and fPKG evidence remains unchanged.

A private read-only syscall-marker preflight passed its **offline saved-byte** verifier and was **sent once** on the user-reported Relapse + Byepervisor post-wake boot. The host received zero bytes of payload output, no checkpoint and no EOF before timeout. A subsequent port connection and ping timed out; the user reported a suspected kernel panic. Klogsrv was present, but no evidence attributes the failure to it. Without a first-entry or current-boot receipt checkpoint, neither execution nor a specific failing read is established. The preflight is not a normal-stack or allocation success. No further marker write, allocator call or recorder send should follow this result without an independently verified fresh-boot receipt and offline fault localization.

Further **offline** IDA review clarified why the built-in `sysarch` allocation is unsuitable for a smoke test: the caller does not test a zero allocator return before dereferencing it. The allocator can return zero on a saved failure path, and success changes thread TSS/I/O state without returning the allocation address. A storage latency-test ioctl is another ordinary-stack caller but also drives storage operations. Neither supplies a small, failure-safe, address-reporting call context. No follow-up ELF or ioctl was sent; the normal-context allocation and recovery gates remain open.

The saved taskqueue worker is a further normal-stack shape, but its callback has a separate CFI check and its enqueue path acquires a lock. Neither a compatible allocator wrapper nor a safe result/readback and lifetime path has been demonstrated. No taskqueue operation was attempted.

An **offline** screen of all 268 accepted callback stubs in the saved text found no direct destination inside the known direct allocator-caller functions. IDA-recognized calls from their defined target functions also had no direct edge to those callers; a second step yielded only a generic malloc intermediary. The result is bounded by the captured text, IDA-defined functions and direct edges. It does not establish a safe callback, allocation return channel, or live taskqueue operation.
