#!/usr/bin/env bash
# Build only the optional module; never rebuild or rewrite the kernel source.
set -euo pipefail
if (( $# != 2 )); then
  echo "Usage: $0 KERNEL_BUILD OUTPUT_DIRECTORY" >&2
  exit 2
fi
root="$(cd "$(dirname "$0")" && pwd)"
kernel="$(realpath "$1")"
output="$(realpath -m "$2")"
case "$output/" in
  "$root/"*) echo 'Output must be outside the source repository' >&2; exit 1 ;;
esac
for input in Makefile .config Module.symvers include/config/kernel.release; do
  [[ -s "$kernel/$input" ]] || { echo "Missing built-kernel input: $kernel/$input" >&2; exit 1; }
done
mkdir -p "$output"
make -C "$kernel" ARCH="${ARCH:-arm64}" \
  CROSS_COMPILE="${CROSS_COMPILE:-}" M="$root/module" MO="$output" modules
release="$(cat "$kernel/include/config/kernel.release")"
vermagic="$(modinfo -F vermagic "$output/thorch-dump-watchdog.ko")"
[[ "$vermagic" == "$release "* ]] || { echo 'Module release mismatch' >&2; exit 1; }
printf 'Module: %s\nKernel release: %s\n' "$output/thorch-dump-watchdog.ko" "$release"
