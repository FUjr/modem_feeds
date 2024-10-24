# QModem

[English](README.en.md)

这是一个模组管理插件，兼容 Openwrt 21及之后的版本，使用 lua 开发，因此同时兼容 QWRT/LEDE/Immortalwrt/Openwrt

(使用 js luci 时请添加 luci-compat 软件包)

[TOC]



# 使用方法

### 增加feed源

```
echo >> feeds.conf.default
echo 'src-git modem https://github.com/FUjr/modem_feeds.git;main' >> feeds.conf.default
./scripts/feeds update modem
./scripts/feeds install -a -p modem
```

### 集成软件包

```
make menuconfig
```

### 选择软件包

```shell
< > luci-app-qmodem.............................. LuCI support for QWRT Modem
[ ] Add Lua Luci Homepage                                                 
[ ] Add PCIe Modem SUPPORT                                               
[ ] Using Tom customized Quectel CM                                       
[ ] Using QWRT quectel-CM-5G                                             
[ ] Using Normal quectel-cm                                               
< > luci-app-qmodem-hc..................................... hc-g80 sim switch
< > luci-app-qmodem-mwan........................ Luci qwrt modem mwan support
< > luci-app-qmodem-sms.......................... Luci qwrt modem sms support
< > luci-app-qmodem-ttl.......................... Luci qwrt modem ttl support
```

### 软件包介绍

**luci-app-qmodem**
LuCI 支持 QWRT Modem。该应用程序为 QWRT 路由器提供图形用户界面，使用户能够方便地管理和配置调制解调器设置。

**Add Lua Luci Homepage**
添加 Lua Luci 首页。此选项允许用户将模组信息添加到Lua LuCI 界面中首页。

**Add PCIe Modem SUPPORT**
添加 PCIe 调制解调器支持。此选项使系统能够识别和使用通过 PCIe 接口连接的调制解调器。

**Using Tom customized Quectel CM**
使用 Tom 定制的 Quectel CM，Tom定制的quectel-cm增加了跃点数选项，使用quectel-cm工具拨号不再只能是默认路由。

**Using QWRT quectel-CM-5G**
QWRT仓库中的 quectel-cm 的软件包名为quectel-CM-5G,这是一个兼容性选项。

**Using Normal quectel-cm**
使用普通的 quectel-cm，如果在其他仓库，不希望使用Tom定制的quectel-cm，则可选择此项。

**luci-app-qmodem-hc**
hc-g80 SIM 切换。此应用程序允许用户在 hc-g80 cpe中方便地切换不同的 SIM 卡，可配置watchdog，断网自动切卡。

**luci-app-qmodem-mwan**
Luci QWRT 调制解调器 MWAN 支持。该应用程序为多 WAN 设置提供简易界面，用户能够在多个网络之间进行负载均衡和故障转移。

**luci-app-qmodem-sms**
Luci QWRT 调制解调器 SMS 支持。此应用程序允许用户通过调制解调器发送和接收短信，提供便捷的消息通信功能。

**luci-app-qmodem-ttl**
Luci QWRT 调制解调器 TTL 支持。此选项可设置某个接口的ttl和hl。

# 项目介绍

## 为什么选择该项目

- **稳定性**：通过缓存和减少 AT 指令的次数，提高了系统的稳定性。
- **可扩展性**：最小化 API 端点和统一后端程序设计，便于二次开发和扩展。
- **可靠性**：功能分离设计，确保核心功能的稳定性，即使其他功能出现问题也不影响主要使用。
- **多模组支持**: 根据 slot 定位模组，模组和配置有一对一的绑定关系，即使重启或热插拔模组也不会造成模组和配置混淆。
- **短信支持**: 长短信合并、中文短信发送
- **多语言支持**: 开发时将语言资源分离，可以添加需要的语言
- **IPV6支持**: 部分支持ipv6 ，测试条件 （移动卡 rm50xq qmi/rmnet/mbim 驱动，使用quectel-CM-M拨号，使用扩展前缀模式）

#### [全新实现的AT工具](docs/tom_modem.cn.md)

* 尽管 sendat、sms_tool 和 gl_modem_at 这三个工具在大多数情况下表现出色，能够满足大部分需求，但它们在超时机制、mhi_DUN 和短信支持方面各自存在一些小问题。如果想要同时使用所有功能，就必须内置这三个 AT 工具，这显然不够优雅，因此我参考这三个工具，实现了一个包含所有功能的at工具。
* 支持使用 ```-t ```选项设置超时
* 支持使用 ```-o``` 选项 选择AT、发短信、收短信、删短信功能
* 支持 ```-b``` 选项设置波特率 

#### 修改版本的quectel-cm

* 默认版本的quectel-cm不支持指定默认路由的跃点数，导致会清空默认路由，对多wan用户不友好，我增加补丁支持了跃点数的选项

#### 缓存机制

- **减少了发 AT 指令的次数**：通过缓存模组信息，降低了直接与模组通信的频率，从而提高了系统的稳定性。
- **多窗口支持**：即使同时开启多个窗口查看模组信息，也不会导致模组死机。

#### API 设计

- **最小化 API 端点**：暴露尽可能少的 API 端点，大部分模组信息和模组设置均使用同一端点，简化了二次开发。
- **统一后端程序**：所有与拨号无关的模组通信均采用统一的后端程序，便于维护和扩展。

#### 功能分离

- **解耦合设计**
  - 模组信息和设置、模组拨号、短信收发、多 WAN 设置、TTL 设置 模块解耦
  - 前后端解耦，便于后续升级c语言实现的后端 和更先进的 js luci
- **稳定性保障**：确保即使某一些功能挂了也不影响最重要的上网稳定性。

### 主程序

项目主程序为luci-app-qmodem（原谅我将后端程序也放在了这里），有模组信息、拨号总览、模组调试三大功能块。由于主程序在这里，因此其他功能依赖该程序。

#### 模组信息

<img src="imgs/homepage.png" style="zoom: 25%;" alt="在首页显示（Lua)" />

<img src="imgs/modem_info.png" style="zoom: 25%;" />

#### 模组高级设置

页面顶部有一个模块选择器，可以选择不同的模块,选择后可进行拨号模式、制式偏号、IMEI设置、锁小区、锁频段等设置，当然这些功能需要模组支持

<img src="imgs/modem_debug_lock_cell.png" style="zoom:25%;" />

<img src="imgs/modem_debug_lock_band.png" style="zoom:25%;" />

#### 拨号总览

<img src="imgs/dial_overview.png" style="zoom:25%;" />

##### 全局配置

提供全局性的配置选项，允许用户进行统一的模组配置。

- **重新加载拨号**：重新加载模组的配置文件，确保配置生效。
- **拨号总开关**: 拨号总开关，启用后才会进行拨号

##### 配置列表

显示当前配置的模组列表，提供模组的详细信息。

- **模组位置**：显示模组的物理位置或插槽编号。
- **状态**：显示模组的当前状态（例如：已启用、禁用）。
- **别名**：设置别名后，网络接口名会设置为别名，模块选择器和日志也会显示别名。因此别名不可重复、不得含有空格及特殊符号

##### 拨号状态和日志

启用拨号后实时显示模组的当前拨号信息和拨号日志，便于用户查看模组的详细运行情况和排查问题。可以下载或清除日志。

### 短信

包名 luci-app-qmodem-sms ，该页面主要用于短信(SMS)的管理和发送，页面顶部有一个模块选择器，可以选择不同的模块,页面会显示与该模块相关的短信信息。用户可以查看和管理已有的短信记录，并且可以向指定号码发送新的短信。
![](imgs/modem_sms.png)

**短信列表**

页面中部显示了一个短信列表，每条短信包括**发信人**、 **时间**、**内容**，每条短信旁边都有一个删除按钮，点击可以删除该条短信。

**发送短信**

- **电话号码**：输入接收短信的电话号码。如10086、8613012345678
- **短信内容**：中文短信会在前端使用js编码，ascii短信则在后端编码

### Mwan配置

该页面是 **MWAN 配置** 界面，帮助用户管理多 WAN 连接，通过监控特定 IP 来确保网络的稳定性和可靠性。用户可以根据需求自定义连接的优先级和接口，从而实现负载均衡或故障转移

1. **启用 MWAN**

   - **相同源地址**: 选中此框后，路由器将在一定时间内使用相同的 WAN 端口处理来自同一源的流量。

2. **IPv4 配置**

   - **接口**: 选择要添加的 WAN 接口（如 `wan`、`usb0` 等），以便于配置不同的网络连接。

   - **跟踪IP**: 通过输入特定的 IP 地址或域名

     **优先级**: 设置连接的优先级，范围为 1 到 255，数值越低优先级越高。

### QModem 设置

- **禁用自动加载/移除模组**: 关闭以下所有功能。
- **启用 PCIe 模块扫描**: 选中后，系统会在开机时扫描 PCIe 接口。（耗时较长）
- **启用 USB 模块扫描**: 选中后，系统会在开机时扫描 USB 接口。（耗时较长）
- **监控设置的 USB 接口**: 系统会在开机时扫描插槽配置里的 USB 端口，同时监控usb的热插拔事件。
- **监控设置的 PCIe 接口**: 系统会在开机时扫描插槽配置里的 PCIe 端口。

##### 插槽配置

该页面允许用户对每个插槽进行一些设置

1. **插槽类型**
   - 选择插槽的类型（ PCIe/USB），用于识别设备。
2. **插槽 ID**
   - 输入设备的唯一标识符（如 `0001:11:00.0[pcie]`），用于设备识别。
3. **SIM 卡指示灯**
   - 绑定插槽与相应的指示灯，以显示 SIM 卡的状态。
4. **网络指示灯**
   - 绑定插槽的网络状态指示灯，以便监控网络连接的状态。
5. **启用 5G 转网络口**
   - 某些 CPE 设备模组接口设置了网卡芯片，允许模组通过 PHY 与路由器通信，从而提高性能。启用此选项可使支持转网口功能的模组通过网络接口与主机通信。
6. **关联的 USB**
   - 全功能的 m.2 接口包含pcie和usb协议，配置该项可将usb端口与pcie端口关联，用户使用同时支持pcie和usb的模组时，可以使用兼容性更好的usb serial驱动进行at通信



## 开发计划


| 计划                                              | 进度               |
| ------------------------------------------------- | ------------------ |
| 将后端程序与luci-app完全分离                      | 0                  |
| 修复quectel-CM乱call udhcpd和删除默认路由表的问题 | 基本完成           |
| 加入pcie模组支持                                  | 实验性支持         |
| 自己实现at收发程序                                | 基本完成           |
| 切换js luci                                       | 5%                 |
| 修复ipv6                                          | 使用quectel-cm支持 |
| 优化模组扫描逻辑                                  | 基本完成           |
| 模组led展示                                       | 基本完成           |

# 鸣谢

在模组管理插件的开发过程中，参考了以下仓库


| 项目                                         |       参考内容       |
| -------------------------------------------- | :------------------: |
| https://github.com/Siriling/5G-Modem-Support | 模组列表和部分at实现 |
| https://github.com/fujr/luci-app-4gmodem     | 沿用该项目大部分思想 |
| https://github.com/obsy/sms_tool             |    AT命令发送工具    |
| https://github.com/gl-inet/gl-modem-at       |    AT命令发送工具    |
| https://github.com/ouyangzq/sendat           |    AT命令发送工具    |

# 
