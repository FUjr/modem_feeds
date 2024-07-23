module("luci.controller.modem_mwan", package.seeall)

function index()
    if not nixio.fs.access("/etc/config/modem_mwan") then
        return
    end
	--mwan配置
	entry({"admin", "network", "modem", "mwan_config"}, cbi("modem/mwan_config"), luci.i18n.translate("Mwan Config"), 21).leaf = true
end
