

local d = require "luci.dispatcher"
local uci = luci.model.uci.cursor()
local sys  = require "luci.sys"
local script_path="/usr/share/modem/"

m = Map("modem_mwan")
m.title = translate("Mwan Config")
m.description = translate("Check and modify the mwan configuration")
s = m:section(NamedSection, "global", "global", translate("gloal Config"))
s.anonymous = true
s.addremove = false
enable_mwan = s:option(Flag, "enable_mwan", translate("Enable MWAN"))
sticky = s:option(Flag,"sticky_mode",translate("sticky mode"))
sticky.default = 0
sticky.description = translate("same source ip address will always use the same wan interface")
sticky_timeout = s:option(Value,"sticky_timeout",translate("sticky timeout"))
sticky_timeout.default = 300
sticky_timeout.datatype = "uinteger"
sticky_timeout:depends("sticky_mode",1)

s = m:section(NamedSection, "ipv4", "ipv4", translate("IPV4 Config"))
s.anonymous = true
s.addremove = false
--设置mwan策略 0:不使用 1:使用(作为后备) 2:使用(作为主要) 3:使用(作为负载均衡)
o = s:option(ListValue, "mwan_policy", translate("MWAN Policy"))
o:value("0", translate("Not Use"))
o:value("1", translate("Use(WWAN As Backup)"))
o:value("2", translate("Use(WWAN As Main)"))
o:value("3", translate("Use(WWAN For Load Balance)"))

--设置mwan 有线wan端口
o = s:option(Value, "wan_ifname", translate("WAN Interface"))
o.rmempty = ture
o.description = translate("Please enter the WAN interface name")
o.template = "cbi/network_netlist"
o.widget = "optional"
o.nocreate = true
o.unspecified = true

--设置mwan wwan端口
o = s:option(Value, "wwan_ifname", translate("WWAN Interface"))
o.rmempty = ture
o.description = translate("Please enter the WWAN interface name")
o.template = "cbi/network_netlist"
o.widget = "optional"
o.nocreate = true
o.unspecified = true
o = s:option(DynamicList, 'track_ip', translate('track_ip'))
o.datatype = 'host'

-- m.title = translate("Mwan Config")
-- m.description = translate("Check and modify the mwan configuration")
-- s = m:section(NamedSection, "ipv6", "ipv6", translate("IPV6 Config"))
-- s.anonymous = true
-- s.addremove = false




-- --设置mwan策略 0:不使用 1:使用(作为后备) 2:使用(作为主要) 3:使用(作为负载均衡)
-- o = s:option(ListValue, "mwan_policy", translate("MWAN Policy"))
-- o:value("0", translate("Not Use"))
-- o:value("1", translate("Use(WWAN As Backup)"))
-- o:value("2", translate("Use(WWAN As Main)"))
-- o:value("3", translate("Use(WWAN For Load Balance)"))

-- --设置mwan 有线wan端口
-- o = s:option(Value, "wan_ifname", translate("WAN Interface"))
-- o.rmempty = ture
-- o.description = translate("Please enter the WAN interface name")
-- o.template = "cbi/network_netlist"
-- o.widget = "optional"
-- o.nocreate = true
-- o.unspecified = true

-- --设置mwan wwan端口
-- o = s:option(Value, "wwan_ifname", translate("WWAN Interface"))
-- o.rmempty = ture
-- o.description = translate("Please enter the WWAN interface name")
-- o.template = "cbi/network_netlist"
-- o.widget = "optional"
-- o.nocreate = true
-- o.unspecified = true

-- o = s:option(DynamicList, 'track_ip', translate('track_ip'))
-- o.datatype = 'host'
return m
