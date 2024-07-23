-- Copyright 2024 Siriling <siriling@qq.com>
module("luci.controller.modem_ttl", package.seeall)
function index()
    if not nixio.fs.access("/etc/config/modem_ttl") then
        return
    end
	entry({"admin", "network", "modem", "modem_ttl"}, cbi("modem/modem_ttl"), luci.i18n.translate("TTL Config"), 22).leaf = true
end
