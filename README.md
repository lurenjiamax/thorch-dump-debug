# Thor dump diagnostics

Requires Qualcomm hypervisor watchdog support and the matching Thorch kernel.
Installing the package does not enable capture.

```sh
sudo thorch-dump-debug enable
sudo thorch-dump-debug softlockup on
sudo thorch-dump-debug softlockup off
sudo thorch-dump-debug status
sudo thorch-dump-debug disable
```

`enable` prepares OEM dump and SCM full-dump settings and enables autostart.
Reboot if prompted; otherwise capture starts immediately.

Kernel panic and detected hardlockups trigger capture. Softlockup capture is
off by default; `softlockup on|off` saves the choice and applies it immediately
when capture is active. Long sleep alone does not trigger a dump.

`disable` stops capture, unloads the module and restores previous settings.
Reboot to apply restored boot settings. External boot-config edits are not overwritten.

On capture, preserve USB connection and avoid restarting before acquisition.
The expected firmware USB device is Qualcomm 05c6:900e. Collect using a host
Sahara-capable tool such as edl in a fresh capture directory. Full-memory
capture is not required for every investigation. 

## Components

- `module/`: panic notifier and Qualcomm hypervisor watchdog SMC bridge.
- `bin/thorch-dump-debug`: service control, prerequisite checks and restoration
  of previous detector, panic and firmware settings.
- `bin/thorch-dumpctl`: OEM dump flag status/change, with original-record backup.
- `packaging/`: one optional Arch package containing both tools and the module.

The service uses `armed=1 early_bite=1`. Detection, panic, firmware capture and host acquisition are
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

After diagnosis run `sudo thorch-dump-debug disable`.
