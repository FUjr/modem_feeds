# QModem VoIP interface reference

This document describes the experimental implementation inspected on
2026-09-07. The owning application repository remains authoritative while the
contract is evolving.

## ubus object

Object: `qmodem_voip`

Event topic: `qmodem_voip.call`

Read operations return a flattened object. Successful call-control mutations
return `status: "ok"` followed by the current redacted snapshot and media
fields. Errors return:

```json
{"status":"error","error":"invalid_state","message":"call is not in this state"}
```

### Methods

| Method | Request | Result and constraints |
| --- | --- | --- |
| `status` | `{}` | Redacted call snapshot and media status |
| `capabilities` | `{}` | Runs the read-only hardware probe and reports support/media fields |
| `enable` | `{}` | Journals baseline, configures the modem, starts media, and begins reconciliation |
| `disable` | `{}` | Restores baseline and releases browser/RTP/media resources |
| `originate` | `{"endpoint":"browser|lan_sip","number":"..."}` | Requires `idle`, armed modem media, and a valid dial string |
| `answer` | `{"endpoint":"browser|lan_sip"}` | First local answer wins |
| `reject` | `{"endpoint":"browser|lan_sip"}` | Valid for incoming ringing/early media; termination is idempotent |
| `hangup` | `{"endpoint":"browser|lan_sip"}` | Releases the active/setup call; termination is idempotent |
| `generate_sip_credentials` | `{"username":"..."}` | Generates a password, stores the account in UCI, and returns the password once |
| `call_history` | `{}` | Returns up to 100 completed calls, newest first, including missed-call classification |
| `issue_media_token` | session/revision/origin object below | Issues a single-use browser token for the active call |
| `attach_rtp` | RTP negotiation object below | Internal registrar boundary; attaches PCMA/PCMU RTP to the call |
| `release_rtp` | `{}` | Internal registrar boundary; detaches RTP idempotently |

`external_sip` remains a reserved endpoint in the call-control API during the
outbound SIP rollout. Unknown endpoints and client attempts to act as
`cellular` are invalid. Outbound registration is selected by the UCI
`qmodem_voip.sip.mode` setting described in
[the outbound design](outbound-sip-design.md); it is not an ubus endpoint.

### Browser media token

Request:

```json
{
  "session_id": "<authenticated ubus session>",
  "call_revision": 42,
  "https_origin": "https://router.example"
}
```

Success returns `token`, `expires_in` (30 seconds), and `call_revision`. The
token is at least 128 bits of encoded randomness, single-use, and valid only for
the active revision and bound origin. The daemon also verifies that the ubus
session has access to `issue_media_token`. Tokens never appear in status or
events.

### Internal RTP attachment

Request:

```json
{
  "address": "192.0.2.10",
  "port": 49152,
  "payload_type": 8,
  "session_id": 1234
}
```

`address` is IPv4, `port` is 1 through 65535, and `payload_type` is `0` (PCMU)
or `8` (PCMA). A nonzero integer `session_id` binds RTP packets to the current
SIP dialog. Attachment is allowed during outgoing setup, early media, or an
active call when the media engine is ready. Browser and RTP media ownership are
mutually exclusive.

These two RTP methods are daemon/registrar integration APIs. They are not
granted to the LuCI ACL and should not be exposed as general browser RPCs.

## Status fields

### Call snapshot

| Field | Type | Meaning |
| --- | --- | --- |
| `state` | string | `disabled`, `idle`, `outgoing_setup`, `incoming_ringing`, `early_media`, `active`, `terminating`, `recovering`, or `fault` |
| `enabled` | boolean | Experimental voice mode is enabled |
| `origin` | endpoint | Endpoint that initiated the call |
| `endpoint` | endpoint | Current modem-facing endpoint |
| `answer_owner` | endpoint | Local endpoint that won incoming-call answer ownership |
| `number_present` | boolean | A remote number is known |
| `remote_number` | string | Remote number when known and not withheld |
| `call_duration_seconds` | integer | Active call duration, otherwise zero |
| `sip_configured` | boolean | Valid UCI SIP credentials were loaded |
| `sip_username` | string | Configured local SIP username; password is never included |
| `caller_id_withheld` | boolean | Incoming caller ID was withheld |
| `revision` | uint64 | Monotonic call-state revision |
| `restart_epoch` | uint64 | AT-daemon restart generation |
| `sequence` | uint64 | Current modem-event sequence |
| `drop_count` | uint64 | Observed event loss count |
| `reconcile_pending` | boolean | A correlated `CLCC` snapshot is required |

Endpoint values are `none`, `browser`, `lan_sip`, `cellular`, and the reserved
`external_sip`.

### Media fields

| Field | Meaning |
| --- | --- |
| `media_engine` | `ready` or `not_ready` |
| `media_backend` | `serial_pcm`, `uac`, or `none` |
| `browser_media` | Browser WSS listener readiness |
| `rtp_media` | `attached` or `detached` |
| `media_url` | Runtime WSS URL when the listener is available |
| `canonical_format` | Currently `s16le/mono/8000/20ms` |
| `capture_rate`, `playback_rate` | Backend sample rates |
| `media_drop_count`, `media_underrun_count` | Aggregate queue health counters |
| `media_drift_ppm`, `media_tone_failures` | Timing and validation diagnostics |
| `serial_*` | Serial capture, poll, byte, EAGAIN, error, and reopen counters |
| `rtp_receive_packets`, `rtp_send_packets` | RTP transport counters |

Counters are diagnostic observations, not audio acceptance on their own.

## Capabilities

`capabilities` includes `status: "experimental"`, `supported`,
`support_state`, `reason`, `enabled`, `hardware`, `external_sip`, and all media
fields. `supported` is computed by the exact read-only probe. Consumers must
require both `supported: true` and `support_state: "supported"` before enabling
call controls.

## Events

Events are flattened objects containing `event` and the call snapshot. They do
not include credentials, raw AT lines, media tokens, or media counters. Current
event names are `enabled`, `disabled`, `originate`,
`answer`, `reject`, `hangup`, `fault`, `event_gap`, `caller_id`,
`reconcile_inconclusive`, `reconcile_idle`, `phantom_clcc_quarantined`,
`release_pending`, `call_state`, `ring`, and `release`.

On `event_gap`, an epoch change, a sequence discontinuity, or
`reconcile_pending: true`, consumers must request `status` and must not infer
intermediate state. Only the response or terminal event correlated to the
daemon's reconciliation command may clear recovery quarantine.

## Errors

Stable or currently emitted error codes include:

| Error | Meaning |
| --- | --- |
| `invalid_endpoint` | Endpoint is unknown or unsupported for the action |
| `invalid_number` | Dial string is missing or invalid |
| `busy` | The single call slot or answer owner is already occupied |
| `invalid_state` | The action does not apply to the current call state |
| `at_failed` | A modem command failed |
| `unsupported` | Hardware, endpoint, or runtime operation is unsupported |
| `restore_failed` | Baseline modem state could not be restored |
| `media_not_ready` | Call/media prerequisites are incomplete |
| `media_busy` | Another media owner is active |
| `invalid_media` | RTP negotiation input is invalid |
| `rtp_bind_failed` | The local RTP transport could not be attached |
| `invalid_credentials` | SIP credentials failed validation or storage |
| `activation_failed` | SIP service activation could not be completed |
| `invalid_session` | Browser session, revision, origin, or authorization is invalid |

Do not depend on a nonexistent `UBUS_STATUS_BUSY`; clients should use the JSON
`error` field as the stable application-level discriminator.

## rpcd ACL

The LuCI package defines two roles:

- read-only: `status`, `capabilities`, `call_history`;
- administrator: the read methods plus `enable`, `disable`, `originate`,
  `answer`, `reject`, `hangup`, `generate_sip_credentials`, and
  `issue_media_token`.

The ACL is the authorization boundary. Hiding or disabling a LuCI control is
only presentation.

## UCI

Package: `/etc/config/qmodem_voip`

```uci
config sip 'sip'
        option enabled '0'
	option username 'qmodem'
	option password '<generated>'
        option interface 'wan'
        option rtp_start '40000'
        option rtp_end '40031'
```

`interface` selects the OpenWrt network interface used to discover the LAN
address. The daemon generates the password from the kernel random source and
stores the authoritative account in UCI with mode `0600`. It derives the SIP
HA1 runtime file under `/var/run` at each start. SIP is disabled until valid
credentials are generated. The firewall
include follows the same enable state. Values are product defaults, not an
external-SIP configuration surface.

## QModem AT-daemon dependency

The adapter discovers the Quectel USB slot and resolves the control interface
by USB interface number rather than a persistent `/dev/ttyUSB*` basename. It
uses the existing `at-daemon` object:

- `list` to verify whether the resolved port is open;
- `open` to request the standard serial settings when necessary;
- `sendat` to serialize a command through the existing owner.

Responses are normalized per command before the safety layer consumes them.
The adapter must not read the AT TTY directly, and voice code must not become a
second URC consumer.
