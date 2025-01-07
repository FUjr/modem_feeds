#!/bin/sh
source /usr/share/libubox/jshn.sh
method=$1
config_section=$2
at_port=$(uci get qmodem.$config_section.at_port)
sms_at_port=$(uci get qmodem.$config_section.sms_at_port)
vendor=$(uci get qmodem.$config_section.manufacturer)
platform=$(uci get qmodem.$config_section.platform)
define_connect=$(uci get qmodem.$config_section.define_connect)
modem_path=$(uci get qmodem.$config_section.path)
modem_slot=$(basename $modem_path)
[ -z "$define_connect" ] && {
    define_connect="1"
}

case $vendor in
    "quectel")
        . /usr/share/qmodem/vendor/quectel.sh
        ;;
    "fibocom")
        . /usr/share/qmodem/vendor/fibocom.sh
        ;;
    "sierra")
        . /usr/share/qmodem/vendor/sierra.sh
        ;;
    *)
        . /usr/share/qmodem/generic.sh
        ;;
esac

try_cache() {
    cache_timeout=$1
    cache_file=$2
    function_name=$3
    current_time=$(date +%s)
    file_time=$(stat -t $cache_file | awk '{print $14}')
    [ -z "$file_time" ] && file_time=0
    if [ ! -f $cache_file ] || [ $(($current_time - $file_time)) -gt $cache_timeout ]; then
        touch $cache_file
        json_add_array modem_info
        $function_name
        json_close_array
        json_dump > $cache_file
        return 1
    else
        cat $cache_file
        exit 0
    fi
}

get_sms(){
    [ -n "$sms_at_port" ] && at_port=$sms_at_port
    cache_timeout=$1
    cache_file=$2
    current_time=$(date +%s)
    file_time=$(stat -t $cache_file | awk '{print $14}')
    [ -z "$file_time" ] && file_time=0
    if [ ! -f $cache_file ] || [ $(($current_time - $file_time)) -gt $cache_timeout ]; then
        touch $cache_file
        #sms_tool_q -d $at_port -j recv > $cache_file
        tom_modem -d $at_port -o r > $cache_file
        cat $cache_file
    else
        cat $cache_file
    fi
}

get_at_cfg(){
    json_add_object at_cfg
    duns=$(ls /dev/mhi_DUN*)
    ttys=$(ls /dev/ttyUSB*)
    ttyacms=$(ls /dev/ttyACM*)
    all_ttys="$duns $ttys $ttyacms"
    json_add_array other_ttys
    for tty in $all_ttys; do
        [ -n "$tty" ] && json_add_string "" "$tty"
    done
    json_close_array
    json_add_array ports
    ports=$(uci get qmodem.$config_section.ports)
    for port in $ports; do
        json_add_string "" "$port"
    done
    json_close_array
    json_add_array valid_ports
    v_ports=$(uci get qmodem.$config_section.valid_at_ports)
    for port in $v_ports; do
        json_add_string "" "$port"
    done
    json_close_array
    json_add_string using_port $(uci get qmodem.$config_section.at_port)
    json_add_array cmds
    general_cmd=$(jq -rc '.general[]|to_entries| .[] | @sh "key=\(.key) value=\(.value)"' /usr/share/qmodem/at_commands.json)
    platform_cmd=$(jq -rc ".${vendor}.${platform}[]|to_entries| .[] | @sh \"key=\(.key) value=\(.value)\"" /usr/share/qmodem/at_commands.json)
    [ -z "$platform_cmd" ] && platform_cmd=$(jq -rc ".$vendor.general[]|to_entries| .[] | @sh \"key=\(.key) value=\(.value)\"" /usr/share/qmodem/at_commands.json)
    cmds=$(echo -e "$general_cmd\n$platform_cmd")
    IFS=$'\n'
    for cmd in $cmds; do
        json_add_object cmd
        eval $cmd
        json_add_string "name" "$key"
        json_add_string "value" "$value"
        json_close_object
    done
    json_close_array
    json_close_object
    json_dump
    unset IFS
}

#会初始化一个json对象 命令执行结果会保存在json对象中
json_init
json_add_object result
json_close_object
case $method in
    "get_at_cfg")
        get_at_cfg
        exit
        ;;

    "clear_dial_log")
        json_select result
        log_file="/var/run/qmodem/${config_section}_dir/dial_log"
        [ -f $log_file ] && echo "" > $log_file && json_add_string status "1" || json_add_string status "0"
        json_close_object
        ;;
    "get_dns")
        get_dns
        ;;
    "get_imei")
        get_imei
        ;;
    "set_imei")
        set_imei $3
        ;;
    "get_mode")
        get_mode
        ;;
    "set_mode")
        set_mode $3
        ;;
    "get_network_prefer")
        get_network_prefer
        ;;
    "set_network_prefer")
        set_network_prefer $3
        ;;
    "get_lockband")
        get_lockband
        ;;
    "set_lockband")
        set_lockband $3
        ;;
    "get_neighborcell")
        get_neighborcell
        ;;
    "send_at")
        cmd=$(echo "$3" | jq -r '.at')
        port=$(echo "$3" | jq -r '.port')
        res=$(at $port $cmd)
        json_add_object at_cfg
        if [ "$?" == 0 ]; then
            json_add_string status "1"
            json_add_string cmd "at $port $cmd"
            json_add_string "res" "$res"
        else
            json_add_string status "0"
        fi
        ;;
    "set_neighborcell")
        set_neighborcell $3
        ;;
    "base_info")
        cache_file="/tmp/cache_$1_$2"
        try_cache 10 $cache_file base_info
        ;;
    "sim_info")
        cache_file="/tmp/cache_$1_$2"
        try_cache 10 $cache_file sim_info
        ;;
    "cell_info")
        cache_file="/tmp/cache_$1_$2"
        try_cache 10 $cache_file cell_info
        ;;
    "network_info")
        cache_file="/tmp/cache_$1_$2"
        try_cache 10 $cache_file network_info
        ;;
    "info")
        cache_file="/tmp/cache_$1_$2"
        try_cache 10 $cache_file get_info
        ;;
    "get_sms")
        get_sms 10 /tmp/cache_sms_$2
        exit
        ;;
    "get_reboot_caps")
        get_reboot_caps
        exit
        ;;
    "do_reboot")
        reboot_method=$(echo $3 |jq -r '.method')
        echo $3 > /tmp/555/reboot
        case $reboot_method in
            "hard")
                hard_reboot
                ;;
            "soft")
                soft_reboot
                ;;
        esac
        ;;
    "send_sms")
        cmd_json=$3
        phone_number=$(echo $cmd_json | jq -r '.phone_number')
        message_content=$(echo $cmd_json | jq -r '.message_content')
        [ -n "$sms_at_port" ] && at_port=$sms_at_port
        sms_tool_q -d $at_port send "$phone_number" "$message_content" > /dev/null
        json_select result
        if [ "$?" == 0 ]; then
            json_add_string status "1"
            json_add_string cmd "sms_tool_q -d $at_port send \"$phone_number\" \"$message_content\""
            json_add_string "cmd_json" "$cmd_json"
        else
            json_add_string status "0"
        fi
        json_close_object
        ;;
    "send_raw_pdu")
        cmd=$3
        [ -n "$sms_at_port" ] && at_port=$sms_at_port
        #res=$(sms_tool_q -d $at_port send_raw_pdu "$cmd" )
        res=$(tom_modem -d $at_port -o s -p "$cmd")
        json_select result
        if [ "$?" == 0 ]; then
            json_add_string status "1"
            json_add_string cmd "tom_modem -d $at_port -o s -p \"$cmd\""
            json_add_string "res" "$res"
        else
            json_add_string status "0"
        fi
        ;;
    "delete_sms")
        json_select result
        index=$3
        [ -n "$sms_at_port" ] && at_port=$sms_at_port
        for i in $index; do
            # sms_tool_q -d $at_port delete $i > /dev/null
            tom_modem -d $at_port -o d -i $i
            touch /tmp/cache_sms_$2
            if [ "$?" == 0 ]; then
                json_add_string status "1"
                json_add_string "index$i" "tom_modem -d $at_port -o d -i $i"
            else
                json_add_string status "0"
            fi
        done
        json_close_object
        rm -rf /tmp/cache_sms_$2
        ;;
    "get_disabled_features")
        json_add_array disabled_features
        #从vendor文件中读取对vendor禁用的功能
        vendor_get_disabled_features
        get_modem_disabled_features
        get_global_disabled_features
        json_close_array
        ;;
esac
json_dump
