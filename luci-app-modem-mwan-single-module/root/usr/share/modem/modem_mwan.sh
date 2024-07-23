#! /bin/sh
. /lib/functions.sh
family=$1
config_load modem_mwan
config_get wan  "$family" wan_ifname 
config_get wwan  "$family" wwan_ifname 
config_get track_ip  "$family" track_ip
config_get sticky_mode  global sticky_mode
config_get sticky_timeout global sticky_timeout

append_if(){
    interface=$1
    track_ip=$2
    uci batch <<EOF
set mwan3.$interface=interface
set mwan3.$interface.enabled=1
set mwan3.$interface.family="$family"
set mwan3.$interface.track_method=ping
set mwan3.$interface.reliability='1'
set mwan3.$interface.count='1'
set mwan3.$interface.size='56'
set mwan3.$interface.max_ttl='60'
set mwan3.$interface.timeout='4'
set mwan3.$interface.interval='10'
set mwan3.$interface.failure_interval='5'
set mwan3.$interface.recovery_interval='5'
set mwan3.$interface.down='5'
set mwan3.$interface.up='5'
set mwan3.$interface.add_by=modem
delete mwan3.$interface.track_ip
EOF
    if [ -n "$track_ip" ]; then
        for ip in $track_ip; do
            uci add_list mwan3.$interface.track_ip=$ip
        done
    fi
}




add_mwan3_member()
{
    interface=$1
    metric=$2
    weight=$3
    member_name="${interface}_m_${metric}_w_${weight}"
    uci batch <<EOF
set mwan3.$member_name=member
set mwan3.$member_name.interface=$interface
set mwan3.$member_name.metric=$metric
set mwan3.$member_name.weight=$weight
set mwan3.$member_name.add_by=modem
EOF

}

remove_member()
{
    config_load mwan3
    config_foreach remove_member_cb member
}

remove_member_cb()
{
    local add_by
    config_get add_by $1 add_by
    if [ "$add_by" = "modem" ]; then
        uci delete mwan3.$1
    fi
}

add_mwan3_policy()
{
    policy_name=$1
    use_member=$2
    uci batch <<EOF
set mwan3.$policy_name=policy
set mwan3.$policy_name.last_resort='default'
set mwan3.$policy_name.add_by=modem
delete mwan3.$policy_name.use_member
EOF
    for member in $use_member; do
        uci add_list mwan3.$policy_name.use_member=$member
    done
}


flush_config(){
    config_load mwan3
    config_foreach remove_cb interface
    config_foreach remove_cb member
    config_foreach remove_cb policy
    config_foreach remove_cb rule
}

remove_cb(){
    local add_by
    config_get add_by $1 add_by
    if [ "$add_by" = "modem" ]; then
        uci delete mwan3.$1
    fi
}


add_balance_policy()
{
    
    add_mwan3_member $wan 1 1
    add_mwan3_member $wwan 1 1
    add_mwan3_policy lb_${family} "${wan}_m_1_w_1 ${wwan}_m_1_w_1"
}

add_wan_prefer_policy()
{
    config_load modem_mwan
    config_get wan   "$family" wan_ifname 
    config_get wwan  "$family" wwan_ifname 
    config_get  track_ip "$family" track_ip
    add_mwan3_member $wan 1 1
    add_mwan3_member $wwan 2 1
    add_mwan3_policy preferwan_${family} "${wan}_w_1_m_1 ${wwan}_w_2_m_1"
}


add_wwan_prefer_policy()
{
    add_mwan3_member $wan 2 1
    add_mwan3_member $wwan 1 1
    add_mwan3_policy preferwwan_${family} "${wan}_w_2_m_1 ${wwan}_w_1_m_1"
}

set_if()
{
    family=$1
    append_if $wan "$track_ip"
    append_if $wwan  "$track_ip"
}

gen_if()
{
    append_if $wan "$track_ip"
    append_if $wwan  "$track_ip"
}

gen_rule()
{   
    use_policy=$1
    rule_name=${family}_rule
    uci batch <<EOF
set mwan3.$rule_name=rule
set mwan3.$rule_name.family="$family"
set mwan3.$rule_name.sticky=$sticky_mode
set mwan3.$rule_name.proto='all'
set mwan3.$rule_name.use_policy=$use_policy
set mwan3.$rule_name.add_by=modem
EOF
    if [ -n "$sticky_timeout" ]; then
        uci set mwan3.$rule_name.timeout=$sticky_timeout
    fi
}
/etc/init.d/mwan3 stop
case $2 in
    "lb")
        set_if $family
        add_balance_policy
        gen_rule lb_${family}
        start=start
        ;;
    "wan")
        set_if $family
        add_wan_prefer_policy
        gen_rule preferwan_${family}
        start=start
        ;;
    "wwan")
        set_if $family
        add_wwan_prefer_policy
        gen_rule preferwwan_${family}
        start=start
        ;;
    "disable")
        /etc/init.d/mwan3 stop
        flush_config
        ;;
    "stop")
        rule_name=${family}_rule
        uci delete mwan3.$rule_name
        ;;
esac
uci commit mwan3
/etc/init.d/mwan3 $start
