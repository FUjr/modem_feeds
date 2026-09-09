#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/bin" "$TMP/etc/config"
cat >"$TMP/bin/uci" <<'EOF'
#!/bin/sh
key=$3
case "$key" in
qmodem_voip.sip.mode) printf '%s\n' "${QMODEM_MODE:-lan}" ;;
qmodem_voip.sip.engine) printf '%s\n' "consumer" ;;
qmodem_voip.sip.outbound_server) printf '%s\n' "${QMODEM_SERVER:-}" ;;
qmodem_voip.sip.outbound_port) printf '%s\n' "${QMODEM_PORT:-5061}" ;;
qmodem_voip.sip.outbound_transport) printf '%s\n' "${QMODEM_TRANSPORT:-tls}" ;;
qmodem_voip.sip.outbound_username) printf '%s\n' "${QMODEM_USER:-}" ;;
qmodem_voip.sip.outbound_password) printf '%s\n' "${QMODEM_PASS:-}" ;;
qmodem_voip.sip.outbound_realm) printf '%s\n' "${QMODEM_REALM:-asterisk}" ;;
qmodem_voip.sip.outbound_register_interval) printf '%s\n' "${QMODEM_INTERVAL:-300}" ;;
esac
EOF
chmod +x "$TMP/bin/uci"

PATH="$TMP/bin:$PATH" "$ROOT/files/usr/sbin/qmodem_voip_sip_validate"
QMODEM_MODE=outbound QMODEM_SERVER='pbx.example.net' QMODEM_USER=u QMODEM_PASS=p \
	PATH="$TMP/bin:$PATH" "$ROOT/files/usr/sbin/qmodem_voip_sip_validate"
QMODEM_MODE=outbound QMODEM_SERVER='bad value' QMODEM_USER=u QMODEM_PASS=p \
	PATH="$TMP/bin:$PATH" "$ROOT/files/usr/sbin/qmodem_voip_sip_validate" >/dev/null 2>&1 && exit 1 || :
QMODEM_MODE=outbound QMODEM_SERVER='pbx.example.net' QMODEM_USER=u QMODEM_PASS=p \
	QMODEM_INTERVAL=30 PATH="$TMP/bin:$PATH" \
	"$ROOT/files/usr/sbin/qmodem_voip_sip_validate" >/dev/null 2>&1 && exit 1 || :

grep -q 'pjsip_auth_clt_reinit_req' "$ROOT/src/sip_consumer.c"
grep -q 'send_outbound_ack' "$ROOT/src/sip_consumer.c"
grep -q 'pjsip_tx_data_set_transport(ack, &selector)' \
	"$ROOT/src/sip_consumer.c"
grep -q 'app.outbound ? "sips" : "sip"' "$ROOT/src/sip_consumer.c"
echo 'PASS: outbound SIP topology validation accepts TLS and rejects unsafe values'
