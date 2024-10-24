#!/bin/sh
SCRIPT_DIR="/usr/share/qmodem"
source /usr/share/libubox/jshn.sh
source "${SCRIPT_DIR}/modem_util.sh"
add_plain_info_entry()
{
    key=$1
    value=$2
    key_full_name=$3
    class_overwrite=$4
    if [ -n "$class_overwrite" ]; then
        class="$class_overwrite"
    fi
    json_add_object ""
    json_add_string  key "$key"
    json_add_string  value "$value"
    json_add_string "full_name" "$key_full_name"
    json_add_string "type" "plain_text"
    if [ -n "$class" ]; then
        json_add_string "class" "$class"
        json_add_string "class_origin" "$class"
    fi
    json_close_object
}

add_warning_message_entry()
{
    key=$1
    value=$2
    key_full_name=$3
    class_overwrite=$4
    if [ -n "$class_overwrite" ]; then
        class="$class_overwrite"
    fi
    json_add_object ""
    json_add_string  key "$key"
    json_add_string  value "$value"
    json_add_string "full_name" "$key_full_name"
    json_add_string "type" "warning_message"
    json_add_string "class" "warning"
    json_add_string "class_origin" "warning"
    json_close_object
}

add_bar_info_entry()
{
    key=$1
    value=$2
    key_full_name=$3
    min_value=$4
    max_value=$5
    unit=$6
    class_overwrite=$7
    if [ -n "$class_overwrite" ]; then
        class="$class_overwrite"
    fi
    json_add_object ""
    json_add_string  key "$key"
    json_add_string  value "$value"
    json_add_string  min_value "$min_value"
    json_add_string  max_value "$max_value"
    json_add_string "full_name" "$key_full_name"
    json_add_string "unit" "$unit"
    json_add_string "type" "progress_bar"
    if [ -n "$class" ]; then
        json_add_string "class" "$class"
        json_add_string "class_origin" "$class"
    fi
    json_close_object
}

add_avalible_band_entry()
{
    band_id=$1
    band_name=$2
    json_add_object ""
    json_add_string  band_id "$band_id"
    json_add_string  band_name "$band_name"
    json_add_string "type" "avalible_band"
    json_close_object
}


get_dns()
{
    [ -z "$define_connect" ] && {
        define_connect="1"
    }

    local public_dns1_ipv4="223.5.5.5"
    local public_dns2_ipv4="119.29.29.29"
    local public_dns1_ipv6="2400:3200::1" #下一代互联网北京研究中心：240C::6666，阿里：2400:3200::1，腾讯：2402:4e00::
    local public_dns2_ipv6="2402:4e00::"

    #获取DNS地址
    at_command="AT+GTDNS=${define_connect}"
    local response=$(at ${at_port} ${at_command} | grep "+GTDNS: ")

    local ipv4_dns1=$(echo "${response}" | awk -F'"' '{print $2}' | awk -F',' '{print $1}')
    [ -z "$ipv4_dns1" ] && {
        ipv4_dns1="${public_dns1_ipv4}"
    }

    local ipv4_dns2=$(echo "${response}" | awk -F'"' '{print $4}' | awk -F',' '{print $1}')
    [ -z "$ipv4_dns2" ] && {
        ipv4_dns2="${public_dns2_ipv4}"
    }

    local ipv6_dns1=$(echo "${response}" | awk -F'"' '{print $2}' | awk -F',' '{print $2}')
    [ -z "$ipv6_dns1" ] && {
        ipv6_dns1="${public_dns1_ipv6}"
    }

    local ipv6_dns2=$(echo "${response}" | awk -F'"' '{print $4}' | awk -F',' '{print $2}')
    [ -z "$ipv6_dns2" ] && {
        ipv6_dns2="${public_dns2_ipv6}"
    }
    json_add_object "dns"
    json_add_string "ipv4_dns1" "$ipv4_dns1"
    json_add_string "ipv4_dns2" "$ipv4_dns2"
    json_add_string "ipv6_dns1" "$ipv6_dns1"
    json_add_string "ipv6_dns2" "$ipv6_dns2"
    json_close_object
}

get_sim_status()
{
    local sim_status
    case $1 in
        "") 
            sim_status="miss"
            sim_state_code=0
            ;;
        *"ERROR"*) 
            sim_status="miss"
            sim_state_code=0
            ;;
        *"READY"*) 
            sim_status="ready" 
            sim_state_code=1
            ;;
        *"SIM PIN"*) 
            sim_status="MT is waiting SIM PIN to be given"
            sim_state_code=2
             ;;
        *"SIM PUK"*) 
            sim_status="MT is waiting SIM PUK to be given"
            sim_state_code=3
            ;;
        *"PH-FSIM PIN"*)
            sim_status="MT is waiting phone-to-SIM card password to be given"
            sim_state_code=4
            ;;
        *"PH-FSIM PIN"*) 
            sim_status="MT is waiting phone-to-very first SIM card password to be given"
            sim_state_code=5
            ;;
        *"PH-FSIM PUK"*) 
            sim_status="MT is waiting phone-to-very first SIM card unblocking password to be given"
            sim_state_code=6
            ;;
        *"SIM PIN2"*) 
            sim_status="MT is waiting SIM PIN2 to be given"
            sim_state_code=7
            ;;
        *"SIM PUK2"*) 
            sim_status="MT is waiting SIM PUK2 to be given" 
            sim_state_code=8
            ;;
        *"PH-NET PIN"*) 
            sim_status="MT is waiting network personalization password to be given" 
            sim_state_code=9
            ;;
        *"PH-NET PUK"*) 
            sim_status="MT is waiting network personalization unblocking password to be given" 
            sim_state_code=10
            ;;
        *"PH-NETSUB PIN"*) 
            sim_status="MT is waiting network subset personalization password to be given" 
            sim_state_code=11
            ;;
        *"PH-NETSUB PUK"*) 
            sim_status="MT is waiting network subset personalization unblocking password to be given" 
            sim_state_code=12
            ;;
        *"PH-SP PIN"*) 
            sim_status="MT is waiting service provider personalization password to be given" 
            sim_state_code=13
            ;;
        *"PH-SP PUK"*)
            sim_status="MT is waiting service provider personalization unblocking password to be given"
            sim_state_code=14
            ;;
        *"PH-CORP PIN"*) 
            sim_status="MT is waiting corporate personalization password to be given" 
            sim_state_code=16
            ;;

        *"PH-CORP PUK"*) 
            sim_status="MT is waiting corporate personalization unblocking password to be given" 
            sim_state_code=17
            ;;
        *) 
            sim_status="unknown" 
            sim_state_code=99
            ;;
    esac
    echo "$sim_status"
}

#获取信号强度指示
# $1:信号强度指示数字
get_rssi()
{
    local rssi
    case $1 in
		"99") rssi="unknown" ;;
		* )  rssi=$((2 * $1 - 113)) ;;
	esac
    echo "$rssi"
}

#获取网络类型
# $1:网络类型数字
get_rat()
{
    local rat
    case $1 in
		"0"|"1"|"3"|"8") rat="GSM" ;;
		"2"|"4"|"5"|"6"|"9"|"10") rat="WCDMA" ;;
        "7") rat="LTE" ;;
        "11"|"12") rat="NR" ;;
	esac
    echo "${rat}"
}

#获取连接状态
#return raw data
get_connect_status()
{
    at_cmd="AT+CGPADDR=1"
    [ "$define_connect" == "3" ] && at_cmd="AT+CGPADDR=3"
    expect="+CGPADDR:"
    result=$(at  $at_port $at_cmd | grep $expect)
    if [ -n "$result" ];then
            ipv6=$(echo $result | grep -oE "\b([0-9a-fA-F]{0,4}:){2,7}[0-9a-fA-F]{0,4}\b")
            ipv4=$(echo $result | grep -oE "\b([0-9]{1,3}\.){3}[0-9]{1,3}\b")
            disallow_ipv4="0.0.0.0"
            #remove the disallow ip
            if [ "$ipv4" == "$disallow_ipv4" ];then
                ipv4=""
            fi
    fi
    if [ -n "$ipv4" ] || [ -n "$ipv6" ];then
        connect_status="Yes"
    else
        connect_status="No"
    fi
    add_plain_info_entry "connect_status" "$connect_status" "Connect Status"
}

#获取移远模组信息
# $1:AT串口
# $2:平台
# $3:连接定义
get_info()
{
    #基本信息
    base_info

	#SIM卡信息
    sim_info
    if [ "$sim_status" != "ready" ]; then
        add_warning_message_entry "sim_status" "$sim_status" "SIM Error,Error code:" "warning"
        return
    fi

    #网络信息
    network_info
    if [ "$connect_status" != "Yes" ]; then
        return
    fi

    #小区信息
    cell_info

    return

}
