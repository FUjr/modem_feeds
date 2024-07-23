-- Copyright 2024 Siriling <siriling@qq.com>
module("luci.controller.modem", package.seeall)
local http = require "luci.http"
local fs = require "nixio.fs"
local json = require("luci.jsonc")
uci = luci.model.uci.cursor()
local script_path="/usr/share/modem/"
local run_path="/tmp/run/modem/"
local modem_ctrl = "/usr/share/modem/modem_ctrl.sh "

function index()
    if not nixio.fs.access("/etc/config/modem") then
        return
    end

	entry({"admin", "network", "modem"}, alias("admin", "network", "modem", "modem_info"), luci.i18n.translate("Modem"), 100).dependent = true
	--模块信息
	entry({"admin", "network", "modem", "modem_info"}, template("modem/modem_info"), luci.i18n.translate("Modem Information"),2).leaf = true
	entry({"admin", "network", "modem", "get_modem_cfg"}, call("getModemCFG"), nil).leaf = true
	entry({"admin", "network", "modem", "modem_ctrl"}, call("modemCtrl")).leaf = true
	--拨号配置
	entry({"admin", "network", "modem", "dial_overview"},cbi("modem/dial_overview"),luci.i18n.translate("Dial Overview"),3).leaf = true
	entry({"admin", "network", "modem", "dial_config"}, cbi("modem/dial_config")).leaf = true
	entry({"admin", "network", "modem", "modems_dial_overview"}, call("getOverviews"), nil).leaf = true
	--模块调试
	entry({"admin", "network", "modem", "modem_debug"},template("modem/modem_debug"),luci.i18n.translate("Modem Debug"),4).leaf = true
	entry({"admin", "network", "modem", "send_at_command"}, call("sendATCommand"), nil).leaf = true
	entry({"admin", "network", "modem", "modem_scan"}, call("modemScan"), nil).leaf = true
end

--[[
@Description 执行Shell脚本
@Params
	command sh命令
]]
function shell(command)
	local odpall = io.popen(command)
	local odp = odpall:read("*a")
	odpall:close()
	return odp
end

function translate_modem_info(result)
	modem_info = result["modem_info"]
	response = {}
	for k,entry in pairs(modem_info) do
		if type(entry) == "table" then
			key = entry["key"]
			full_name = entry["full_name"]
			if full_name then
				full_name = luci.i18n.translate(full_name)
			elseif key then
				full_name = luci.i18n.translate(key)
			end
			entry["full_name"] = full_name
			if entry["class"] then
				entry["class"] = luci.i18n.translate(entry["class"])
			end
			table.insert(response, entry)
		end
	end
	return response
end

function modemCtrl()
	local action = http.formvalue("action")
	local cfg_id = http.formvalue("cfg")
	local params = http.formvalue("params")
	local translate = http.formvalue("translate")
	if params then
		result = shell(modem_ctrl..action.." "..cfg_id.." ".."\""..params.."\"")
	else 
		result = shell(modem_ctrl..action.." "..cfg_id)
	end
	if translate == "1" then
		modem_more_info = json.parse(result)
		modem_more_info = translate_modem_info(modem_more_info)
		result = json.stringify(modem_more_info)
	end
	luci.http.prepare_content("application/json")
	luci.http.write(result)
end

--[[
@Description 执行AT命令
@Params
	at_port AT串口
	at_command AT命令
]]
function at(at_port,at_command)
	local command="source "..script_path.."modem_util.sh && at "..at_port.." "..at_command
	local result=shell(command)
	result=string.gsub(result, "\r", "")
	return result
end


--[[
@Description 获取模组信息
]]
function getOverviews()
	-- 获取所有模组
	local modems={}
	local logs={}
	uci:foreach("modem", "modem-device", function (modem_device)
		section_name = modem_device[".name"]
		modem_name = modem_device["name"]
		modem_state = modem_device["state"]
		if modem_state == "disabled" then
			return
		end
--模组信息部分
		cmd = modem_ctrl.."base_info "..section_name
		result = shell(cmd)
		json_result = json.parse(result)
		modem_info = json_result["modem_info"]
		tmp_info = {}
		name = {
			type = "plain_text",
			key = "name",
			value = modem_name
		}
		table.insert(tmp_info, name)
		for k,v in pairs(modem_info) do
			full_name = v["full_name"]
			if full_name then
				v["full_name"] = luci.i18n.translate(full_name)
			end
			table.insert(tmp_info, v)
		end
		table.insert(modems, tmp_info)
	--拨号日志部分
	log_path = run_path..section_name.."_dir/dial_log"
	if fs.access(log_path) then
		log_msg = fs.readfile(log_path)
		modem_log = {}
		modem_log["log_msg"] = log_msg
		modem_log["section_name"] = section_name
		modem_log["name"] = modem_name
		table.insert(logs, modem_log)
	end
	end)
	
	-- 设置值
	local data={}
	data["modems"]=modems
	data["logs"]=logs
	luci.http.prepare_content("application/json")
	luci.http.write_json(data)
end

function getModemCFG()

	local cfgs={}
	local translation={}

	uci:foreach("modem", "modem-device", function (modem_device)
		modem_state = modem_device["state"]
		if modem_state == "disabled" then
			return
		end
		--获取模组的备注
		local network=modem_device["network"]
		local remarks=modem_device["remarks"]
		local config_name=modem_device[".name"]
		--设置模组AT串口
		local cfg = modem_device[".name"]
		local at_port=modem_device["at_port"]
		local name=modem_device["name"]:upper()
		local config = {}
		config["name"] = name
		config["at_port"] = at_port
		config["cfg"] = cfg
		table.insert(cfgs, config)
	end)

	-- 设置值
	local data={}
	data["cfgs"]=cfgs
	data["translation"]=translation

	-- 写入Web界面
	luci.http.prepare_content("application/json")
	luci.http.write_json(data)
end



function sendATCommand()
    local at_port = http.formvalue("port")
	local at_command = http.formvalue("command")

	local response={}
    if at_port and at_command then
		response["response"]=at(at_port,at_command)
		response["time"]=os.date("%Y-%m-%d %H:%M:%S")
    end

	-- 写入Web界面
	luci.http.prepare_content("application/json")
	luci.http.write_json(response)
end

--[[
@Description 模组扫描
]]
function modemScan()

	local command=script_path.."modem_scan.sh scan"
	local result=shell(command)
	-- 写入Web界面
	luci.http.prepare_content("application/json")
	luci.http.write_json(result)
end
