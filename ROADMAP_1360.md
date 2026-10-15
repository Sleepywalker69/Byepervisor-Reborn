# From 13.60 AP completion to a usable post-HV chain

Current downstream state is reconciled in [DOWNSTREAM_AUDIT_1360.md](DOWNSTREAM_AUDIT_1360.md#v5v8-offline-reconciliation-and-remaining-functional-gates). Live work later resumed for bounded A53 observation and stopped on a process-read error; V7 was not rerun. V8 is an offline build only, and no fPKG has demonstrably mounted and run as a title.

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
   global patch bytes. A separate read-only status ELF returned the AP-complete
   marker, zero hook state, matching restored gate/IST, firmware, and boot time
   on the current boot. This proves the status format can be observed through
   standalone elfldr; it is not an attempt-scoped completion receipt and it
   has not been integrated with Y2JB. No downstream functional patch or
   post-wake autoloader chain has been deployed.

The immediate goal is milestone 4: derive a 13.60 patch map from the captured
code, compare it with downstream HEN/kstuff needs, and build a checked
post-wake completion signal before autoloader integration. The operational
read/write gate is met on one post-AP boot. AP causality remains an open
research question and must not be inferred from this gate.

## Milestone 4 gate record, 13 October

1. **Attempt receipt: V3 live gate passed on one boot.** The candidate
   prints `BPR_ARM_V1` only after the resume hook installs and writes matching
   completion only after AP-complete and successful hook cleanup. A separate
   status reader checks stage, hook, gate, IST, ID, firmware, and boot. Ten C
   state cases and 23 Y2JB parser cases passed. On an older AP-completed boot,
   the read-only V2 reader returned `ok=0` because both attempt IDs were zero.
   At that offline gate, the new hook ELF had not run; no post-wake chain is
   enabled.
   A subsequent controlled V2 run did arm and complete AP/cleanup, but the
   strict `kern.boottime` equality failed because that value moved by
   31.546393 seconds across Rest Mode. The chain stopped. V3 now compares
   the attempt ID plus kernel text base and pmap root. On fresh boot
   `1791898356.667496`, the V3 arm record and independent post-wake status
   matched all three, reported AP-complete, zero hook, restored gate/IST, and
   firmware 13.60. The Y2JB parser accepted the saved records. The user
   reported a responsive home screen. This is one successful receipt check,
   not proof that YouTube survives Rest Mode or that a downstream payload ran.
2. **Controlled attempt: achieved once for V3.** The exact live arm ID was
   `6ace3357104ff054`, text base `ffffffff998a0000`, and pmap root
   `000000001d6f4000`. The independent `BPR_STATUS_V3` reported matching
   values, AP-complete stage, zero hook state, restored gate/IST, and matching
   completion ID. `kern.boottime` shifted to `1791898416.675424` and is kept
   as diagnostic information, not an equality gate. The original host socket
   reset across S3; a separate post-wake elfldr connection worked.
3. **Y2JB: pending.** The user's latest instruction authorizes building for
   both PPSA01651 and PPSA01652. The Y2JB branch now has a receipt-only test
   path: after Relapse handoff it sends the exact tested V3 hook ELF, stores
   the arm values in a console file, and queries the independent status after
   wake. Its offline ZIP passes archive and embedded-ELF hash checks; no
   `download0.dat` was built by the artifact-only GitHub workflow and installed
   in both PPSA01651 and PPSA01652 with full staged and final readback hashes;
   the previous images were retained under backup names. The first PPSA01652
   launch showed Relapse clean handoff, then its single immediate local
   elfldr connect failed before V3 arm and before Rest Mode. Port 9021 was
   unreachable from the host immediately afterward, then reachable later;
   delayed readiness is plausible but not yet proven inside the console.
   A bounded readiness wait passed offline tests and was built as a new UFS2
   image. Both title files were replaced after YouTube was closed; staged and
   promoted full-file hashes matched
   `b2914ebcb41fbc0d507d63b9db5383d7da131446f7163b67de030e4ebaa66f8d`.
   The previous copies remain under `download0.dat.pre-v3wait-20261013`.
   This revised image has not yet been launched. No later payload is enabled.
   Verify
   YouTube and elfldr survival before choosing a surviving post-wake
   orchestrator; a completed socket send is not module completion.
4. **Downstream capability: pending.** Compare full Kstuff's desired behavior
   with kstuff-lite's 13.60 implementation. Choose one concrete consumer and
   trace its scoped handler and kernel sites before a guarded live change.
   Current byte matches in `PATCH_MAP_1360.md` are not functional proof.

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

## Optional post-wake ELF gate, 13 October

The PPSA01651 V3 Y2JB attempt now has a verified matching pre-Rest arm record
and independent post-wake AP-complete status, with zero hook state and restored
gate/IST. The YouTube process did not produce a post-wake continuation, so
post-wake orchestration must live in the resumed native process.

`POSTWAKE_CONFIG=1` builds a candidate native coordinator. It runs only after
the existing AP-complete branch has restored the hook and written the matching
completion ID. It checks the live receipt again and requires the exact Y2JB
pre-Rest arm file. Missing `postwake.txt` or `manual` sends no extra ELF.
One `elf=<filename>.elf` entry selects one packaged ELF; malformed or multiline
config stops. The current test image explicitly selects `pldmgr.elf`. That
name receives an additional readiness check: port 8084 must be absent before
launch and `/version` must return exactly `0.5.2` afterward. Other names can
only be reported as sent, not confirmed running. There is no automatic
multi-payload chain in this candidate.

The SDK build and 13 native config cases passed offline. Candidate ELF:
209,408 bytes, SHA-256
`4736c131f13d6879eabdcde4ea2911678a4d1919e9a2e8e72c7a638d4ba484cd`.
The Y2JB ZIP passed CRC, 48 receipt parser cases, five transport cases and
embedded hashes; SHA-256
`24ad2fe211985c7cdd169baf24c88c36a34b148ade9cdabb7d35615c12c25550`.
These are offline checks only. A fresh boot, installed test image, one Rest
Mode cycle, matching receipt, and live port 8084 response remain required.
Payload Manager's own autoload preferences take effect after it starts; the
native gate does not override them. GitHub Actions run `38413832508` built the
UFS2 image from Y2JB commit `02dcc79`; the 336,789,504-byte image's manifest
and independent local SHA-256 both equal
`f5191fa528ac86eeaaeeb9f55574d826e0554b4ba9c0a490e788598f1b55e0d8`.
Both PPSA01651 and PPSA01652 images were installed via ftpsrv on port 2121.
Staged and final full-file readbacks matched the image hash above; prior
images remain as `download0.dat.pre-postwake-20261013`. The 1,197-byte local
deployment report has SHA-256
`30af26db8a2fc2386e0ce0dcb220da5920fdb4bb5e1bd87ad6ae4e930e64b82c`.
The new image had not been launched at this deployment gate.

### First optional manager run and V2 retry candidate

The first PPSA01651 Rest Mode run produced arm ID `6ace41b51d84a059`.
Independent V3 status after wake matched that ID, text base and pmap root,
reported AP-complete, zero hook state and restored gate/IST. Before manual
intervention, Payload Manager port 8084 refused connections and the persistent
stage log ended at pre-Rest `HOOK arming begins`. The user later launched
Payload Manager manually; its HTTP 200 `/version` body `0.5.2` is separate
evidence. The mounted title files and hashes matched the installed image.
Thus the AP receipt passed and automatic manager launch did not reach
readiness; the exact stopping instruction remains unidentified.

The pre-Rest socket closes across S3. A failed post-wake file-log append could
trigger a `printf` to that dead socket and SIGPIPE before the optional launch.
This is a hypothesis. V2 ignores SIGPIPE before hook installation, waits 15
seconds after verified cleanup before filesystem checks, and stamps a distinct
pre-Rest build marker. The 209,576-byte V2 candidate ELF has SHA-256
`fceb24e5f83f3d14358fe4741e5e65611ac68781ab4edb80ff8e839f33010afb`.
SDK compilation, 13 native config cases and Y2JB's 48 parser/five transport
cases passed offline. V2 has not been installed or run on the console.

GitHub Actions run `38415881929` passed for Y2JB commit `e454eda` and built
the V2 UFS2 image. Its manifest and independent local SHA-256 match:
336,789,504 bytes, `7187c22657153ccfcd1ed47afee1303e4e6afa5743355262d3b5d2a95991d9ac`.
Both regional title images were staged, fully read back, promoted and fully
read back via ftpsrv. Each final file matched that hash. Prior images remain
as `download0.dat.pre-postwake-v2-20261013`. The local deployment report's
SHA-256 is `810f28a322ab9cdfe6f055d73db4365ae06fb2f37478a70cd647cf7363339efd`.
Before replacement, PPSA01651's image hash was
`341a7bf5997b1e36f2928762769c0aaafb448033d9849e18fd8620bc35e729f5`,
different from the V1 image; PPSA01652 still matched V1. The reason for that
PPSA01651 difference has not been investigated. V2 has not yet run on PS5.

### V2 live run: automatic manager ready; later safety check failed

On a fresh PPSA01651 boot, the user observed automatic Rest Mode and a
responsive wake. Before manual Payload Manager intervention, a host HTTP
request to port 8084 returned 200 with body `0.5.2`. The saved Y2JB arm file
recorded `6ace4524b8022059`, text base `ffffffff98880000`, and pmap root
`000000001c6d4000`. The latest stage-log session identified
`build=13.60-postwake-sigpipe-v2`, pid 89. It recorded AP-complete cleanup,
`POSTWAKE_GATE` for that arm ID and PPSA01651, and `POSTWAKE_PM_READY` for
that same ID. This supports an automatic manager launch on this run; the prior
boot's manual manager is excluded by the fresh boot and distinct arm record.

An independent V3 status ELF then returned the matching ID/base/root,
AP-complete stage and zero hook state, but `ok=0 gate=0 ist=0`. Its 203-byte
output SHA-256 is `5233d4aee438f9d20b659bdb127df63314a6dbea19de021ca7bae5563da35d8e`.
The host HTTP/log check JSON and saved stage/arm files are under the local
`PPSA0165x-postwake-v2-image` directory. The status failure means the full
post-wake safety gate did not pass. The native coordinator checks gate/IST
before sending Payload Manager; it does not recheck them after Payload Manager
or its own autoload has run. A later change by another payload is one possible
explanation, but the exact cause is unproved. The user reported the home
screen responsive. No repeat or further console payload test was started.
The user later reported that Payload Manager launched all configured downstream
payloads and proposed that the short Rest Mode interval caused the mismatch.
Both are observations/hypotheses, not a proven cause. A fresh-boot repeat with
steady amber for a full minute is being prepared; no existing receipt is
being reused.

### Second V2 attempt: automatic manager repeated, gate/IST mismatch repeated

A second fresh PPSA01651 boot used new arm ID `6ace45f68e139059`, text base
`ffffffff8ae40000`, and root `000000000ec94000`. The user confirmed the
Rest Mode period was again shorter than one minute and the home screen was
responsive. Port 8084 again answered HTTP 200 `/version` body `0.5.2` before
any manual manager launch. The new V2 stage-log session recorded AP-complete,
`POSTWAKE_GATE` and `POSTWAKE_PM_READY` for the new arm ID. The independent
V3 reader matched the new ID/base/root and showed AP-complete and zero hook,
but again returned `ok=0 gate=0 ist=0`. Its 203-byte SHA-256 is
`d72f61b202b91fe663b25be34566c14a21a6334a21167dcce2f71d1e4d67b3a6`.
Thus V2's automatic manager launch repeated, but the full post-chain safety
gate did not pass. Both runs had short Rest Mode intervals, so timing remains
unexcluded.

The manager's read-only `/log` endpoint (saved locally, 1,946 bytes, SHA-256
`f21023145fad9af3890ed3cc12bcb6a4b30fc5ee388ec917d8bffe1456743ea8`)
reported autoload socket sends, in order, for kstuff-fpkg, ShadowmountPlus,
ProsperoMgr, ps5upload, klogsrv, and ps5debug-NG. The user reported that
these launched. A socket send alone is not proof each payload completed.
One or more downstream payloads could alter the checked gate/IST after the
native coordinator's pre-send receipt check; this remains a hypothesis.
No third attempt or additional payload launch has been started.
The current boot's bounded evidence collection is complete. Read-only FTP
saved `/data/pldmgr/autoload.txt` (127 bytes, SHA-256
`6c92b3eee5f8c33f35a1e76ee9225ff9e7611b2e7586a44ff2df400036b1eff6`)
and `pldmgr_config.txt` (183 bytes, SHA-256
`93984f6a50e93b73e2cc334a701de0f025f24b8da26df558c390e2499f1cf8aa`).
The config enables autoload with a five-second delay. The read-only manager
`/autoload_status` response (275 bytes, SHA-256
`9d7d785634084b5cb9a0d250dbfebb0602d87067dd19d5e73a80ef18ef5ae8c0`)
reported `total=6`, `done=6`, `current=DONE`. This is manager-side status,
not independent proof each ELF completed. The user is doing one final fresh
boot with a full-minute Rest Mode interval; its new arm ID must be checked
separately from these two short-rest attempts.

### Final full-minute control: same post-chain mismatch

On a third fresh PPSA01651 boot, the user confirmed at least one full minute
at steady amber and a responsive home screen after wake. The new arm ID was
`6ace477b04d7a059`, text base `ffffffffd7f10000`, root
`000000001bd64000`. The saved V2 stage-log session again recorded
AP-complete cleanup, `POSTWAKE_GATE`, and `POSTWAKE_PM_READY` for this ID.
Host `/version` returned HTTP 200 body `0.5.2` before manual manager launch.
The independent V3 status matched the ID/base/root, AP-complete stage,
zero hook and matching completion ID, but again returned `ok=0 gate=0 ist=0`.
Its 203-byte output SHA-256 is
`9aa697a87bd6b71b2e4c1f4af49bcca2530cdc12446825b3e259b61666b0124c`.
The third boot's manager `/log` captured the same six-entry autoload sequence
and had SHA-256 `f21023145fad9af3890ed3cc12bcb6a4b30fc5ee388ec917d8bffe1456743ea8`.
A full-minute interval did not remove the mismatch. The next diagnostic
boundary is before versus after manager autoload; no payload should be
credited or blamed without that measurement. Automatic Payload Manager
startup has passed on three distinct boot/arm attempts, but post-chain
safety validation has failed on all three. No further live launch was made.

### Stability stop after third boot

At approximately 15:05 UTC, after the full-minute control and its manager
six-payload autoload, the user reported the PS5 frozen and still frozen on
follow-up. The assistant had not sent a kernel-text reader or the new
before/after watcher. The last console requests were the bounded V3 status
reader and a read-only manager `/log` GET; no further console request was
made after the freeze report. The failure cause is unproved. Hold all further
live launches until the user reports recovery and a fresh boot.

A host-only watcher candidate, `diagnostics/watch_postwake_autoload_1360.py`,
was written after this result. It pins the previously used read-only V3
status ELF, requires port 8084 closed before Y2JB, accepts an early receipt
only when the manager countdown has at least 2.5 seconds left and `done=0`
both before and after the read, then takes a late receipt after `done=total`.
It has not run on the PS5. Its purpose is to distinguish a pre-autoload
cleanup state from a post-autoload state; it does not assign causality to an
individual payload.

### Manual post-wake package installed

The user selected a no-launch post-wake default after the freeze. Y2JB's
`postwake.txt` now contains only `manual`, which the existing native parser
maps to no ELF launch; the resumed Byepervisor code is unchanged. GitHub
Actions built the 336,789,504-byte UFS2 image at Y2JB commit `d58a027`.
On 13 October, both YouTube titles' `download0.dat` files were replaced via
FTP 2121 with staged upload and full hash readback. Both installed images
matched SHA-256
`ff1ec8eea341fc282416ba07047e51a6d0e074019c8ef10bf6b4ccea777725c0`;
each preceding image remains as `download0.dat.pre-manual-20261013`.
The private deployment receipt is
`PPSA0165x-postwake-manual-image/deployment.json`, SHA-256
`e3afbea570a3a752ee2b7fa0d2f2a5f3582db52ca4ec2229a551e8d62203a2c1`.
This is an installed-byte result. A fresh manual-mode Rest Mode run and
independent receipt check remain unperformed. The six-payload automatic chain
is no longer the configured default.

### Downstream IDA slice after manual image installation

The existing 13.60 IDB was opened without re-running autoanalysis. The
`mprotect_fix_start` offset resolves to a six-byte `jnz` in the 0x13fa-byte
`mprotect_path_1360_candidate` function. Bounded disassembly confirms the
branch follows `test dx,si` and targets a shared cleanup/return area; the
function has at least seven distinct caller functions, including a small
page-rounded protection wrapper. The kstuff-lite source trap skips this
branch only for its instrumented flow. A global static branch patch would
change more callers, so no replacement bytes or live patch are proposed
from this finding alone. Exact addresses, bytes and limits are recorded
in `PATCH_MAP_1360.md`.

After the user rebooted, a read-only FTP fetch recovered the first
manual-mode session. The PPSA01651 Y2JB arm ID and the latest stage-log
`POSTWAKE_MANUAL` ID both equal `6ace4bc8e2029059`; the session recorded
AP completion and contains no Payload Manager readiness marker. The user
reported a responsive console. This validates a live manual branch after
wake, while an independent gate/IST status after wake remains unmeasured.
The private copies and their hashes are recorded in the Y2JB
`POSTWAKE_GATE_1360.md`.

The missing independent status read was then run once with the pinned
111,024-byte V3 reader while Payload Manager port 8084 was closed. It
returned `ok=1`, firmware `13600007`, boot `1791904675.805577`, the same
arm/completion ID `6ace4bc8e2029059`, base/root matching the pre-Rest
record, AP-complete stage, hook zero, and `gate=1 ist=1`. The raw 203-byte
private output hashes
`be1e2793f9a5c501fc9934d5d23082efaf1d4abbe28e382cba25a7782d81fba0`.
This closes the attempt-specific manual-mode AP/hook/restore gate at the
time of the read. It does not prove any downstream payload launch or a
functional 13.60 kernel capability. Console responsiveness after the read
is awaiting the user's observation.

IDA also resolved the kstuff-lite `aslr_fix_end = aslr_fix_start-0xe`
target: it is an existing alternate block that writes `0x400000` to a
field at `+0x30`, then rejoins the surrounding flow. The previously
undefined code was bounded from `0xffffffffd3eff3a0` to
`0xffffffffd3f00514` and provisionally named
`exec_self_imgact_1360_candidate` in the private IDB. Its
`_exec_self_imgact` string and two mode-setting jump wrappers support
the image-activation context; the structure field and app-specific effect
still need proof. This is documented in `PATCH_MAP_1360.md`; no patch
was made.

The next stack-reference slice shows the `aslr` redirect writes into a
newly allocated 424-byte object returned by `sub_FFFFFFFFD469AB10` and
stored at image-context offset `+0x150`. The object's field `+0x30`
remains untyped. Hex-Rays could not decompile the newly defined owner at
`0xffffffffd3eff517`; the instruction-level trace is retained as the
evidence. Within the same owner, the `+0x30` value is added to three
image-context/object fields immediately after the branch, supporting an
offset interpretation in that path. A second caller of the object's
constructor treats `+0x30` and `+0x38` as the start and span of an address
range passed to an unwire-related routine. This narrows the field's role
without proving its type. The next useful offline step is to map the
constructor and range consumers before choosing any patch behavior.

The next IDA slice traced the ordinary assignment: the 76-byte callee at
`0xffffffffd3f005c0` calculates an image displacement with
`0xffffffffd3e3a920`, using bounds `0x400000`–`0x80000000`, a rounded
image span, and a random value shifted by 14 or 21 bits. It stores the
result at the new object's `+0x30`. The alternate block instead stores the
fixed lower bound `0x400000`, bypassing that calculation. This explains
the ASLR effect at instruction level and narrows the field to an image load
displacement in this path; it does not make a global branch patch safe.
Exact addresses, original bytes, and limits are in `PATCH_MAP_1360.md`.
No console write or payload launch was made for this trace.

### Installed downstream kstuff fPKG binary audit

The exact console file `kstuff-fpkg-1.13-dr-test6.elf` was retrieved
read-only and matched the earlier 1,818,680-byte SHA-256
`b1f60c1ec1685904d396da16710dc9d5676bf0880abf74c65459c886010b9443`.
Offline IDA shows its PPR installer at ELF offset `0x8e40` compares the
masked firmware against `0x12000000` and fails for firmware 12 and higher.
At 13.60 it takes the fall-through that prints the missing-support message,
then the loader reports plaintext PPR disabled. This disqualifies this
exact build as evidence of a working 13.60 PPR/fPKG stage; other kstuff
features remain untested. See `DOWNSTREAM_AUDIT_1360.md`. No downstream
ELF was launched during this audit.

The public kstuff-lite checkout at `48cd2c4` is not an immediately usable
13.60 PPR replacement. Although `fpkg.c` contains a 13.60 profile and the
matching offset table, `get_ppr_profile()` returns `NULL` for every firmware
above 11.60 before the profile switch. The source explicitly leaves those
newer profiles available for later work but does not arm plaintext PPR.
Its 13.60 verifyImage/key-cleanup ABI still needs matching-image review and
live functional proof. This is recorded in `DOWNSTREAM_AUDIT_1360.md`.

The first matching-image static review then passed the public
`validate_ppr_offsets.py` checks against the private 13.60 synthetic ELF
(`checks=PASS count=0`). IDA independently confirmed that the two
get-index return sites call the expected XTS/CMAC functions and that the
profile's cleanup deltas land on calls to `ppfs_put_cmac_index` and
`ppfs_put_xts_index`. This narrows the ABI uncertainty but does not remove
the source's `FWVER > 0x1160` gate or establish live PPR/fPKG behavior.
The private test receipt and exact call bytes are in `DOWNSTREAM_AUDIT_1360.md`.

An exact-13.60 compile-time PPR opt-in patch was then prepared against the
public kstuff-lite checkout. The default source path remains disabled above
11.60, while `-DKSTUFF_ENABLE_PPR_1360=1` admits only 13.60. Both the
unmodified and opt-in full loader ELFs now build offline with a separately
cross-built SQLite 3.46.1 library; the opt-in ELF is 1,523,688 bytes and
SHA-256 `fd3a5a355c954990c736a162c4dea0b681e64550798dc7475534d3ea3aee161b`.
The patch and build provenance are in `downstream/KSTUFF_1360_BUILD.md`.
Neither ELF has been launched. Static call-site matches and a completed
build do not establish runtime PPR, fPKG, or the whole kstuff offset table.

An observed exact-13.60 opt-in loader now also builds offline with
`KSTUFF_OBS=1`. The reader was corrected to preserve its log across polling
passes and avoid replaying old ring entries; its source patch and build hashes
are recorded in `downstream/KSTUFF_1360_BUILD.md`. A console test still needs
a fresh attempt receipt, a bounded PPR operation, reader counters tied to
that operation, a functional package result, and a post-test stability check.

The first observed opt-in loader did start on a previously verified manual
Byepervisor wake. Its reader attached, but the subsequent private fPKG
installer did not yield a native result. A small read-only probe confirmed
the package file, while an AppInstUtil-linked probe produced no output
before its first print. A separate unlinked probe reached its first print,
then stopped at `dlopen("libSceAppInstUtil.sprx", RTLD_NOW)`. Kstuff's
`dynlib_load_prx` trap count increased; PPR and fPKG counters stayed at
zero. A private snapshot of app.db had no row for the test title, and no
title files were found. This makes registration unproved and unlikely at
the capture time, but does not provide a native install-status result.
The exact observations and limits are in `DOWNSTREAM_AUDIT_1360.md`.

Next gate: on a fresh boot, establish whether the AppInstUtil load path
completes in a controlled, separately observable helper before any install
request. Preserve its durable entry/exit log and process status. Only
after a completed load and a native status query may one install request
be attempted, followed by a title-registration readback, PPR counters,
and a user-observed launch of the test title. Do not resend the earlier
installer on the current boot while its outcome and old probe processes
remain ambiguous.

The AppInstUtil load path completed after adding its missing `libSceIpmi`
dependency before `libSceAppInstUtil`. One corrected local-URI install
request registered the private test title; an independent query returned
`playable`, zero error, and 100% progress. A separate one-shot launch call
returned `0x80f00800`; no app marker or process was observed. PPR and fPKG
counters remained zero. See `DOWNSTREAM_AUDIT_1360.md` for exact receipts.

Next gate: do not equate registration with runtime execution. Establish
whether a known-good 13.60 PS5 homebrew package launches under this exact
environment, and separately identify the 13.60 A53 PPR profile needed for
plaintext PS5 fake-package reads. The public A53 fast installer has exact
profiles only through 11.40. Do not load it as a 13.60 solution or claim
fPKG support from the current kernel-only PPR opt-in.

The one-shot launcher control did succeed for an existing installed
homebrew folder app and yielded a new `eboot.bin` process. The rejected
test title is a fake `app.pkg`, so the next comparison should use a known-good
13.60 package of the same kind or inspect its mount/authorization path.
Do not infer that the new package is malformed or that PPR is solely
responsible from this two-title comparison.

The A53 status action confirmed the live retail `releases/13.60` build and
stopped at the absent exact profile. Small reads at two older physical
addresses and one EL3 virtual address all returned status `0x20`, zero
bytes, while `GET_CONF` worked. The next acquisition route must produce a
matching A53 image or validated current memory map; scanning further
unverified addresses or loading the older fast installer would not advance
the gate.

### Next configured-payload gate

An isolated Y2JB branch `postwake-marker-1360` now selects one diagnostic
ELF after the existing same-attempt AP receipt gate. The diagnostic writes
an arm-ID-named file only after independently checking the live receipt and
one matching YouTube arm file; it launches no other payload. Its source is
`diagnostics/postwake_receipt_marker_1360.c`. The 13.60 SDK build, Y2JB
parser tests, ZIP checks, and GitHub UFS2 image build passed offline. The
image is not installed, and no marker has run on the PS5. The live gate is
a fresh boot, one Rest Mode/wake cycle, a new marker with matching arm/base/
root, and a separate V3 status read matching the marker's boot time and
showing AP complete, hook zero, and restored gate/IST. Only then count a
configured post-wake ELF as executed rather than merely transmitted.

### Narrow downstream kernel change proved on one boot

The opt-in 13.60 kstuff build's PPR profile gate was exercised through its
control kekcall. A one-shot probe entered a PPR mount scope, called `CHECK`
without staging data, and left the scope. `CHECK` returned `EPERM`, which
the reviewed source reaches after a non-null firmware profile; the
disabled-profile branch would return `EINVAL`. Scope entry and exit both
returned success. A separate reader still showed zero PPR G6, fPKG and
plaintext key-pair activity. This validates the narrow kernel-side gate,
not the A53 selector or a playable fPKG. The A53 image/profile and a
functional package-read/launch check remain required.

The next bounded package test used the exact plaintext/no-auth marker
format instead of the earlier unmarked/free-DRM package. Its private
603,009-byte `PPSA99099` package passed offline structural checks and
was installed once; independent native status reported `playable`.
One launch returned `0x80940010`, `accepted=0`. A before/after kstuff
reader found one fake-package verify emulation and two PPR G6
applications, with a matched 13.60 profile and hook stage 9 (mount
latch armed). One synthetic key pair remained outstanding and clear-key
count stayed zero. This advances the proof to the kernel-side PPR path,
but title execution, successful mount, A53 plaintext processing, and
proper key-pair release remain open. The next gate is an exact
mount/clear lifecycle review and bounded read-only status snapshot
before any more launch attempts.

The delayed read-only snapshot retained the single synthetic key pair.
An app-info probe identified the active big-app process as
`PPSA01651` YouTube; the public ps5upload launcher source documents
`0x80940010` for another app already running on FW13.60. Run the next
single launch only on a fresh boot with a zero-pair baseline and YouTube
closed. Use before/after PPR counts and actual title execution as the
acceptance gate.

The read-only app-info result identified `PPSA01651` as the active
`eboot.bin`. A guarded launch probe detected it and made no launch call.
The guard's clear branch and title execution remain untested.

The isolated post-wake marker had a separate acceptance race: it required
EOF immediately after the title/arm prefix, while resumed Y2JB may append
status lines before the native coordinator sends the marker. The marker
now accepts the same exact three-line prefix as the coordinator, with a
bounded file size. Host parser cases cover appended status, missing and
mismatched fields; the new marker ELF compiles with SDK v0.44. Its live
post-wake execution remains unverified.

### Current live boundary and next boot gate

The one-shot exact-title close request identified PPSA01651 but did not
return a close result before its host socket timed out. A later process
probe found no `eboot.bin` app, but does not establish how the close call
ended. On that same boot, native first-page reads of both installed
YouTube images and both saved backups returned `EIO`; FTP also returned
`EIO` for the installed test package while small metadata remained
readable. The cause is undetermined. A subsequent boot-identity read
confirmed this was still the same boot. A kstuff reader printed only its
start line, then timed out before its attach line; its console lifetime
is unknown. Neither YouTube image was replaced and no further package
launch was made.

The next live gate requires a fresh boot and a small content-read test of
both title images and the installed package before any upload, rename,
or launch. Confirm the PPR synthetic-pair baseline before another package
attempt. The private two-title deployment script now verifies both old
images and both staged images before promoting either; fake-FTP tests
cover a second-title read failure with zero mutations. The revised marker
image is still offline only.

The opt-in mount-result instrumentation has also been rebuilt as a full
kstuff loader with its embedded kernel payload, plus a matching one-shot
reader. The earlier 675,864-byte kernel payload ELF was mislabeled as a
loader and has not been sent. These new artifacts remain private and
untested on PS5; they cannot substitute for the fresh-boot content-read
and zero-pair gates.

### Post-wake marker attempt and pause (13 October 2026)

A fresh FW13.60 boot passed a read-only gate: the boot identity differed
from the earlier stalled boot, FTP answered a command, and the first 4,096
bytes of both installed YouTube images and both saved backups were readable.
Both PPSA01651 and PPSA01652 were then replaced with the same staged and
full-readback-verified marker-test UFS2 image (336,789,504 bytes, SHA-256
`bc50e6b179d03b30fa97cbd45a8d9744a96f0002d78ddeb2d1a40a0603b2ca66`).
The prior images were preserved as backups. This was the first live run of
the configured `postwake_receipt_marker_1360.elf` path; the optional Payload
Manager image was not installed.

The user rebooted, launched one of the identical YouTube icons, and then
reported a kernel panic after the Rest Mode wake. The console powered off.
There was no host request during the Rest Mode/wake cycle. No new arm ID,
AP-complete status, marker output, stage log, or crash dump was recovered,
and the launched title is unknown. The marker acceptance gate therefore
**did not pass**. A previous V3 receipt success on another boot does not
verify this attempt. The added post-wake ELF selection is a credible
suspect, but the evidence does not locate the fault within the S3 hook,
AP completion, resumed coordinator, marker send/execution, or later system
path.

Testing is paused at the user's request. Preserve recovery evidence and
audit the manual-to-marker delta before another configured ELF run. Do not
advance to the optional Payload Manager candidate or downstream package
testing based on this failed marker attempt.

### Recovery and controlled comparison (13 October 2026)

After the user resumed work, a read-only recovery capture found the newest
saved Byepervisor session through `HOOK arming begins; file logging paused
until cleanup`, with no later saved event for that session. The logging gap
cannot locate the failure: logging pauses during the hook and later file
writes can fail. Neither title arm file nor a per-attempt marker file was
available after reboot. Generic error history and a later coredump did not
identify a fault in the marker path.

An offline comparison found the same embedded Byepervisor ELF, V3 status ELF,
and kexp bytes in the manual and marker images. The marker image adds the
receipt-marker ELF and selects it in `postwake.txt`; filesystem/build blocks
also differ. The launched PPSA01651 image had changed 69 of its 4-KiB blocks
by the time it was recaptured, while those embedded ELFs and the configuration
remained byte-identical. Thus a whole-image hash after launch is not a stable
identity check for this UFS2 image; the changed blocks do not establish a
crash cause.

Both YouTube titles were restored to the known manual-mode image and fully
read back: 336,789,504 bytes, SHA-256
`ff1ec8eea341fc282416ba07047e51a6d0e074019c8ef10bf6b4ccea777725c0`.
The previous marker images were preserved as backups. This restored image
has not yet run on the new boot. The user cannot observe a live test at
present, so no new YouTube launch, ELF send, or Rest Mode cycle followed.

The next controlled comparison is one fresh-boot manual-mode wake with a
current arm ID, independent AP-complete receipt, zero hook state, restored
gate/IST and a matching `POSTWAKE_MANUAL` event. Only after that gate passes
should the same pinned marker ELF be sent separately through elfldr once.
A matching per-attempt marker file and a second independent receipt would
isolate marker execution from automatic post-wake orchestration. The private
control script has seven passing offline success/failure-path tests, including
rejection of any second title arm and preservation of unknown partial-send
state; it has not run on PS5. No inference about functional kstuff, fPKG, or Linux support follows
from this recovery check.

The manual and marker branches contain identical Y2JB runtime code and
identical Byepervisor and status ELF blobs. The runtime input change is the
marker ELF plus `postwake.txt` selecting it. The common Byepervisor path
consults that config only after AP-complete, successful hook cleanup,
completion-ID write and a 15-second wait. The saved failure log does not
show whether the last wake reached that point. A direct marker send after
a separately verified manual wake is still needed to distinguish the
configured coordinator path from marker execution.

### Manual wake and external marker control (13 October 2026)

One fresh manual-mode Y2JB/Byepervisor run on PPSA01651 reached a saved
`POSTWAKE_MANUAL` event. Its sole title arm record matched an independent V3
status read: AP complete, hook state zero, matching arm/completion IDs and
restored gate/IST. The status `kern.boottime` was about nine seconds later
than the stage log's session wall-clock timestamp; previous 13.60 Rest Mode
cycles had also moved `kern.boottime`. Attempt identity therefore used the
exact arm ID, title, kernel base/root and latest session event, while the
clock difference was retained as evidence.

With the user watching a responsive console, the same pinned marker ELF was
sent once through the existing elfldr **after** that manual receipt passed.
It wrote a 224-byte per-attempt marker record whose title, boot, arm and
completion IDs, base/root, AP stage, hook state, gate and IST matched the
independent status. A second status read was byte-identical. The private
control report recorded `execution_verified` and passed; the user then
confirmed the home screen was still responsive. This establishes
that the marker ELF can execute via an external send after manual wake.
It does not establish that the configured Y2JB post-wake coordinator survives
Rest Mode or sends an ELF, and it does not identify the earlier power-off
cause. No Payload Manager or kstuff test was part of this control.

The manual-mode chain remains the chosen operating path. Automatic
post-wake ELF loading is lower priority; continue the 13.60 kstuff
qualification work from its separate offline source/build and live-state
gates without treating configured ELF delivery as a prerequisite.

Offline work in parallel mapped all 78 entries of the published 13.60
kstuff-lite kernel offset table to the saved capture or its one structure
field, and captured the retail on-disk ShellCore ELF read-only. All 34
retail ShellCore patch sites resolve to executable file bytes. One published
near-branch patch changes its target when decoded, so the full table is
not ready to apply unchanged. The [kstuff audit](KSTUFF_1360_DISABLED_MAP.md)
records the exact finding, source-only candidate fix, firmware gates, and
unverified A53/loaded-module boundaries. These offline checks did not
launch a payload or change the console.

The corrected 13.60 kstuff candidate has since compiled as a full loader in
private storage. A pinned offline verifier parsed all 34 retail ShellCore
patch rows from its embedded payload and confirmed the corrected two-byte
`90 e9` entry at `0x36ae22`. The matching payload binary is embedded once.
This closes the source-to-build check for that table; loaded ShellCore bytes,
the dynamic PPR hook, A53 behavior, mount result, key cleanup and fPKG launch
still require separate live evidence. That offline candidate had not run on
PS5 at this stage of the investigation.

A later manual-mode post-HV boot supplied a bounded read-only comparison of
the running retail ShellCore against the saved on-disk image. All 34 patch
sites matched at their compiled write widths, including `0f 86` at the
corrected two-byte site. The user confirmed the home screen remained
responsive. This establishes current-boot patch preimages with the debugger's
non-atomic memory-read limits; it does not establish that kstuff can apply
them or that fPKG behavior works.

One guarded full-loader ELF was then sent through elfldr. Its preflight
reported `reason=module_info` before the ShellCore patch loop, and the outer
loader reported that it skipped its follow-on work. The user again confirmed
the home screen was responsive. Source review found that the remote module
syscall had been given a pointer in the loader's address space. A revised
private candidate stages that structure in ShellCore's address space,
checks all 34 preimages, orders code-cave writes before their redirecting
branches, and verifies writes with a rollback attempt on failure. Its
compiled tables and embedded payload passed offline verification. It needs
a fresh manual-mode boot and live run; no kstuff success is claimed yet.

That revised candidate was then tried once on a fresh manual post-HV boot.
The current-boot AP receipt passed, but the loader stopped at `info_map`
before any ShellCore patch and skipped follow-on work. The operator confirmed
the home screen remained responsive. Source review found a second ordering
error: the preimage gate used kstuff's remote syscall before that facility
was installed. A third candidate places the gate after uELF/IDT/syscall
setup and before the ShellCore patch call; its 34 compiled rows pass offline
verification. This ordering permits earlier kernel changes before the gate,
so a gate failure is not a clean rollback. The third candidate has not run.

The third candidate subsequently ran once after a matching current-boot
Byepervisor receipt. It reached its target-process module-info syscall but
returned `reason=info_syscall` before the 34-site guard and patch loop.
The operator confirmed the home screen remained responsive. Source review
found that the call supplied handle zero; the repository's module-listing
path obtains handles first. A fourth private candidate uses that listing,
requires two independent live byte anchors and all 34 original-byte checks,
and has passed compiled-table checks offline. It has not run on the console.

The fourth candidate ran once on a fresh post-HV boot. Its module-list
syscall returned `-1`, and the loader stopped before ShellCore writes; the
operator reported a responsive home screen. The saved 13.60 kernel capture
has actual handlers at syscall entries 592 and 608, leaving the remote
dynamic-library failure unresolved. A separate read-only VM-map diagnostic
found a plausible large executable ShellCore segment. A fifth candidate
uses that ordinary process-map route for the early two-anchor and 34-site
byte gate, ahead of IDT/kernel-data installation. Its compiled table passed
offline verification. On a fresh post-HV boot, the separate read-only
VM-map diagnostic succeeded before kstuff. The fifth loader then matched all
34 original ShellCore site bytes and returned an inner-payload failure. The
outer loader skipped app.db/ShellUI/USB follow-ons, and the operator reported
a responsive home screen. Because the failure reason was notification-only,
the host stream does not identify the failed patch step or prove rollback.
The next build needs a bounded host-visible failure reason and a fresh boot;
loaded-kstuff and fPKG behavior remain unproved.

A sixth private diagnostic also passed the 34-site live preimage gate, but
its added inner stdout message did not reach the host; the outer loader again
reported only a nonzero result, and the console remained responsive. A
seventh candidate encodes the existing failure reason in that return value
and prints it from the outer loader. Its 34 compiled patch/preimage rows pass
offline verification. This candidate has not run on the console.

The seventh candidate was sent once on a fresh post-HV boot after a passing
AP receipt. The host received only the outer loader's initial line, then
timed out; the operator reported a kernel panic. No preimage result or
numeric failure code was observed. Its full ELF kept the same segment layout
and entry point as the sixth candidate, but its embedded payload location
shifted by `0x6d` in the linked image and its compiled inner bytes changed.
These static differences do not establish the panic's cause. Further console
testing is on hold pending offline early-path analysis.

An eighth offline candidate moves the numeric diagnostic helper to reduce
unrelated code layout changes. Its aligned embedded payload differs from the
sixth candidate in 242 bytes, and its compiled 34-row table checks pass.
It has not run on PS5. The saved 13.60 ShellCore ELF also contains all seven
imports required by the hook as PLT relocations, ruling out missing on-disk
imports as a simple explanation for the earlier post-preimage failure; the
live dynlib layout and actual failure reason remain unknown.

The saved kernel-data capture also supports the published 13.60 TSS array
stride: all 16 records at `0x68` spacing have populated RSP0 and IST4 fields,
while IST3 and IST7 are zero in that capture. This is one non-atomic boot,
so a future loader run still needs live preimage, CPU-owner and restoration
checks before relying on its IST writes.

### Current gate order after offline V5–V8 reconciliation

The V5 and V6 host streams prove the 34-site original-byte check passed and the inner loader returned failure. Source flow points to ShellCore patch or dynamic-hook failure as the ordinary return path, but the exact operation and rollback state were not captured. The V7 stream contains no inner checkpoint; the operator reported a kernel panic. Its early stop cannot be attributed to the numeric diagnostic helper or another specific instruction. V8's smaller binary delta does not qualify it for live use. The next live action requires explicit operator resumption and an observed recovery state.

After resumption, use one supervised, fresh-boot test at a time: exact artifact hash; new V3 arm/completion receipt; one expected checkpoint; byte-exact durable log; stop on mismatch, missing output or instability; and a recovery plan before sending. The first downstream capability gate is a reliable loaded module with an independent health check. Then require ShellCore applied-byte and rollback readback, runtime import/GOT and PPR hook verification, original mount return and synthetic-pair cleanup, exact 13.60 A53 evidence or a separately justified alternative, and finally a known-good fPKG's install, mount/read, process or visible execution, and cleanup. Neither installer `playable`, PPR stage 9, nor an ELF socket send meets that final gate. Automatic post-wake delivery remains lower priority than the manual receipt-gated route.

Offline A53 preparation now includes a private, built and host-tested resident LAN diagnostic that can submit serial, 64-byte maximum physical or EL3 virtual reads, log raw SDBGP result fields, and checkpoint one known 4 KiB page per host capture. As of 13 October it had **not** run on PS5 and did not supply a 13.60 A53 exploit, image, profile, or panic-free memory dump. The old anchor reads used the physical selector while their public reference used EL3 virtual addressing. On explicit live resumption, establish recovery and current-boot HV state first; then select one tiny A53 transport request as a distinct supervised test before any page capture or package-trigger experiment. Stop after instability.

### 14 October 2026 live A53 checkpoint

After explicit resumption, the current-boot V3 receipt passed and the private, hash-pinned A53 toolbox executed once. Its resident LAN service answered `GET_CONF` with the retail 13.60 version. A single 4-byte EL3 virtual read at the historical anchor returned a parsed result with `status=0x20` and zero bytes. The operator reported the console responsive after the read and after the toolbox answered `STOPPED`; the outer logging socket reset without a clean exit line. This establishes a working version command but **no A53 memory read, 13.60 image/profile, PPR selector, kstuff health, or fPKG result**. Investigate the read status and obtain a verified 13.60 map/preimage offline before choosing a further bounded A53 request. Do not infer that every A53 address is blocked from one anchor result.

### 14 October map and HYPERHEN audit update

A separate 64-byte read of the same historical anchor returned the same `0x20`/zero-byte result; the toolbox then stopped with a host-visible exit checkpoint. A read-only MP4 MDSR header probe and a distinct bounded log capture both **executed** after current-boot V3 receipts. The capture passed per-chunk CRC, contiguous-offset, stable-header and clean-exit checks. Its private boot logs label the old anchor as a PSP-staged A53 ELF **physical** segment and show current EL3/EL0 mappings. This gives a specific 13.60 map to reason from, but neither A53 image bytes nor a successful SDBGP memory read. The next test, if any, must choose one map-derived address and selector, pin the exact build and current-boot receipt, preserve full result fields, and stop after a mismatch or instability. Do not chain probes or attempt a write from map labels alone.

Source review of HYPERHEN commit `6f9b36eb2adc2fbaaf96366ede05cf0ca94870ce` found no bundled 13.60 A53/PPR profile: its exact A53/PPR table ends at 11.40. Its 13.x branch accepts a Hypervisor readiness marker and explicitly **skips** PPR installation. The project documents 5.00 combined-core console-log validation and says 13.x end-to-end validation remains open. Treat its HV route as an upstream integration hypothesis, not proof that HV access alone supplies the A53 plaintext/fPKG behavior required here. No HYPERHEN payload was sent to the console.

The functional gate order is unchanged: independently healthy kstuff load; ShellCore applied-byte and rollback readback; live import/PPR-hook behavior; synthetic key-pair release; exact 13.60 A53 evidence or a separately demonstrated alternative; then a known-good homebrew fPKG's install, mount/read, process or on-screen execution and cleanup. Current PS5 instructions: leave the responsive console at the home screen with elfldr available if continuing this boot; do not launch HYPERHEN, V7/V8, ps5debug or FTP as part of this offline conclusion. Recheck a fresh current-boot receipt before any further controlled payload, and stop after instability.

One subsequent **executed** read used the log's loaded EL3 segment virtual base with the same pinned toolbox and a fresh same-boot V3 receipt. It also returned parsed SDBGP `status=0x20` and zero bytes. The toolbox stopped with a clean outer exit checkpoint; port 9021 remained reachable. The physical-versus-virtual anchor mixup alone therefore does not explain the read failure. Stop address trials pending a decoded status or a separately justified access route. A reachable port remains only liveness evidence; there is still no 13.60 A53 image read, exact PPR selector, functional fPKG mount or title execution.

A separate x86 MP4 aperture test checked two known MDSR controls and attempted one 16-byte read at the boot log's A53 ELF-staging offset. The controls matched; the target returned all `ff`, not an ELF header. The first version had stopped before its data read on an overstrict cross-address-space comparison; both builds and raw logs remain private. This route has not yielded an image and must not be expanded into a blind aperture scan. Current next gate is an evidence-backed explanation of the read restrictions or another independently validated way to acquire exact 13.60 A53 image/preimages. The console's reachable elfldr port after these tests is only a liveness signal. No further live payload is queued.

An executed, private syscall-boundary tracer trial reached the A53MM owner on a native game launch but captured no ioctl entry or completion. Two arm-before-launch variants were followed by a frozen game, including one that detached with signal zero before syscall tracing. The operator manually closed each game instance and confirmed a responsive home screen. Suspend game-process ptrace while its stop and detach behavior is analyzed offline. A listener READY, successful brief attach check, or detach return is not functional evidence. The remaining PPR, key-cleanup, A53 13.60 and known-good fPKG mount/execution gates above remain open.

Later fresh-receipt A53 transport work found a narrow positive exception to the earlier `0x20` private-memory results. Strict physical reads returned one eight-byte value at PA 0 and two separately captured 4 KiB low-ROM pages at PA `0x1000` and `0x2000`, each with saved-log verification and clean STOP/outer exit. A separate private batch candidate then executed once: 16 four-byte physical reads in **one** SDBGP transaction exactly matched the previously captured PA `0x1000` prefix. This is an executed bounded batch-read capability for that ROM range. It does not imply access to the DRAM-staged A53 image, establish a 13.60 A53/PPR exploit, or satisfy any kstuff/fPKG functional gate. The next memory acquisition should use a justified region and per-packet error/partial-output checks, not a blind address sweep. Private ROM data, logs, IDA files, source and token-bearing ELF remain off this branch.

The subsequent bounded read stopped at its first failed page after validating 191 contiguous low physical pages through PA `0xbffff`; PA `0xc0000` returned `status=0x20` and no bytes. Offline IDA 9.4 analysis of the assembled private range now uses previously observed A53 virtual aliases, which resolves many direct controller calls that appeared external in a flat physical import. The alias map is from a different boot and remains an offline hypothesis for this capture. It exposes controller/ring and bounds-checking code but does not recover the staged A53 image, explain the private-memory `0x20` response, or yield a 13.60 PPR profile. No new live probe followed the first failed page. Before any further A53 test, select a specific missing command/handler boundary with preimage and recovery evidence; the v1 recorder panic and v2 hook/storage risks are still open. Do not replace the kstuff, ShellCore, key-lifecycle and real fPKG execution gates with this static map.

An offline command-copy review ruled out an apparent stack-overwrite path in two handlers by checking their exact ARM branch flags. A separate generic copy remains a **conditional** hypothesis because its destination is smaller than the full header count range, but the captured bytes do not establish a reachable caller permitting an excessive count. Resolve the 13.60 dispatcher/caller precondition before proposing an exploit or live command. No exploit, A53 image, PPR profile or fPKG capability was gained from this review.

The saved x86 indirect-buffer sender's entry check accepts nine words, which narrows one possible upstream cap. It does not establish that such a command reaches the conditional A53 helper. Exact A53 dispatch, count validation and ring-length evidence remain the next gate; no live probe is queued from this static check.

A later, single **executed** read targeted the missing A53 dispatch data after the saved boot log placed it in loaded controller text. The hash-pinned toolbox used a fresh current-boot HV receipt; the SDBGP response matched the command but returned `status=0x20` and zero bytes. STOP and outer exit were observed. This access route still cannot supply the table or a 13.60 A53 image. The conditional copy path therefore remains an offline hypothesis; obtain its exact 13.60 caller/count constraints or a separately validated image/trace path before considering another live A53 command. No kstuff, PPR/key lifecycle, mount, or title-execution gate has changed.

Offline comparison now suggests that the conditional helper is a generic MM-controller allocator, but does not establish a 13.60 caller. The table targeted by the failed read is used by a separate visible dispatcher and would not, on its own, prove that this helper is reached. Prioritize acquiring exact matching-build MM-controller caller code or callback data, then validating the permitted header count and source-ring length. Only a safe, attributable A53 entry trace of a genuine command would turn that static path into runtime proof; the previous text-hook recorder risks remain unresolved.

A further offline pass checked address-forming instructions across every aligned slot of the captured executable A53 alias ranges. It found no reference to that helper or its mapped page in those ranges. Exact low-code windows were also absent from the saved x86 captures and both saved PUP forms; these windows are not signatures of the missing high controller image. The acquisition priority is therefore unchanged: obtain byte-verified matching-build high controller caller/callback data or a separately safe A53 entry trace, then tie a genuine command header and completion to the helper. The captures cannot establish the helper's caller constraints, and no new live test or fPKG gate followed this search.

The saved 13.60 x86 sender also resolves a misleading count comparison. Its accepted indirect-buffer length is encoded in a later descriptor word; its outer ring header has a fixed two-element count field. Accepting a nine-element buffer through that ioctl therefore does not present a nine-element outer header to the A53 helper. If the helper consumes that outer header, this producer cannot exercise the suspected excessive copy. The helper's actual caller, any inner-buffer header, and other producers remain unverified. Do not submit a crafted nine-element command from the old count-gate observation.

The previously tested x86 aperture offset is identified by the saved boot layout as reserved controller data, while the PSP-staged A53 ELF occupies a separate G6 range. Its `ff` result cannot be used to conclude the ELF image was read or denied, and G6 offsets cannot be applied as if the x86 source buffer were a verified linear mirror. Prepare a bounded command-time recorder for a genuine A53MM client only after replacing the prior freezing/text-hook designs with independently checked installation and rollback. Pair submit bytes with a completion, then resolve the A53 caller/header path before a targeted helper-reachability test.

The immediate offline gate is the recorder installer: verify 13.60 executable kernel allocation/protection and the A53MM device-switch indirect-call path before any data-pointer redirection. The current generic core lacks an installer, and V2's live text-jump/storage risks remain. If those prerequisites cannot be proved, use another passive observation path rather than send either recorder ELF. A command-time x86 trace would support, but not itself prove, A53 helper reachability or fPKG support.

The saved 13.60 image now supports exact allocator/protection candidates and a dynamic A53MM `d_ioctl` dispatch with the recorder's five-argument ABI. The next gate is a *bounded kernel execution and allocation smoke*: prove an allocated page can be filled, made executable, read back and entered, with current-boot page-table and recovery evidence. SDK kernel read/write alone cannot call the allocator or promise an atomic pointer swap. Only after that smoke should an installer link/relocate retained recorder code and compare-and-swap the exact live device-switch slot. Restoring the slot is not sufficient proof that no CPU still executes the wrapper; retain its code until quiescence is proved or the boot ends.

The unused hook call helper now has an offline stack-alignment correction and host contract tests, and the saved 13.60 return/result-store gadget bytes are pinned. A private host-tested 64-bit compare-and-swap helper provides expected-value install and rollback semantics. Neither is a live kernel-call or recorder result. At that checkpoint the next controlled entry test was to capture one short leaf function's result and independently read back gate/IST restoration. The demonstrated entry is during S3 wake; a post-wake test hook would need its own multicore/exception safety case. Do not place `kmem_malloc` in the wake handler merely because the leaf smoke succeeds. Its calling context, stack depth, returned allocation, page permissions and cleanup still need separate proof before any A53MM slot change.

At the preparation checkpoint, the first bounded call test was an **offline-built, unsent** opt-in Byepervisor ELF, distinct from the recorder. It copies one sentinel qword through the 13.60 `memcpy` candidate in the existing S3 hook and captures destination, return pointer and a done marker. The independent read-only `call_probe_status_1360` source verifies these alongside the current-boot V3 attempt receipt and gate/IST restoration. Host hook tests passed eight contract cases; the opt-in ELF SHA-256 is `362eb19e6e1d5f18b24fe42c420ad2f6f3a4e72b7e587b62ea70cca0f35c922f`, and the status ELF SHA-256 is `40303a01e6cedf9d74cbab203a5dc36751699fd914c8f929c50103805171ea14`. At that checkpoint those hashes identified **builds**, not PS5 results. Do not send either to an already completed Relapse+Byepervisor boot; the wake-hook source refuses re-arming there. A fresh Relapse-only boot is needed only when this specific S3 test is ready to run. A successful call marker would still leave allocator context, executable placement and A53MM recorder installation open.

### 15 October read-only slot check and caller-reference triage

A private, read-only diagnostic was **sent once and executed** after a valid
current-boot V3 receipt. It read the expected original A53MM `d_ioctl`
pointer, a nonzero kernel-arena pointer and a present guest RW mapping for
the slot; elfldr accepted a connection after it exited. This establishes
the live slot preimage and mapping on that boot. It did not invoke an
ioctl, change the pointer, allocate executable code, or test the recorder.
The full raw result and boot-specific addresses remain private.

HyperHEN's vendored `ps5-hen-dev` source contains a syscall-`0x11`
`kexec` caller, but its bootstrapper uses that engine only for firmware
1.xx–4.51. The 13.60 route selects a separate Byepervisor stage and
HyperCore. Our saved 13.60 syscall `0x11` entry is marked absent, and
the exact legacy `jmp [rsi]` gadget had no instruction-aligned match in
the analyzed saved text. This rules out treating HyperHEN's bundled
legacy caller as a ready post-wake 13.60 service; alternate designs remain
unexamined. At that checkpoint the opt-in S3 call probe was still **built, unsent**.

The user-linked [MM PS5 API Mapper](https://github.com/Michele-M-Media/MM-PS5-API-MAPPER)
offers a read-only process, FD and bus-driver topology reference. A
separate bounded 13.60 diagnostic was **sent once and executed** after a
valid current-boot receipt. Its bus walk ended consistently after 123
nodes, with `sbl0` (`Oberon PSP`) and `mp40` among five filtered matches.
Thirty-two device names were unreadable or failed validation, so the
names do not provide complete semantic coverage. This identifies live
x86 device topology; no bus method, A53MM ioctl or A53 helper was invoked.
The mapper does not supply kernel execution, atomic pointer redirection,
an A53 image or fPKG support. The installer gate remains a safe
kernel-call and executable-allocation smoke, followed by an independently
checked A53MM pointer swap and rollback.

### 15 October S3 caller and retained-carrier boundary

The saved 13.60 allocator's known `0x102` call flags include `M_WAITOK`.
On page shortage, the mapped function can wait and retry. The S3 #DB
call chain uses data-cave scratch as its stack, so a successful leaf
`memcpy` probe would not qualify that blocking allocator call in the
same context. At that checkpoint the opt-in leaf probe remained **built, unsent**. A normal
kernel execution context must be verified before an allocation smoke.

A separate receipt-gated read-only diagnostic was **sent once and
executed** on the completed post-HV boot. The guest direct-map path for
the staged AP carrier had NX clear, but its first byte differed from the
pinned 508-byte Byepervisor AP build. The resident code identity and
post-wake callability remain unverified; no allocation or instruction
fetch was tested. At that checkpoint a fresh S3 call attempt needed a full console restart
to Relapse-only with elfldr before running Byepervisor once. Clearing
the completed receipt fields on the old boot is not a fresh attempt.

### 15 October S3 leaf-call live result (sent/executed)

On a fresh 13.60 Relapse-only boot, the exact opt-in Byepervisor ELF (SHA-256 `362eb19e6e1d5f18b24fe42c420ad2f6f3a4e72b7e587b62ea70cca0f35c922f`) was **sent once**. It armed the S3 hook and entered Rest Mode automatically. The initial sender timed out while the console remained asleep; that stream established arming only. After the operator woke the PS5, a separate hash-pinned read-only status ELF was **sent once and executed**. It reported a passing current-attempt V3 receipt, matching nonzero arm/completion IDs, AP-complete stage, zero hook state, restored gate/IST, matching source/destination sentinel, destination return pointer, and the call-done marker. A separately executed read-only log reader recovered a durable post-wake stage log with exactly one matching call-probe row after hook cleanup. These independent observations establish the bounded S3 `memcpy` call and return/result storage on this one wake.

The exact tested ELF embeds a private local build path in debug metadata and remains private; a stripped or rebuilt ELF would have a different, untested hash. No allocator or recorder ELF was sent. This call does not prove safe deep or blocking kernel calls on the S3 #DB stack, a callable post-wake kernel service, executable allocation, A53MM ioctl dispatch, atomic slot installation, PPR, key cleanup, or fPKG mount and execution. The next installer gate is a separately justified normal kernel call context, followed by bounded allocation/execute/readback and recovery evidence before any A53MM pointer change.

### 15 October normal-context caller review (offline)

The saved 13.60 syscall dispatcher calls a syscall handler on the calling thread's kernel stack and resumes with its return value. Its call target passes through a fixed, eight-byte-spaced CFI stub range; an arbitrary new handler address is outside that range and reaches a fatal error path in the saved state. The absent syscall `0x11` uses a stub shared by 239 PS5 entries and 607 combined PS5/PS4 entries, so changing that shared jump would affect unrelated calls. An apparently unused CFI-range slot is not yet proven unowned or safe to repurpose. The live slot diagnostic and S3 leaf-call proof do not close this post-wake bridge.

An exact disassembly of the previously **executed** private 13.60 kstuff observation payload found the same `r0gdb_kfncall`, `r0gdb_kmalloc` and allocation-loop machine code as the retained local slices. The sent image contains 384 small `r0gdb_kmalloc` calls followed by a 4 MiB block-allocation request that retries on zero. Later loader and reader observations support progress through that historical path, but no allocation address, page readback or clean uninstall was recorded. It is the ordinary `malloc(size,type,M_NOWAIT)` path and still depends on r0gdb's global IDT/TSS instrumentation, whose separate 13.60 cleanup probes failed. This is **offline binary reconciliation plus prior executed evidence**, not a verified safe `kmem_malloc` caller.

No normal-context allocation test is ready. A bounded future test first needs an independently owned executable wrapper, an isolated call target accepted by the 13.60 dispatcher or another ordinary-thread callback, same-boot preimage and return/rollback checks, and a leaf-call marker on that context. Keep the allocator, A53MM pointer swap, recorder and fPKG tests behind those gates.

One existing `sysarch` operation does invoke the mapped allocator on an ordinary syscall stack, but it is the privileged `I386_SET_IOPERM` path: allocation is conditional, uses waiting flags, retains the pointer in thread state, and installs TSS/I/O-permission state. It does not return the allocation address or offer a small reversible smoke. This was established from saved bytes and source headers only; the operation was not run.

### Narrower normal-stack marker (offline)

Saved 13.60 bytes identify an existing no-argument, static syscall record and a separate existing CFI-approved leaf that returns a fixed error. A one-byte change to that record's call pointer could provide a bounded normal syscall-stack call/return marker without staging code. This is an offline candidate, not a live preimage or verified rollback. It temporarily changes one syscall globally, so a same-boot record/target check, exact write/readback, single invocation, compare-guarded restore/readback and durable result are mandatory before using it. It does not qualify an allocator, executable wrapper, A53MM observer, PPR or fPKG.

### PUP unpacker follow-up (offline)

The earlier 13.60 PUP work extracted only `kernel.bin`. A later run of the pinned public unpacker against the saved outer-decrypted PUP wrote 32 components from 33 descriptors; its `kernel.bin` exactly matched the old extraction and remained opaque. The two system components are readable exFAT images. A separate read-only directory walk found 769 files in `system_b` and 2,444 in `system_ex_b`. Selected `.elf`-named system files are SELF containers, not already decrypted ELF code. Neither directory listing names `a53.elf` or an obvious A53 firmware image; bounded string checks did not establish one. This corrects the prior broad description of both nested images as inaccessible. The unpacker does not decrypt those SELFs or the PUP kernel blob, produce a verified 13.60 A53 image/profile, or change any PPR/fPKG gate. Raw update components and full inventories remain private.

The separate public 13.60 live-kernel archive matches every byte of our saved 13,369,344-byte x86 text range. Its data differs on 5,382 overlapping 4 KiB pages and has a zero-filled gap that omits 128 mapped nonzero pages present in ours. This supports byte identity for the captured text range only; both live dumps have coverage limits and differ from the opaque PUP `kernel.bin`. It does not change the normal-call, A53, PPR or fPKG acceptance gates.

### ROM-key lead and normal-stack preflight stop (offline / sent, no checkpoint)

The public PSDevWiki PS5 key list names EAP KBL key families, and the saved 13.60 update contains two EAP KBL components. This is a format-research lead, not a verified 13.60 decryption recipe. No listed key has been shown to produce a validated 13.60 A53 image or to satisfy a PPR/fPKG gate.

A private read-only preflight for the narrower syscall-stack marker was built and matched against saved 13.60 bytes, then **sent once** on a user-reported Relapse + Byepervisor post-wake boot. The host received no payload output or current-boot checkpoint and timed out. Subsequent network checks failed, and the user reported a suspected kernel panic. Klogsrv was also running, with no demonstrated causal link. The exact failure point is unknown. This send does not establish execution, a valid receipt, normal-stack callability or rollback. The marker, allocator and recorder tests remain stopped pending offline failure review and a separate fresh-boot receipt. No fPKG capability follows.

An offline IDA follow-up found a concrete flaw in the existing `sysarch` allocator route: after a waiting allocation, the setter writes through the returned pointer without checking for zero, while the allocator has a saved zero-return path. On success it installs thread TSS/I/O state and exposes only status. It is a real normal-stack call edge, but not a failure-safe allocator smoke. A separate storage-controller latency-test ioctl allocates and frees memory on a normal ioctl stack while also issuing storage operations; it is likewise unsuitable for a first isolated allocation test. The private caller census covers recognized direct references only. The safe normal-context callback with controlled arguments, result readback and cleanup remains open.

The saved taskqueue worker provides another ordinary kernel-thread callback context, but checks its callback against a CFI stub range. Its enqueue path takes a lock. No compatible owned allocator callback, result channel, safe enqueue from the S3 hook, or queue-list mutation/rollback is established. This is an offline architecture lead, not a runnable allocation test.

A bounded saved-kernel screen found that none of the 268 accepted taskqueue callback stubs lands in a function with a recognized direct call to the mapped allocator. IDA's next direct-call step found no such caller either; one further step reached only a generic malloc path without a controlled allocation result. This narrows ready-made callbacks in the captured text, while indirect/deeper/module paths and nine undefined destinations remain unclassified. It does not justify taskqueue injection or an allocator send.

A follow-up **offline saved-byte** check decoded 64 bytes at each of those nine IDA-undefined destinations. All contain executable-looking instructions; one performs another indirect-call CFI check, and some start with conditional tests before a frame prologue. None has a direct allocator call among its first 12 decoded instructions. Their full boundaries and indirect/deeper reachability remain unproved, so this does not create a safe allocator callback or change the stopped live-test state.

One of the eight callbacks reaching the generic malloc intermediary was checked more closely. It ignores the task callback arguments, chooses its allocation size from global object counts, installs the result into a global table, rehashes entries and conditionally frees the old table. Its zero-return check does not make it an isolated allocator probe. The other roots remain bounded leads; no callback was run.

The saved-text call windows for all eight transitive malloc callbacks were then checked. Their observed sizes are fixed or derived from existing service state; returned pointers feed service operations or shared structures, sometimes without an immediate zero check. None of these bounded windows supplies task-argument controlled allocation, isolated pointer readback and reversible cleanup. Full indirect and deeper behavior remains open. A new Byepervisor allocation ELF is therefore still an offline design task, not a ready live test.

On a later recovered post-HV boot, the already tested read-only V3 status ELF **executed once** and independently reported the expected firmware, matching nonzero arm/completion IDs, AP-complete stage, zero hook, and restored gate/IST. The raw receipt and boot identifiers are private. This confirms that specific HV attempt only. The silent syscall preflight was not resent, and no allocator or kernel-write payload followed.

A further **offline** saved-text census reconciled 50 recognized direct edges into the mapped allocator as 48 calls and two tail jumps. One tail wrapper has five fixed-size callers embedded in service operations; the other writes through an incoming pointer before allocation. A larger helper has a zero-checked allocation and output pointer, but its three aligned direct callers are two substantial service routines with internal state and error handling. None of these observed paths provides a small controlled allocation, independent pointer readback and reversible cleanup. This screen does not exclude indirect or loaded-module callers. The normal-context allocation test is still unbuilt and unexecuted.

A new read-only observer with an immediate first-entry checkpoint was **sent once** after that V3 receipt. It printed the entry line, then gave no identity/read checkpoint or socket EOF within its bound. One subsequent connection to each of elfldr and FTP timed out. Exact-ELF disassembly places a newly imported `fflush(stdout)` between the entry print and the firmware getter; the stream cannot localize the stop to either call or establish a panic cause. No syscall call, kernel write, allocator or A53 operation was reached by evidence. Both preflight ELFs are retired from live use; stop until a fresh console state and an independently reviewed test exist.

A further **offline** screen followed the previously undefined task callback through its second CFI range. All 408 saved jump stubs were well formed. Across the 361 IDA-defined destination functions, bounded linear decoding found generic allocation calls but no direct call to the mapped allocator or its reviewed wrappers. The tempting fixed-size allocation/free pairs are registered process/thread lifecycle handlers and write into live object fields. Taskqueue enqueue may also invoke its notification callback before returning, so an S3 caller is not guaranteed to hand all work to a normal worker stack. No owned task, isolated result/cleanup channel or safe enqueue path is established; no allocation payload is ready.

After the user reported another post-HV boot, the previously tested V3 receipt reader was prepared with its exact hash, but the prior console address timed out at TCP connect before any ELF byte was sent. The reported boot therefore has no independent current-boot receipt from this attempt. The current service address is being checked; the stopped syscall preflights were not resent.
