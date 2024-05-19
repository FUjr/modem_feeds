#!/bin/sh
sleep 15
logger -t usb_modem_scan "Start to scan USB modem"
source /usr/share/modem/modem_scan.sh
modem_scan
