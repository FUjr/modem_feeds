# QModem LuCI browser tests

The container runs Chromium headlessly against a reachable OpenWrt device. It
does not embed device credentials in the image or repository.

```sh
cd tests/browser
QMODEM_BASE_URL=https://10.96.210.191 \
QMODEM_USERNAME=admin \
QMODEM_PASSWORD=... \
docker compose run --rm luci-browser-tests
```

The suite covers desktop and mobile Chromium viewports. Screenshots, traces,
and the JSON report are written to `test-results/`, which is ignored by Git.
