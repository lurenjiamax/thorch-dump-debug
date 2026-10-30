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

Build `module/` against the matching configured ARM64 kernel build, using an
external module output directory. Package with `makepkg` in `packaging/`, setting
`THORCH_DUMP_MODULE` to the resulting module. Build products stay outside source.
