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
