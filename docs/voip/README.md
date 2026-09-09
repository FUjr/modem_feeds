# QModem VoIP documentation

This directory collects the engineering documentation for the experimental
`qmodem_voip` application. The application is developed as the independent
`qmodem-voip` OpenWrt feed; these documents describe its integration boundary
with QModem and do not move application ownership into this repository.

The feature is experimental. A modem family name, accepted AT command, SIP
response, or increasing packet counter is not sufficient evidence of supported
voice service. Hardware support is gated by the exact module, firmware, USB
composition, operator profile, and sustained bidirectional media evidence.

## Documents

- [Architecture and design](architecture.md): ownership, call state, control and
  media paths, recovery, and security boundaries.
- [Development guide](development.md): repository layout, development workflow,
  validation, and the acceptance evidence expected for a new modem profile.
- [Interface reference](interfaces.md): the current `qmodem_voip` ubus, event,
  UCI, AT-daemon, browser-media, and RTP contracts.
- [Development notes](development-notes.md): RM520N-GL findings, failed
  hypotheses, lifecycle lessons, and remaining gaps.
- [Outbound SIP design](outbound-sip-design.md): topology, security,
  idempotent lifecycle, and phased delivery plan.
- [Outbound SIP usage](outbound-sip.md): UCI configuration and verification
  procedure for the outbound mode.
- [Asterisk TLS endpoint](asterisk-tls.md): public TLS endpoint, NAT, routing,
  and container configuration.

## Scope

The experiment provides one cellular call slot with browser and LAN SIP control
surfaces. It does not provide a PBX, external SIP service, multi-line routing,
customer-specific branding, or a second owner for the modem AT port.

The implementation evolves in its owning repository. Before changing code,
compare these documents with the current `qmodem-voip` source, tests, and
capability registry. The interface reference records the implementation
inspected on 2026-09-06 and calls out internal-only methods separately.
