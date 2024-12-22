#/bin/sh

action=$1
config=$2
slot_type=$3
modem_support=$(cat /usr/share/qmodem/modem_support.json)
debug_subject="modem_scan"
source /lib/functions.sh
source /usr/share/qmodem/modem_util.sh

get_associate_usb()
{
    target_slot=$1
    config_load qmodem
    config_foreach _get_associated_usb_by_path modem-slot
}

get_default_alias()
{
    target_slot=$1
    config_load qmodem
    config_foreach _get_default_alias_by_slot
}

get_default_metric()
{
    target_slot=$1
    config_load qmodem
    config_foreach _get_default_metric_by_slot
}

_get_associated_usb_by_path()
{
    local cfg="$1"
    echo $target_slot
    config_get _get_slot $cfg slot
    if [ "$target_slot" == "$_get_slot" ];then
        config_get associated_usb $cfg associated_usb
        echo \[$target_slot\]associated_usb:$associated_usb
    fi
    
}

_get_default_alias_by_slot()
{
    local cfg="$1"
    config_get _get_slot $cfg slot
    if [ "$target_slot" == "$_get_slot" ];then
        config_get default_alias $cfg  alias
    fi

}

_get_default_metric_by_slot()
{
    local cfg="$1"
    config_get _get_slot $cfg slot
    if [ "$target_slot" == "$_get_slot" ];then
        config_get default_metric $cfg  default_metric
    fi

}

scan()
{
    local slot_type=$1
    if [ "$slot_type" == "usb" ] || [ -z "$slot_type" ];then
        scan_usb
        usb_slots=$(echo $usb_slots | uniq )
        for slot in $usb_slots; do
            slot_type="usb"
            add $slot
        done
    fi
    if [ "$slot_type" == "pcie" ] || [ -z "$slot_type" ];then
        scan_pcie
        pcie_slots=$(echo $pcie_slots | uniq )
        for slot in $pcie_slots; do
            slot_type="pcie"
            add $slot
        done
    fi
}

scan_usb()
{
    usb_net_device_prefixs="usb eth wwan"
    usb_slots=""
    for usb_net_device_prefix in $usb_net_device_prefixs; do
        usb_netdev=$(ls /sys/class/net | grep -E "${usb_net_device_prefix}")
        for netdev in $usb_netdev; do
            netdev_path=$(readlink -f "/sys/class/net/$netdev/device/")
            [ -z "$netdev_path" ] && continue
            [ -z "$(echo $netdev_path | grep usb)" ] && continue
            usb_slot=$(basename $(dirname $netdev_path))
            m_debug "netdev_path: $netdev_path usb slot: $usb_slot"
            [ -z "$usb_slots" ] && usb_slots="$usb_slot" || usb_slots="$usb_slots $usb_slot"
        done
    done
}

scan_pcie()
{
    #beta
    m_debug "scan_pcie"
    pcie_net_device_prefixs="rmnet"
    pcie_slots=""
    for pcie_net_device_prefix in $pcie_net_device_prefixs; do
        pcie_netdev=$(ls /sys/class/net | grep -E "${pcie_net_device_prefix}")
        for netdev in $pcie_netdev; do
            netdev_path=$(readlink -f "/sys/class/net/$netdev/device/")
            [ -z "$netdev_path" ] && continue
            [ -z "$(echo $netdev_path | grep pci)" ] && continue
            pcie_slot=$(basename $(dirname $netdev_path))
            [ "$pcie_slot" == "net" ] && continue
            m_debug "netdev_path: $netdev_path pcie slot: $pcie_slot"
            [ -z "$pcie_slots" ] && pcie_slots="$pcie_slot" || pcie_slots="$pcie_slots $pcie_slot"
        done
    done
}

scan_pcie_slot_interfaces()
{
    local slot=$1
    local slot_path="/sys/bus/pci/devices/$slot"
    net_devices=""
    dun_devices=""
    [ ! -d "$slot_path" ] && return
    local short_slot_name=`echo ${slot:2:-2} |tr ":" "."`
    local slot_interfaces=$(ls $slot_path | grep -E "_*${short_slot_name}_")
    for interface in $slot_interfaces; do
        unset device
        unset dun_device
        interface_driver_path="$slot_path/$interface/driver"
        [ ! -d "$interface_driver_path" ] && continue
        interface_driver=$(basename $(readlink $interface_driver_path))
        [ -z "$interface_driver" ] && continue
        case $interface_driver in
            mhi_netdev)
                net_path="$slot_path/$interface/net"
                [ ! -d "$net_path" ] && continue
                device=$(ls $net_path)
                [ -z "$net_devices" ] && net_devices="$device" || net_devices="$net_devices $device"
                ;;
            mhi_uci_q)
                dun_device=$(ls "$slot_path/$interface/mhi_uci_q" | grep mhi_DUN)
                [ -z "$dun_device" ] && continue
                dun_device_path="$slot_path/$interface/mhi_uci_q/$dun_device"
                [ ! -d "$dun_device_path" ] && continue
                dun_device_path=$(readlink -f "$dun_device_path")
                [ ! -d "$dun_device_path" ] && continue
                dun_device=$(basename "$dun_device_path")
                [ -z "$dun_device" ] && continue
                [ -z "$dun_devices" ] && dun_devices="$dun_device" || dun_devices="$dun_devices $dun_device"
                ;;
        esac
    done
    m_debug "net_devices: $net_devices dun_devices: $dun_devices"
    at_ports="$dun_devices" 
    [ -n "$net_devices" ] && get_associate_usb $slot
    if [ -n "$associated_usb" ]; then
        echo checking associated_usb: $associated_usb
        local assoc_usb_path="/sys/bus/usb/devices/$associated_usb"
        [ ! -d "$assoc_usb_path" ] && return
        local slot_interfaces=$(ls $assoc_usb_path | grep -E "$associated_usb:[0-9]\.[0-9]+")
        echo checking slot_interfaces: $slot_interfaces
        for interface in $slot_interfaces; do
            unset device
            unset ttyUSB_device
            unset ttyACM_device
            interface_driver_path="$assoc_usb_path/$interface/driver"
            [ ! -d "$interface_driver_path" ] && continue
            interface_driver=$(basename $(readlink $interface_driver_path))
            [ -z "$interface_driver" ] && continue
            case $interface_driver in
                option|\
                cdc_acm|\
                usbserial_generic|\
                usbserial)
                    ttyUSB_device=$(ls "$assoc_usb_path/$interface/" | grep ttyUSB)
                    ttyACM_device=$(ls "$assoc_usb_path/$interface/" | grep ttyACM)
                    [ -z "$ttyUSB_device" ] && [ -z "$ttyACM_device" ] && continue
                    [ -n "$ttyUSB_device" ] && device="$ttyUSB_device"
                    [ -n "$ttyACM_device" ] && device="$ttyACM_device"
                    [ -z "$tty_devices" ] && tty_devices="$device" || tty_devices="$tty_devices $device"
                ;;
            esac 
        done
        at_ports="$dun_devices $tty_devices"
    fi
        
    validate_at_port
}

scan_usb_slot_interfaces()
{
    local slot=$1
    local slot_path="/sys/bus/usb/devices/$slot"
    net_devices=""
    tty_devices=""
    [ ! -d "$slot_path" ] && return
    local slot_interfaces=$(ls $slot_path | grep -E "$slot:[0-9]\.[0-9]+")
    for interface in $slot_interfaces; do
        unset device
        unset ttyUSB_device
        unset ttyACM_device
        interface_driver_path="$slot_path/$interface/driver"
        [ ! -d "$interface_driver_path" ] && continue
        interface_driver=$(basename $(readlink $interface_driver_path))
        [ -z "$interface_driver" ] && continue
        case $interface_driver in
            option|\
            cdc_acm|\
            usbserial_generic|\
            usbserial)
                ttyUSB_device=$(ls "$slot_path/$interface/" | grep ttyUSB)
                ttyACM_device=$(ls "$slot_path/$interface/" | grep ttyACM)
                [ -z "$ttyUSB_device" ] && [ -z "$ttyACM_device" ] && continue
                [ -n "$ttyUSB_device" ] && device="$ttyUSB_device"
                [ -n "$ttyACM_device" ] && device="$ttyACM_device"
                [ -z "$tty_devices" ] && tty_devices="$device" || tty_devices="$tty_devices $device"
            ;;
            qmi_wwan*|\
            cdc_mbim|\
            cdc_ncm|\
            cdc_ether|\
            rndis_host)
                net_path="$slot_path/$interface/net"
                [ ! -d "$net_path" ] && continue
                device=$(ls $net_path)
                [ -z "$net_devices" ] && net_devices="$device" || net_devices="$net_devices $device"
            ;;
        esac 
    done
    echo "net_devices: $net_devices tty_devices: $tty_devices"
    at_ports="$tty_devices"
    validate_at_port
}

validate_at_port()
{
    valid_at_ports=""
    for at_port in $at_ports; do
        dev_path="/dev/$at_port"
        [ ! -e "$dev_path" ] && continue
        res=$(fastat $dev_path "ATI")
        [ -z "$res" ] && continue
        [[ "$res" != *"OK"* ]] && continue
        valid_at_port="$at_port"
        [ -z "$valid_at_ports" ] && valid_at_ports="$valid_at_port" || valid_at_ports="$valid_at_ports $valid_at_port"
    done
}

match_config()
{
    local name=$(echo $1 | sed 's/\r//g' | tr 'A-Z' 'a-z')
    [[ "$name" = *"nl668"* ]] && name="nl668"
    [[ "$name" = *"nl678"* ]] && name="nl678"

	[[ "$name" = *"em120k"* ]] && name="em120k"

	#FM350-GL-00 5G Module
	[[ "$name" = *"fm350-gl"* ]] && name="fm350-gl"

	#RM500U-CNV
	[[ "$name" = *"rm500u-cn"* ]] && name="rm500u-cn"

	[[ "$name" = *"rm500u-ea"* ]] && name="rm500u-ea"

	#rg200u-cn
    [[ "$name" = *"rg200u-cn"* ]] && name="rg200u-cn"

    modem_config=$(echo $modem_support | jq '.modem_support."'$slot_type'"."'$name'"')
    [ "$modem_config" == "null"  ] && return
    [ -z "$modem_config"  ] && return
    modem_name=$name
    manufacturer=$(echo $modem_config | jq -r ".manufacturer")
    platform=$(echo $modem_config | jq -r ".platform")
    define_connect=$(echo $modem_config | jq -r ".define_connect")
    modes=$(echo $modem_config | jq -r ".modes[]")
}

get_modem_model()
{
    local at_port=$1
    cgmm=$(at $at_port "AT+CGMM")
    sleep 1
    cgmm_1=$(at $at_port "AT+CGMM?")
    name_1=$(echo -e "$cgmm" |grep "+CGMM: " | awk -F': ' '{print $2}')
    name_2=$(echo -e "$cgmm_1" |grep "+CGMM: " | awk -F'"' '{print $2} '| cut -d ' ' -f 1)
    name_3=$(echo -e "$cgmm" | sed -n '2p')
    modem_name=""

    [ -n "$name_1" ] && match_config "$name_1"
    [ -n "$name_2" ] && [ -z "$modem_name" ] && match_config "$name_2"
    [ -n "$name_3" ] && [ -z "$modem_name" ] && match_config "$name_3"
    [ -z "$modem_name" ] && return 1
    return 0
}

add()
{
    local slot=$1
    lock -n /tmp/lock/modem_add_$slot
    [ $? -eq 0 ] || return
    #slot_type is usb or pcie
    #section name is replace slot .:- with _ 
    section_name=$(echo $slot | sed 's/[\.:-]/_/g')
    is_exist=$(uci -q get qmodem.$section_name)
    case $slot_type in
        "usb")
            scan_usb_slot_interfaces $slot
            modem_path="/sys/bus/usb/devices/$slot/"
            ;;
        "pcie")
            #under test
            scan_pcie_slot_interfaces $slot
            modem_path="/sys/bus/pci/devices/$slot/"
            ;;
    esac
    #if no netdev return
    [ -z "$net_devices" ] && lock -u /tmp/lock/modem_add_$slot && return
    for trys in $(seq 1 3);do
        for at_port in $valid_at_ports; do
            m_debug "try at port $at_port;time $trys"
            get_modem_model "/dev/$at_port"
            [ $? -eq 0 ] && break || modem_name=""
        done
        [ -n "$modem_name" ] && break
        sleep 1
    done
    [ -z "$modem_name" ] && lock -u /tmp/lock/modem_add_$slot && return
    m_debug  "add modem $modem_name slot $slot slot_type $slot_type"
    if [ -n "$is_exist" ]; then
        #network at_port state name 不变，则不需要重启网络
        orig_network=$(uci -q get qmodem.$section_name.network)
        orig_at_port=$(uci -q get qmodem.$section_name.at_port)
        orig_state=$(uci -q get qmodem.$section_name.state)
        orig_name=$(uci -q get qmodem.$section_name.name)
        uci -q del qmodem.$section_name.modes
        uci -q del qmodem.$section_name.valid_at_ports
        uci -q del qmodem.$section_name.tty_devices
        uci -q del qmodem.$section_name.net_devices
        uci -q del qmodem.$section_name.ports
        uci -q set qmodem.$section_name.state="enabled"
    else
    
    #aqcuire lock
        lock /tmp/lock/modem_add
        unset default_alias
        unset default_metric
        get_default_alias $slot
        get_default_metric $slot
        modem_count=$(uci -q get qmodem.main.modem_count)
        [ -z "$modem_count" ] && modem_count=0
        modem_count=$(($modem_count+1))
        uci set qmodem.main.modem_count=$modem_count
        uci set qmodem.$section_name=modem-device
        [ -n "$default_alias" ] && uci set  qmodem.${section_name}.alias="$default_alias"
        uci commit qmodem
        lock -u /tmp/lock/modem_add
    #release lock
        metric=$(($modem_count+10))
        [ -n "$default_metric" ] && metric=$default_metric
        uci batch << EOF
set qmodem.$section_name.path="$modem_path"
set qmodem.$section_name.data_interface="$slot_type"
set qmodem.$section_name.enable_dial="1"
set qmodem.$section_name.pdp_type="ip"
set qmodem.$section_name.state="enabled"
set qmodem.$section_name.metric=$metric
EOF
    fi
    uci batch <<EOF
set qmodem.$section_name.name=$modem_name
set qmodem.$section_name.network=$net_devices
set qmodem.$section_name.manufacturer=$manufacturer
set qmodem.$section_name.platform=$platform
set qmodem.$section_name.define_connect=$define_connect
EOF
    for mode in $modes; do
        uci add_list qmodem.$section_name.modes=$mode
    done
    for at_port in $valid_at_ports; do
        uci add_list qmodem.$section_name.valid_at_ports="/dev/$at_port"
        uci set qmodem.$section_name.at_port="/dev/$at_port"
    done
    for at_port in $at_ports; do
        uci add_list qmodem.$section_name.ports="/dev/$at_port"
    done
    uci commit qmodem
    mkdir -p /var/run/qmodem/${section_name}_dir
    lock -u /tmp/lock/modem_add_$slot
#判断是否重启网络
    [ -n "$is_exist" ] && [ "$orig_network" == "$net_devices" ] && [ "$orig_at_port" == "/dev/$at_port" ] && [ "$orig_state" == "enabled" ] && [ "$orig_name" == "$modem_name" ] && return
    /etc/init.d/qmodem_network restart
}

remove()
{
    section_name=$1
    m_debug  "remove $section_name"
    is_exist=$(uci -q get qmodem.$section_name)
    [ -z "$is_exist" ] && return
    lock /tmp/lock/modem_remove
    modem_count=$(uci -q get qmodem.main.modem_count)
    [ -z "$modem_count" ] && modem_count=0
    modem_count=$(($modem_count-1))
    uci set qmodem.main.modem_count=$modem_count
    uci commit qmodem
    uci batch <<EOF
del qmodem.${section_name}
del network.${section_name}
del network.${section_name}v6
del dhcp.${section_name}
commit network
commit dhcp
commit qmodem
EOF
    lock -u /tmp/lock/modem_remove    
}

disable()
{
    local slot=$1
    section_name=$(echo $slot | sed 's/[\.:-]/_/g')
    #reorder to first
    uci reorder qmodem.$section_name="1"
    uci set qmodem.$section_name.state="disabled"
    uci commit qmodem
}



case $action in
    "add")
        debug_subject="modem_scan_add"
        add $config $slot_type
        ;;
    "remove")
        debug_subject="modem_scan_remove"
        remove $config
        ;;
    "disable")
        debug_subject="modem_scan_disable"
        disable $config
        ;;
    "scan")
        debug_subject="modem_scan_scan"
        [ -n "$config" ] && delay=$config && sleep $delay
        lock -n /tmp/lock/modem_scan 
        [ $? -eq 1 ] && exit 0
        scan $slot_type
        lock -u /tmp/lock/modem_scan
        ;;
esac
