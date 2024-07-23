m = Map("modem_ttl", translate("TTL Config"))
s = m:section(NamedSection, "global", "global", translate("Global Config"))

enable = s:option(Flag, "enable", translate("Enable"))
enable.default = "0"

ttl = s:option(Value, "ttl", translate("TTL"))
ttl.default = 64
ttl.datatype = "uinteger"

o = s:option(Value, "ifname", translate("Interface"))
o.rmempty = ture
o.template = "cbi/network_netlist"
o.widget = "optional"
o.nocreate = true
o.unspecified = true

return m
