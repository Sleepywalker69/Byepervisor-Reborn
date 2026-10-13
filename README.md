# Byepervisor Reborn
**Live status, 13 October 2026:** The logged 13.60 ELF in `dist/` froze one
regular PS5 after the loader printed `Hook installed`, before the `Armed`
notification appeared. Do not relaunch this exact build. Its console log has
not yet been recovered, and the stopping instruction is unknown.

**Note: This repo should be considered a base exploit for work to be built on top of. It is not useful to regular users in its current release state, this is intended for developers.**

## Summary
PS5 hypervisor exploit for 6.xx - 13.60 firmware. Bug details can be found [here](https://github.com/soniciso1/ps5-hv-writeups/blob/main/s3-resume-startup_mp-ap-launch.md). This bug was initially found by [theflow0](https://x.com/theflow0) and subsequently more people recently w/ assistance of LLMs. The TLDR is that on S3 resume, the `startup_mp` hypercall is invoked on core 0 to initialize the other AP cores (1-15), and takes a reset vector (`rip`) as one of the arguments for where the core will execute from. During this time on cores 1-15, an insecure Nested Page Table (NPT) config is applied. We can abuse this to run some shellcode that has R/W into the hypervisor's address space including the VM Control Blocks (VMCBs).

Very similarly to (one version of) [byepervisor of old](https://github.com/PS5Dev/Byepervisor), this exploits the rest mode path too :)

Upstream lists the following firmware targets. This fork's exact ELF has only
been built and host-tested; its 13.60 console behavior remains unverified:

- 13.60
- 13.20
- 13.00 (untested)

However this bug should technically be exploitable as low as 6.xx. With modifications, it could work on 14.00 as well, but this is not provided here, especially since there is no public 14.00 kernel exploit anyways.

## 13.60 controlled-stage fork

This fork records separate resume-hook, AP-entry, AP-validation-failure, and
AP-completion states. It reports completion only after checking all 16 VMCB
records and reading back the intended control changes. It also checks the hook
restore path. These checks are host-tested; this exact ELF has not yet been run
on a PS5. An AP-completion notice does not prove persistent hypervisor control,
kernel text access, or XOM defeat.

This build leaves kernel text mappings untouched and does not read or write
kernel text. Rest Mode is entered manually. The test sequence and stop
conditions are in [TEST_PLAN_1360.md](TEST_PLAN_1360.md). The exact built ELF
is in `dist/`, with its SHA-256 in `dist/SHA256SUMS`.

Each notification and observed stage is appended and synced to
`/data/ps5-s3-1360/byepervisor-reborn-1360.log`. A missing or unwritable log
stops the ELF before the hook is armed. A shutdown before userland resumes may
still leave only the pre-sleep entries; the AP code itself does not perform
filesystem I/O.

## Important Notes
- You may need to go to Settings -> System -> Power Saving -> Features Available in Rest Mode
  - Set Supply Power to USB Ports to "Always"
- This payload is designed to run through the [elfldr](https://github.com/ps5-payload-dev/elfldr)
  - This is somewhat important because the payload expects to continue running on resume from rest mode, and it relies on elfldr for this
- This controlled-stage build does not attempt kernel text access or patching.
- This does not ship with any patches (HEN or otherwise)
  - Consider this a bring your own patches scenario, we didn't feel like doing this.
- This payload needs to be aware of whether or not the console is a pro or a non-pro unit
  - Sony decided to waste their added 2GB precious DDR5 that pro units have on the hypervisor, so it is mapped in a different physical address range
    - `0x062800000` PA for non-pro units
    - `0x8E0800000` PA for pro units
  - Open an issue if the payload mis-identifies what your system is, and note the hex value printed in brackets in the notification
- A shutdown during or after resume is an inconclusive failed test. Record the
  last visible stage and do not relaunch the same ELF on that boot.

## Build notes
This exploit payload is built using the [PS5-Payload-Dev SDK](https://github.com/ps5-payload-dev/sdk). Note also that the build for `break_hv_payload/` is slightly special, as it gets compiled to a flat binary of minimal size. The entirety of code in `break_hv_payload/` runs in supervisor/kernel mode.

Run the offline host checks with `sh tests/run_host_tests.sh`, then build with
`PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk make`. The host checks exercise stage
gating and hook error paths; they do not test a console resume or HV access.

## Adding firmware support
To add support for a different firmware, the main thing that needs to be added is the header file in `include/offsets/`. This includes all the kernel offsets that are needed.

Once the offsets have been added, the following functions need to be modified to add cases to the switch statements:
- `kgadget()`
- `kdlsym()`
- `rawoff()`

Some firmwares might require more extensive modifications if gadgets don't exist for them. 6.xx for example likely will not have some gadgets that are used on 13.xx.

## How to use
1. Run kernel exploit chain and get to elfldr
2. Send the exact `dist/byepervisor_reborn_1360_stage_logged.elf` build through elfldr.
3. Wait for the `Armed` notification, enter Rest Mode manually, wait for steady
   amber, and wake the console once. Observe the stage and cleanup notices.
   Full steps and stop conditions: [TEST_PLAN_1360.md](TEST_PLAN_1360.md).
