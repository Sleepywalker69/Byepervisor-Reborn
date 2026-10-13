# 13.60 downstream binary audit

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

## Gate for downstream use

Do not count sending this ELF, its process startup, or the public
kstuff-lite 13.60 offset table as proof of fPKG/PPR support. A replacement
candidate needs a verified 13.60 PPR path with the source gate and ABI
reviewed, or an explicit narrower feature
goal, a module-ready receipt from the exact launched build, and a functional
test of that feature on a fresh manual-mode wake. Keep the current no-launch
post-wake default while selecting and testing one downstream module.
