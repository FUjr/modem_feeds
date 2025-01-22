# 增加模组支持文档

## 流程简介

在以下几个地方会触发模组扫描
负责扫描的都是 ```luci/luci-app-qmodem/root/usr/share/qmodem/modem_scan.sh``` 脚本
### 模组扫描流程：

1. qmodem_init 服务
2. 网卡和usb的hotplug事件
3. 网页端的手动扫描

*PCIe模组扫描* 遍历 ```/sys/class/net/``` 检查是否存在 ```/sys/bus/pci[e]``` 下的设备,如果存在且含有 加载了串口驱动的接口，则尝试添加该设备
*USB模组扫描* 遍历 ```/sys/class/net/``` 检查是否存在 ```/sys/bus/usb``` 下的设备,如果存在且含有 加载了串口驱动的接口，则尝试添加该设备
*监控预设的USB端口* 遍历 uci 配置 ```qmodem.@modem-slot``` , 扫描 ```slot_type``` 为 ```usb``` 的设备，如果存在且含有加载了串口驱动的接口，尝试添加该设备
*监控预设的PCIe端口* 遍历 uci 配置 ```qmodem.@modem-slot``` , 扫描 ```slot_type``` 为 ```pcie``` 的设备，如果存在且含有加载了串口驱动的接口，尝试添加该设备

### 尝试添加设备流程：
设备经过扫描流程后，会将 devpath 和 slot_type 传入添加流程
设备添加时会向设备发送 ```ATI``` 命令，如果包含返回 ```OK``` 字符串的则认为该端口可用，标记为可用端口后加入列表
检查完所有端口后