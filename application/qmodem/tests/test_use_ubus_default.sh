#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
REPO=$(CDPATH= cd -- "$ROOT/../.." && pwd)

grep -q 'use_ubus", section); uci_set(key, "1")' \
	"$REPO/application/modem_scan/src/modem_scand.c"
grep -q 'config_get use_ubus .* use_ubus 1' \
	"$ROOT/files/usr/share/qmodem/modem_dial.sh"
grep -q 'use_ubus=${use_ubus:-1}' \
	"$ROOT/files/usr/libexec/rpcd/qmodem"
grep -Fq "o.default = '1';" \
	"$REPO/luci/luci-app-qmodem-next/htdocs/luci-static/resources/view/qmodem/network_config.js"
grep -q 'use_ubus.default = "1"' \
	"$REPO/luci/luci-app-qmodem/luasrc/model/cbi/qmodem/dial_config.lua"
grep -q 'option_string(uci, section, "use_ubus", "1")' \
	"$REPO/application/qmodem_smsd/src/main.c"

echo 'PASS: use_ubus defaults to enabled across creation and runtime paths'
