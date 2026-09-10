#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
version=$(sed -n 's/^QMODEM_VERSION:=//p' "$ROOT/version.mk")

[ "$version" = 3.4.0_rc3 ] || {
	echo "unexpected QModem package version: $version" >&2
	exit 1
}

case "$version" in
	*-*|*.*.*.*)
		echo "QModem package version is not APK-compatible: $version" >&2
		exit 1
		;;
esac

echo "PASS: QModem prerelease uses APK-compatible _rcN syntax"
