local modem_cfg = require "luci.model.cbi.qmodem.modem_cfg"

-- Helper function to load slot paths
local function load_slots(path, exclude_pattern)
    local slots = {}
    local handle = io.popen("ls " .. path)
    for line in handle:lines() do
        if not exclude_pattern or not line:match(exclude_pattern) then
            table.insert(slots, line)
        end
    end
    handle:close()
    return slots
end

-- Helper function to populate options dynamically from a table
local function populate_options(option, values)
    for key, value in pairs(values) do
        option:value(key, value)
    end
end

-- Map and Section setup
m = Map("qmodem", translate("Modem Configuration"))
m.redirect = luci.dispatcher.build_url("admin", "modem", "qmodem", "settings")

s = m:section(NamedSection, arg[1], "modem-device", "")
local slot_name = arg[1]

-- Load slot paths
local usb_slot_list = load_slots("/sys/bus/usb/devices/", "usb%d+")
local pcie_slot_list = load_slots("/sys/bus/pci/devices/")

-- Fixed Device Flag
is_fixed_device = s:option(Flag, "is_fixed_device", translate("Fixed Device"))
is_fixed_device.description = translate("If the device is fixed, it will not update when the device is connected or disconnected.")
is_fixed_device.default = "0"

-- Slot Path
path = s:option(ListValue, "slot", translate("Slot Path"))
for _, v in ipairs(usb_slot_list) do
    local uci_name = v:gsub("[%.:%-]", "_")
    if uci_name == slot_name then
        path:value("/sys/bus/usb/devices/" .. v .. "/", v .. "[usb]")
    end
end
for _, v in ipairs(pcie_slot_list) do
    local uci_name = v:gsub("[%.:%-]", "_")
    if uci_name == slot_name then
        path:value("/sys/bus/pci/devices/" .. v .. "/", v .. "[pcie]")
    end
end

-- Interface Type
data_interface = s:option(ListValue, "data_interface", translate("Interface Type"))
data_interface:value("usb", translate("USB"))
data_interface:value("pcie", translate("PCIe"))

-- Alias
alias = s:option(Value, "alias", translate("Alias"))
alias.description = translate("Alias for the modem, used for identification.")
alias.rmempty = true
alias.default = ""
alias.placeholder = translate("Enter alias name")

-- Modem Model
name = s:option(Value, "name", translate("Modem Model"))
name.cfgvalue = function(t, n)
    return Value.cfgvalue(t, n) or "-"
end

-- PDP Context Index
define_connect = s:option(Value, "define_connect", translate("PDP Context Index"))
define_connect.default = "1"

-- Manufacturer (Loaded from modem_cfg.lua)
manufacturer = s:option(ListValue, "manufacturer", translate("Manufacturer"))
populate_options(manufacturer, modem_cfg.manufacturers)

-- Platform (Loaded from modem_cfg.lua)
platform = s:option(ListValue, "platform", translate("Platform"))
populate_options(platform, modem_cfg.platforms)

-- AT Port
at_port = s:option(Value, "at_port", translate("AT Port"))
at_port.description = translate("AT command port for modem communication.")

-- Supported Modes (Loaded from modem_cfg.lua)
modes = s:option(DynamicList, "modes", translate("Supported Modes"))
populate_options(modes, modem_cfg.modes)

-- Enable Flag
enabled = s:option(Flag, "enabled", translate("Enable"))
enabled.default = "1"

disabled_features = s:option(DynamicList, "disabled_features", translate("Disabled Features"))
disabled_features.description = translate("Select features to disable for this modem.")
populate_options(disabled_features, modem_cfg.disabled_features)

-- Band Configurations
local band_options = {
    { name = "wcdma_band", label = "WCDMA Band", placeholder = "Enter WCDMA band" },
    { name = "lte_band", label = "LTE Band", placeholder = "Enter LTE band" },
    { name = "nsa_band", label = "NSA Band", placeholder = "Enter NSA band" },
    { name = "sa_band", label = "SA Band", placeholder = "Enter SA band" },
}

for _, band in ipairs(band_options) do
    local option = s:option(Value, band.name, translate(band.label))
    option.description = translate(band.label .. " configuration, e.g., 1/2/3")
    option.placeholder = translate(band.placeholder)
    option.cfgvalue = function(t, n)
        return Value.cfgvalue(t, n) or "null"
    end
end

return m
