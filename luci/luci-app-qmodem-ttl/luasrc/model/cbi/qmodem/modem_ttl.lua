local uci = luci.model.uci.cursor()
m = Map("qmodem_ttl", translate("TTL Config"))
s = m:section(NamedSection, "global", "global", translate("Global Config"))

enable = s:option(Flag, "enable", translate("Enable"))
enable.default = "0"

ttl = s:option(Value, "ttl", translate("TTL"))
ttl.default = 64
ttl.datatype = "uinteger"

o = s:option(Value, "ifname", translate("Interface"))
uci:foreach("network", "interface", function(s)
    if s[".name"] ~= "loopback" and s[".name"] ~= "lan" then
        o:value(s[".name"])
    end
end)

return m
