m = Map("qmodem", translate("Modem Configuration"))
m.redirect = luci.dispatcher.build_url("admin", "modem", "qmodem","settings")

s = m:section(NamedSection, arg[1], "modem-device", "")
local slot_name = arg[1]
local pcie_slots = io.popen("ls /sys/bus/pci/devices/")
local pcie_slot_list = {}
for line in pcie_slots:lines() do
    table.insert(pcie_slot_list, line)
end
pcie_slots:close()
local usb_slots = io.popen("ls /sys/bus/usb/devices/")
local usb_slot_list = {}
for line in usb_slots:lines() do
    if not line:match("usb%d+") then
        table.insert(usb_slot_list, line)
    end
end
usb_slots:close()



is_fixed_device = s:option(Flag, "is_fixed_device", translate("Fixed Device"))
is_fixed_device.description = translate("If the device is fixed, it will not be update when the device is connected or disconnected.")
is_fixed_device.default = "0"

path = s:option(ListValue, "slot", translate("Slot Path"))
local usb_match_slot = {}
for i,v in ipairs(usb_slot_list) do
    local uci_name = v:gsub("%.", "_"):gsub(":", "_"):gsub("-", "_")
    if uci_name == slot_name then
        usb_match_slot[uci_name] = v
        path:value("/sys/bus/usb/devices/"..v.."/",v.."[usb]")
    end
end

local pcie_match_slot = {}
for i,v in ipairs(pcie_slot_list) do
    local uci_name = v:gsub("%.", "_"):gsub(":", "_"):gsub("-", "_")
    if uci_name == slot_name then
        pcie_match_slot[uci_name] = v
        path:value("/sys/bus/pci/devices/"..v.."/",v.."[pcie]")
    end
end

data_interface = s:option(ListValue, "data_interface", translate("Interface Type"))
data_interface:value("usb", translate("USB"))
data_interface:value("pcie", translate("PCIe"))

alias = s:option(Value, "alias", translate("Alias"))
alias.description = translate("Alias for the modem, used for identification.")
alias.rmempty = true
alias.default = ""
alias.placeholder = translate("Enter alias name")

name = s:option(Value, "name", translate("Modem Model"))
name.cfgvalue = function(t, n)
    local name = (Value.cfgvalue(t, n) or "-")
    return name
end

define_connect = s:option(Value, "define_connect", translate("PDP Context Index"))
define_connect.default = "1"

manufacturer = s:option(ListValue, "manufacturer", translate("Manufacturer"))
manufacturer:value("quectel", "Quectel")
manufacturer:value("simcom", "Simcom")
manufacturer:value("sierra", "Sierra Wireless")
manufacturer:value("fibocom", "Fibocom")

platform = s:option(Value, "platform", translate("Platform"))
platform:value("lte", "lte")
platform:value("lte12","lte12")
platform:value("qualcomm", "qualcomm")
platform:value("mediatek", "mediatek")
platform:value("unisoc", "unisoc")
platform:value("intel", "intel")

at_port = s:option(Value, "at_port", translate("AT Port"))
at_port.description = translate("AT command port for modem communication.")

modes = s:option(DynamicList, "modes", translate("Supported Modes"))
modes:value("ecm", "ECM")
modes:value("mbim", "MBIM")
modes:value("qmi", "QMI")
modes:value("ncm", "NCM")

enabled = s:option(Flag, "enabled", translate("Enable"))
enabled.default = "1"

wcdma_band = s:option(Value, "wcdma_band", translate("WCDMA Band"))
wcdma_band.description = translate("WCDMA band configuration, e.g., 1/2/3")
wcdma_band.placeholder = translate("Enter WCDMA band")

lte_band = s:option(Value, "lte_band", translate("LTE Band"))
lte_band.description = translate("LTE band configuration, e.g., 1/2/3")
lte_band.placeholder = translate("Enter LTE band")

nsa_band = s:option(Value, "nsa_band", translate("NSA Band"))
nsa_band.description = translate("NSA band configuration, e.g., 1/2/3")
nsa_band.placeholder = translate("Enter NSA band")

sa_band = s:option(Value, "sa_band", translate("SA Band"))
sa_band.description = translate("SA band configuration, e.g., 1/2/3")
sa_band.placeholder = translate("Enter SA band")

f = function(t, n)
    if Value.cfgvalue(t, n) == nil then
        return "null"
    else
        return Value.cfgvalue(t, n)
    end
end

wcdma_band.cfgvalue = f
lte_band.cfgvalue = f
nsa_band.cfgvalue = f
sa_band.cfgvalue = f

return m
