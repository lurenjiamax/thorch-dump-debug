# Thor dump diagnostics

Requires Qualcomm hypervisor watchdog support and the matching
Thorch kernel. 

Enable OEM capture with `sudo thorch-dumpctl enable`. Boot with
`qcom_scm.download_mode=full` using the existing Thorch boot-debug configuration. The diagnostic service refuses to start without both.
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
capture is not required for every investigation. 

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

The module must match the **running device kernel**.

The package records an exact `linux-thorch` dependency.

## Build and package

Build with `build-module.sh` on Thor.

Install with
`sudo pacman -U /path/to/thorch-dump-debug.pkg.tar.zst`, then follow the explicit
enable commands above. Check `uname -r`, `modinfo -F vermagic
thorch-dump-watchdog`, and `sudo thorch-dump-debug status` on the device.

## Capture and stop

```sh
mkdir -p /path/to/private/new-capture
cd /path/to/private/new-capture
edl memorydump --vid=0x05c6 --pid=0x900e --partitions=DDRCS0_0.BIN
```

DDR first-segment collection is 2 GiB on the validated Thor firmware; verify the
reported table. If transfer
stalls or reports USB errors, stop it and preserve partial files; Reconnecting may require reproducing the
fault. Log extraction requires the exact unstripped `vmlinux` from that boot.

After diagnosis run `sudo thorch-dump-debug disable`. This stops capture and
disables autostart, but intentionally leaves separately managed OEM/SCM flags
unchanged. Use `sudo thorch-dumpctl disable` and revert the boot-debug SCM setting
separately if firmware dump mode is no longer wanted.
