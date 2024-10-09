#!/bin/sh
sim_gpio="/sys/class/gpio/sim/value"
modem_gpio="/sys/class/gpio/4g/value"
ping_dest=$(uci -q get modem_sim.@global[0].ping_dest)
wwan_ifname=$(uci -q get modem_sim.@global[0].wwan_ifname)
[ -n $wwan_ifname ] && modem_config=$wwan_ifname
is_empty=$(uci -q get modem.$modem_config)
[ -z $is_empty ] && unset modem_config
[ -z "$modem_config" ] && get_first_avalible_config
netdev=$(ifstatus $wwan_ifname | jq -r .device)
judge_time=$(uci -q get modem_sim.@global[0].judge_time)
detect_interval=$(uci -q get modem_sim.@global[0].detect_interval)
[ -z $detect_interval ] && detect_interval=10
[ -z $judge_time ] && judge_time=5

set_modem_config()
{
    cfg=$1
    [ -n "$modem_config" ] && return
    config_get state $1 state
    [ -n "$state" ] && [ "$state" != "disabled" ] && modem_config=$cfg
}

get_first_avalible_config()
{
    . /lib/functions.sh
    config_load modem
    config_foreach set_modem_config modem-device
}

sendat()
{
    tty=$1
    cmd=$2
    sms_tool -d $tty at $cmd 2>&1
}

reboot_modem() {
    echo 0 > $modem_gpio
    sleep 1
    echo 1 > $modem_gpio
}

switch_sim() {
    if [ -f $sim_gpio ]; then
        sim_status=$(cat $sim_gpio)
        if [ $sim_status -eq 0 ]; then
            echo 1 > $sim_gpio
        else
            echo 0 > $sim_gpio
        fi
        reboot_modem
        logger -t modem_sim "switch sim from $sim_status to $(cat $sim_gpio)"
    fi
}

ping_monitor() {
    #ping_dest为空则不进行ping检测 ，如果有多个，用空格隔开
    has_success=0
    for dest in $ping_dest; do
        ping -c 1 -W 1 $dest -I $netdev > /dev/null
        if [ $? -eq 0 ]; then
            return 1
        fi
    done
    return 0
}

at_dial_monitor() {
    ttydev=$1
    define_connect=$2
    #检查拨号状况,有v4或v6地址则返回1
    at_cmd="AT+CGPADDR=1"
    [ "$define_connect" == "3" ] && at_cmd="AT+CGPADDR=3"
    expect="+CGPADDR:"
    result=$(sendat $ttydev $at_cmd | grep "$expect")
    if [ -n "$result" ];then
            ipv6=$(echo $result | grep -oE "\b([0-9a-fA-F]{0,4}:){2,7}[0-9a-fA-F]{0,4}\b")
            ipv4=$(echo $result | grep -oE "\b([0-9]{1,3}\.){3}[0-9]{1,3}\b")
            disallow_ipv4="0.0.0.0"
            #remove the disallow ip
            if [ "$ipv4" == "$disallow_ipv4" ];then
                ipv4=""
            fi
            if [ -n "$ipv4" ] || [ -n "$ipv6" ];then
                return 1
            fi
    fi
    return 0
}

at_sim_monitor() {
    ttydev=$1
    #检查sim卡状态，有sim卡则返回1
    expect="+CPIN: READY"
    result=$(sendat $ttydev "AT+CPIN?" | grep -o "$expect")
    if [ -n "$result" ]; then
        return 1
    fi
    return 0
}

precheck()
{
    modem_config=$1
    config=$(uci -q show modem.$modem_config)
    [ -z "$config" ] && return 1
    ttydev=$(uci -q get modem.$modem_config.at_port)
    enable_dial=$(uci -q get modem.$modem_config.enable_dial)
    global_en=$(uci -q get modem.global.enable_dial)
    [ "$global_en" == "0" ] && return 1
    [ -z "$enable_dial" ] || [ "$enable_dial" == "0" ] && return 1
    [ -z "$ttydev" ] && return 1
    [ ! -e "$ttydev" ] && return 1
    return 0


}

fail_times=0
main_monitor() {
    
    while true; do
        #检测前置条件：1.tty存在 2.配置信息存在 3.拨号功能已开启
        precheck $modem_config
        if [ $? -eq 1 ]; then
            sleep $detect_interval
            continue
        fi
        #检查ping状态，超过judge_time则切卡
        if [ -n "$ping_dest" ]; then
            ping_monitor
            ping_result=$?
        fi

        [ -z $ttydev ] && ttydev=$(uci -q get modem.$modem_config.at_port)
        [ -z $define_connect ] && define_connect=$(uci -q get modem.$modem_config.define_connect)
        if [ -n $define_connect ] && [ -n $ttydev ];then
            at_dial_monitor $ttydev $define_connect
            dial_result=$?
        fi
        if [ -n $ttydev ];then
            at_sim_monitor $ttydev;
            sim_result=$?
        fi
        

        if [ -n "$ping_dest" ];then
            #策略：ping成功则重置fail_times，否则fail_times累加
            [ -z "$dial_result" ] && dial_result=1
            [ -z "$sim_result" ] && sim_result=1
            fail_total=$((3 - $ping_result - $dial_result - $sim_result))
            if [ $ping_result -eq 1 ]; then
                fail_times=0
            else
                fail_times=$(($fail_times + $fail_total))
            fi
            
            #如果失败次数超过judge_time * 3则切卡 切卡后等待3分钟
        else
            #策略 无ping则检测拨号和sim卡状态，拨号成功则重置fail_times，否则fail_times累加
            [ -z "$dial_result" ] && dial_result=1
            [ -z "$sim_result" ] && sim_result=1
            fail_total=$((2 - $dial_result - $sim_result))
            if [ $dial_result -eq 1 ]; then
                fail_times=0
            else
                fail_times=$(($fail_times + $fail_total))
            fi
        fi
        logger -t modem_sim "ping_result:$ping_result dial_result:$dial_result sim_result:$sim_result fail_times:$fail_times fail_total:$fail_total fail_times:$fail_times"
        if [ $fail_times -ge $(($judge_time * 2)) ]; then
            switch_sim
            fail_times=0
            sleep 240
        fi
        sleep $detect_interval
    done
}

sleep 180

main_monitor
