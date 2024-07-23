local d = require "luci.dispatcher"
local uci = luci.model.uci.cursor()
local sys  = require "luci.sys"

m = Map("modem")
m.title = translate("Dial Overview")
m.description = translate("Check and add modem dialing configurations")

--全局配置
s = m:section(NamedSection, "global", "global", translate("Global Config"))
s.anonymous = true
s.addremove = false

-- 模组扫描
o = s:option(Button, "modem_scan", translate("Modem Scan"))
o.template = "modem/modem_scan"

-- 启用手动配置
o = s:option(Flag, "manual_configuration", translate("Manual Configuration"))
o.rmempty = false
o.description = translate("Enable the manual configuration of modem information").." " translate("(After enable, the automatic scanning and configuration function for modem information will be disabled)")


o = s:option(Flag, "enable_dial", translate("Enable Dial"))
o.rmempty = false
o.description = translate("Enable dial configurations")

o = s:option(Button, "reload_dial", translate("Reload Dial Configurations"))
o.inputstyle = "apply"
o.description = translate("Manually Reload dial configurations When the dial configuration fails to take effect")
o.write = function()
    sys.call("/etc/init.d/modem_network reload")
    luci.http.redirect(d.build_url("admin", "network", "modem", "dial_overview"))
end

s = m:section(TypedSection, "modem-device", translate("Config List"))
s.addremove = ture
s.template = "cbi/tblsection"
s.extedit = d.build_url("admin", "network", "modem", "dial_config", "%s")

o = s:option(Flag, "enable_dial", translate("enable_dial"))
o.width = "5%"
o.rmempty = false

o = s:option(DummyValue, "name", translate("Modem Name"))
o.cfgvalue = function(t, n)
    local name = (Value.cfgvalue(t, n) or "")
    return name:upper()
end

o = s:option(DummyValue, "remarks", translate("Remarks"))

o = s:option(DummyValue, "state", translate("Modem Status"))
o.cfgvalue = function(t, n)
    local name = translate(Value.cfgvalue(t, n) or "")
    return name:upper()
end





o = s:option(DummyValue, "pdp_type", translate("PDP Type"))
o.cfgvalue = function(t, n)
    local pdp_type = (Value.cfgvalue(t, n) or "")
    if pdp_type == "ipv4v6" then
        pdp_type = translate("IPv4/IPv6")
    else
        pdp_type = pdp_type:gsub("_","/"):upper():gsub("V","v")
    end
    return pdp_type
end


o = s:option(DummyValue, "apn", translate("APN"))
o.cfgvalue = function(t, n)
    local apn = (Value.cfgvalue(t, n) or "")
    if apn == "" then
        apn = translate("Auto Choose")
    end
    return apn
end

remove_btn = s:option(Button, "_remove", translate("Remove"))
remove_btn.inputstyle = "remove"
function remove_btn.write(self, section)
    local shell
    shell="/usr/share/modem/modem_scan.sh remove "..section
    luci.sys.call(shell)
    --refresh the page
    luci.http.redirect(d.build_url("admin", "network", "modem", "dial_overview"))
end
-- 添加模块拨号日志
m:append(Template("modem/dial_overview"))


return m
