m = Map("modem_sim", translate("SIM Settings"))
s = m:section(TypedSection, "global", translate("SIM Settings"))
s.anonymous = true
s.addremove = false



sim_auto_switch = s:option(Flag, "sim_auto_switch", translate("SIM Auto Switch"))
sim_auto_switch.default = "0"

detect_interval = s:option(Value, "detect_interval", translate("Network Detect Interval"))
detect_interval.default = 15

judge_time = s:option(Value, "judge_time", translate("Network Down Judge Times"))
judge_time.default = 5

ping_dest = s:option(DynamicList, "ping_dest", translate("Ping Destination"))

o = s:option(Value, "wwan_ifname", translate("WWAN Interface"))
o.description = translate("Please enter the WWAN interface name")
o.template = "cbi/network_netlist"
-- o.widget = "optional"
o.nocreate = true

o.default = "cpewan0"

m:section(SimpleSection).template = "modem_hc/modem_sim"

return m
