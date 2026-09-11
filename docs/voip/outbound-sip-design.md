# Outbound SIP design

## Scope

Outbound SIP makes the VoIP application a SIP user agent that registers to a
remote Asterisk endpoint. The modem media engine remains local and owns the
selected interface address; the Asterisk address is used only for SIP
signaling. This separation prevents advertising a non-local address in SDP or
binding the SIP socket to a remote host.

The feature does not embed a PBX, registrar, or RTP relay. Asterisk remains
the policy and distribution point.

## Configuration model

`qmodem_sip.inbound` and `qmodem_sip.outbound` are independent UCI sections.
Both directions may run concurrently. Outbound settings use these option names:

| Option | Meaning |
| --- | --- |
| `server` | Asterisk hostname or IP, validated to a safe token |
| `port` | SIP TLS destination port, normally 5061 |
| `transport` | `tls`; other values fail validation |
| `username` | SIP auth username |
| `password` | SIP auth secret; never logged |
| `realm` | Optional digest realm; learned when empty |
| `register_interval` | Refresh interval in seconds |

The local `interface` option continues to select the source address for SIP
and RTP. It must resolve to an address present on the device (for example
`wan` backed by physical `lan4`); it is never set to the Asterisk address.

## Lifecycle and idempotency

The init script validates configuration before creating a procd instance.
Invalid values stop with a non-zero result and do not open firewall ports.
Repeated `start`/`reload` operations are safe because runtime state is kept
under `/var/run/qmodem_voip` and each process owns one transaction loop.

The outbound transaction client uses REGISTER with digest authentication and
PJSIP automatic refresh. Asterisk replaces an older contact for the same AOR,
and the client attempts to unregister during an orderly shutdown. INVITE
authentication retries preserve the call identity, and every successful final
response is acknowledged on the same TLS transport using the remote Contact,
original From/Call-ID/CSeq, and final To tag. Runtime credentials are written
to a mode-600 file under `/var/run`, are reloaded through a process restart,
and are cleared from process memory during shutdown.

## Security and NAT

Public Asterisk connections require TLS with certificate validation against
the system CA bundle. Only SIP requests arriving on the device-originated TLS
connection are trusted in outbound mode. Store UCI config as mode 600, do not
include secrets in ubus status, logs, crash output, or docs, and open only the
bounded RTP range on the selected interface. Asterisk must rewrite contacts
and use symmetric RTP when the device is behind NAT.

## Implementation status

The implementation includes fail-closed UCI validation, certificate-verified
TLS transport, digest REGISTER and refresh, INVITE handling on the established
TLS flow, bounded RTP, and idempotent procd lifecycle. Registration against a
public Asterisk is part of the delivery validation. A feature is not considered
media-complete until a real modem call proves bidirectional non-silent RTP and
PCM counters; successful REGISTER or SIP 200 alone is not that evidence.
