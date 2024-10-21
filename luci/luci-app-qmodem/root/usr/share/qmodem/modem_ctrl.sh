#!/bin/sh
source /usr/share/libubox/jshn.sh
method=$1
config_section=$2
at_port=$(uci get qmodem.$config_section.at_port)
sms_at_port=$(uci get qmodem.$config_section.sms_at_port)
vendor=$(uci get qmodem.$config_section.manufacturer)
platform=$(uci get qmodem.$config_section.platform)
define_connect=$(uci get qmodem.$config_section.define_connect)
[ -z "$define_connect" ] && {
    define_connect="1"
}

case $vendor in
    "quectel")
        . /usr/share/qmodem/quectel.sh
        ;;
    "fibocom")
        . /usr/share/qmodem/fibocom.sh
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


#会初始化一个json对象 命令执行结果会保存在json对象中
json_init
json_add_object result
json_close_object
case $method in
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
esac
json_dump
