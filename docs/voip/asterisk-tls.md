# Asterisk TLS endpoint for outbound SIP

This guide configures an Asterisk 20 PJSIP endpoint for one QModem device.
Asterisk owns call routing; QModem remains a single cellular voice endpoint.
Use a public DNS name whose certificate chains to a CA trusted by OpenWrt.

## Network and certificate

Expose TCP 5061 and a bounded UDP RTP range such as 10000-10031. Install the
full certificate chain and private key inside the Asterisk container or host.
Do not use a self-signed certificate for a public deployment.

When acme.sh manages the certificate, keep an HTTP-01 webroot reachable for
renewals and issue the certificate through the configured ACME API:

```nginx
server {
    listen 80;
    server_name pbx.example.net;

    location ^~ /.well-known/acme-challenge/ {
        root /var/www/qmodem-acme;
    }
}
```

```sh
mkdir -p /var/www/qmodem-acme /opt/services/asterisk/certs
acme.sh --issue --server zerossl --ecc -d pbx.example.net \
  --webroot /var/www/qmodem-acme
```

Install the result into the mounted directory instead of pointing Asterisk at
acme.sh's internal files. `--reloadcmd` is persisted in the certificate's
renewal configuration, so every successful renewal activates the new files:

```sh
acme.sh --install-cert -d pbx.example.net --ecc \
  --key-file /opt/services/asterisk/certs/asterisk.key \
  --fullchain-file /opt/services/asterisk/certs/asterisk.crt \
  --reloadcmd 'docker compose -f /opt/services/asterisk/docker-compose.yml restart asterisk'
```

Keep the webroot server block and the acme.sh scheduled renewal job in place.
Test that the challenge URL is publicly reachable before relying on unattended
renewal. Certificate files should be `0644` and the private key `0600`.

Set the RTP range in `rtp.conf`:

```ini
[general]
rtpstart=10000
rtpend=10031
```

## PJSIP configuration

Replace the hostname, account, password, and certificate paths. The password
must use the character set accepted by the QModem UCI validator.

```ini
[transport-tls]
type=transport
protocol=tls
bind=0.0.0.0:5061
cert_file=/etc/asterisk/keys/asterisk.crt
priv_key_file=/etc/asterisk/keys/asterisk.key
method=tlsv1_2

[auth-qmodem-1001]
type=auth
auth_type=userpass
username=qmodem-1001
password=REPLACE_WITH_A_UNIQUE_SECRET

[qmodem-1001]
type=aor
max_contacts=1
remove_existing=yes
qualify_frequency=30

[qmodem-1001]
type=endpoint
transport=transport-tls
context=from-qmodem
disallow=all
allow=alaw
allow=ulaw
auth=auth-qmodem-1001
aors=qmodem-1001
direct_media=no
rewrite_contact=yes
force_rport=yes
rtp_symmetric=yes
```

Do not add an `identify` rule matching `0.0.0.0/0`. REGISTER is associated
with the endpoint by its authentication identity, while `rewrite_contact`
keeps requests on the device-originated NAT/TLS flow.

## Dialplan

An incoming call from the QModem endpoint enters `from-qmodem`. Replace the
sample playback with the distribution policy for the deployment:

```ini
[from-qmodem]
exten => qmodem-1001,1,Answer()
 same => n,Playback(demo-congrats)
 same => n,Hangup()
```

To send a call toward the cellular modem, dial a numeric request URI through
the registered endpoint, for example `PJSIP/10086@qmodem-1001`.
QModem deliberately rejects non-telephone URI users.

## Container deployment

A minimal Compose service can mount configuration and certificates read-only:

```yaml
services:
  asterisk:
    image: andrius/asterisk:20
    restart: unless-stopped
    ports:
      - "5061:5061/tcp"
      - "10000-10031:10000-10031/udp"
    volumes:
      - ./config:/etc/asterisk
      - ./certs:/etc/asterisk/keys:ro
```

After restart, verify the endpoint and its rewritten contact:

```sh
asterisk -rx 'pjsip show endpoint qmodem-1001'
asterisk -rx 'pjsip show contacts'
```

One reachable contact proves TLS registration and NAT signaling only. Complete
acceptance still requires a real call with increasing, non-silent RTP and modem
PCM counters in both directions.
