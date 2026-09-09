# Outbound SIP usage

This document describes the outbound SIP configuration and verification
contract. Outbound signaling is TLS-only and server certificates are verified
against the system CA bundle.

## Prepare the interface

Set `interface` to the local OpenWrt network interface that owns the source
address. If the cable is on physical `lan4` and that port is assigned to the
WAN network, use `wan`, not `lan4` and not the Asterisk IP:

```sh
uci set qmodem_voip.sip.interface='wan'
```

## Configure an Asterisk account

Replace the example values with the account created on Asterisk. Do not put
these values in a support bundle or public repository.

```sh
uci set qmodem_voip.sip.mode='outbound'
uci set qmodem_voip.sip.outbound_server='pbx.example.net'
uci set qmodem_voip.sip.outbound_port='5061'
uci set qmodem_voip.sip.outbound_transport='tls'
uci set qmodem_voip.sip.outbound_username='device-1001'
uci set qmodem_voip.sip.outbound_password='REPLACE_ME'
uci set qmodem_voip.sip.outbound_realm=''
uci set qmodem_voip.sip.outbound_register_interval='300'
uci commit qmodem_voip
chmod 600 /etc/config/qmodem_voip
/etc/init.d/qmodem_voip reload
```

## Verification checklist

Verify in this order:

1. `logread -e qmodem_voip` reports registration success and a refresh time.
2. Asterisk shows the endpoint as reachable and sends an inbound test call.
3. The call status exposes a Call-ID and both RTP packet counters increase.
4. A restart produces one registration and one media process, with no stale
   transaction after the old process exits.
