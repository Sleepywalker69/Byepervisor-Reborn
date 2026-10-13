# 13.60 kstuff PPR candidate: offline build record

Source base: `EchoStretch/kstuff-lite` commit
`48cd2c4ce62aa9ca0c2cc014c1e512f276ae7c17`. The local source tree is
a disposable checkout; the source change is reproduced by
`kstuff-lite-1360-ppr-optin.patch` in this directory. The patch makes
`get_ppr_profile()` accept `FWVER == 0x1360` only when compiled with
`-DKSTUFF_ENABLE_PPR_1360=1`. Its default behavior still declines every
firmware above 11.60. It does not alter the other 13.xx cases.
The 682-byte patch has SHA-256
`7db0ed71c588941e1173b916536cc278a8e60fe4340ea8df70acce4f98ca8c99`.

The private captured 13.60 kernel passed the source's unchanged
`tools/validate_ppr_offsets.py` static checks with version key `13_60`:
`checks=PASS count=0`. IDA additionally matched the paired XTS/CMAC
get-index and cleanup call targets. The coverage and exact addresses are
in `../DOWNSTREAM_AUDIT_1360.md`. This supports an offline candidate,
but it does not validate runtime key handling, secure-module responses,
cleanup under failure, or fPKG launch.

## Reproducible build inputs

- PS5 payload SDK v0.44 in the local cross-toolchain.
- SQLite 3.46.1 source ZIP from the PS5 package recipe: SHA-256
  `def3fc292eb9ecc444f6c1950e5c79d8462ed5e7b3d605fd6152d145e1d5abb4`.
  Its static `libsqlite3.a` was cross-built with
  `--host=x86_64-pc-freebsd --enable-static --disable-shared --disable-tcl`.
- The full loader `src/` had no direct zlib include or symbol use; its
  unneeded `-lz` link flag was omitted in this build. The SQLite link was
  supplied through a private `-L` path, and the matching generated
  `sqlite3.h` was placed first in the compiler include search path.
- `EXTRA_CFLAGS=-DKSTUFF_ENABLE_PPR_1360=1` was exported before invoking
  `make kstuff.elf`, so the nested `ps5-kstuff/uelf` build received the
  opt-in. The build log confirms that flag on the uelf compile line.

The unmodified PPR-disabled build produced a 1,523,688-byte ELF, SHA-256
`78e5186e6a55502c60300114acfed8037e5bd6aa34ed0889208f6a0e683b7edd`.
The opt-in build produced a 1,523,688-byte ELF, SHA-256
`fd3a5a355c954990c736a162c4dea0b681e64550798dc7475534d3ea3aee161b`.
Both are x86-64 PIE ELFs; the opt-in build linked with seven console shared
libraries and statically linked the private SQLite archive. The private
build log's SHA-256 is
`3bac300099c02b66c03f9b15f1bc2de8fd163628505fc90fde8add5e0682200d`.
The patch passed `git apply --reverse --check` against the edited checkout,
and the tracked source passed `git diff --check`.

## Live boundary

Neither new ELF has been sent to or run on the PS5. A successful compile,
ELF transmission, or loader process start will not count as PPR support.
The loader also calls its resident entry, patches `app.db`, starts a Shell UI
thread, and monitors USB; these are separate side effects to account for
in any live test. A first run needs a fresh manual-mode post-wake receipt,
an exact build hash, a module-ready observation, a bounded PPR operation,
and a functional result with failure/cleanup checks. The private ELFs,
SQLite archive, build logs, and kernel capture are not included here.
