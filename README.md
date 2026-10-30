# Thor dump diagnostics

Optional, kernel-version-matched AYN Thor diagnostic package. Installing does
not enable it. Requires Qualcomm hypervisor watchdog support and the matching
Thorch kernel. Firmware capture can expose all RAM, including private data;
keep captures private.

Enable OEM capture with `sudo thorch-dumpctl enable`. Boot with
`qcom_scm.download_mode=full` using the existing Thorch boot-debug configuration
and reboot if needed. The diagnostic service refuses to start without both.
OEM changes are backed up by dumpctl; disabling the service does not change
these separately managed persistent firmware settings.

```sh
sudo thorch-dump-debug enable
sudo thorch-dump-debug status
sudo thorch-dump-debug disable
```

Enable explicitly starts the service and enables it for subsequent boots.
Disable restores the previous hardlockup panic setting and unloads the module.
No normal-sleep timer is armed: long sleep by itself does not trigger a dump.
The kernel hardlockup detector must report a lockup for automatic panic capture;
not every firmware/CPU hang is detectable. A false-positive detector report can
also trigger capture. Softlockup panic is not enabled.

On capture, preserve USB connection and avoid restarting before acquisition.
The expected firmware USB device is Qualcomm 05c6:900e. Collect using a host
Sahara-capable tool such as edl in a fresh capture directory. Full-memory
capture is not required for every investigation. Forced power recovery may be
needed after collection. Never publish device dumps in this repository.

## Components

- `module/`: panic notifier and Qualcomm hypervisor watchdog SMC bridge.
- `bin/thorch-dump-debug`: service control, prerequisite checks and restoration
  of the previous `hardlockup_panic` value.
- `bin/thorch-dumpctl`: OEM dump flag status/change, with original-record backup.
- `packaging/`: one optional Arch package containing both tools and the module.

The service uses `armed=1 early_bite=1 suspend_timeout_ms=0`. The module retains
an experimental suspend timer interface for development; the supported service
never enables it. Detection, panic, firmware capture and host acquisition are
separate stages. `status` confirms configuration, not that every hang is captured.

## Kernel compatibility

The module must match the **running device kernel**, not the host PC kernel.
Use the same kernel source revision, configuration, generated headers and
`Module.symvers` as the deployed image. Equal `uname -r` strings alone do not
prove ABI compatibility. Do not force-load an incompatible module.

Kernel upgrades require rebuilding and installing a matching diagnostic package
in the same transaction. The package records an exact `linux-thorch` dependency.
The Python tools themselves are not kernel-ABI-specific.

## Build and package

A Thorch repository path alone is insufficient: first build the target kernel.
Point this project at its **completed kernel build directory**. With Thorch's
usual layout that is `thorch/build/kernel-build`; the whole Thorch checkout is
not otherwise required. `modules_prepare` alone is insufficient when symbol
versioning requires the kernel's generated `Module.symvers`.

Run from this repository on a Linux host, with an ARM64 cross-toolchain and
`make`, `modinfo`, and Arch `makepkg` installed:

```sh
export KERNEL_BUILD=/path/to/completed/kernel-build
export OUTPUT=/path/outside/this/repository/dump-build
CROSS_COMPILE=aarch64-linux-gnu- ./build-module.sh "$KERNEL_BUILD" "$OUTPUT/module"

export THORCH_DUMP_MODULE="$OUTPUT/module/thorch-dump-watchdog.ko"
# Use an aarch64 makepkg configuration when packaging on an x86_64 host.
mkdir -p "$OUTPUT/packages"
cd packaging
BUILDDIR="$OUTPUT/package-work" PKGDEST="$OUTPUT/packages" \
  makepkg --ignorearch --nodeps --nocheck
```

On native ARM64 omit `CROSS_COMPILE`. For another cross-toolchain supply its
prefix. No download or source rewriting is performed by `build-module.sh`.
The package derives `linux-thorch`'s version using the `-thorch` release naming
convention. For a nonstandard package revision, set
`THORCH_KERNEL_PACKAGE_VERSION` to the exact target pacman package version.
Never substitute the host PC's installed kernel package version.

Copy the resulting **aarch64** package to the Thor and install with
`sudo pacman -U /path/to/thorch-dump-debug.pkg.tar.zst`, then follow the explicit
enable commands above. Check `uname -r`, `modinfo -F vermagic
thorch-dump-watchdog`, and `sudo thorch-dump-debug status` on the device.

## Capture and stop

On the host, use a new directory for every capture (edl may delete an existing
`memory/` directory), for example:

```sh
mkdir -p /path/to/private/new-capture
cd /path/to/private/new-capture
edl memorydump --vid=0x05c6 --pid=0x900e --partitions=DDRCS0_0.BIN
```

DDR first-segment collection is 2 GiB on the validated Thor firmware; verify the
reported table. Collection may reset the device on completion. If transfer
stalls or reports USB errors, stop it and preserve partial files; process
existence is not evidence of progress. Reconnecting may require reproducing the
fault. Log extraction requires the exact unstripped `vmlinux` from that boot.

After diagnosis run `sudo thorch-dump-debug disable`. This stops capture and
disables autostart, but intentionally leaves separately managed OEM/SCM flags
unchanged. Use `sudo thorch-dumpctl disable` and revert the boot-debug SCM setting
separately if firmware dump mode is no longer wanted.
