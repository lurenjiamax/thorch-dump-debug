# Thor dump diagnostic package

Maintain the optional kernel module in `module/`, control tool in `bin/`, and
Arch packaging/service in `packaging/`. User operation is documented in README.md.
Do not add device dumps, credentials, or private firmware. Package installation
must not arm capture; only explicit enable may do so. Normal prolonged sleep
must not be treated as failure. Preserve the prior hardlockup panic setting.
Build outputs belong outside this source repository. Validate on the actual Thor;
do not trigger destructive crashes without explicit user authorization.
