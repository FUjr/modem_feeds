# QModem

This is a module management plugin compatible with OpenWrt version 21 and later, developed using Lua, and thus compatible with QWRT/LEDE/Immortalwrt/Openwrt.

(When using js luci, please add the luci-compat package)

[toc]

# Usage

### Adding Feed Source

```bash
echo >> feeds.conf.default
echo 'src-git modem https://github.com/FUjr/modem_feeds.git;main' >> feeds.conf.default
./scripts/feeds update modem
./scripts/feeds install -a -p modem
```

### Integrating Packages

```bash
make menuconfig
```

### Selecting Packages

```shell
< > luci-app-qmodem.............................. LuCI support for QWRT Modem
[ ] Add Lua Luci Homepage                                               
[ ] Add PCIe Modem SUPPORT                                             
[ ] Using Tom customized Quectel CM                                     
[ ] Using QWRT quectel-CM-5G                                           
[ ] Using Normal quectel-cm                                             
< > luci-app-qmodem-hc..................................... hc-g80 SIM switch
< > luci-app-qmodem-mwan........................ Luci QWRT modem MWAN support
< > luci-app-qmodem-sms.......................... Luci QWRT modem SMS support
< > luci-app-qmodem-ttl.......................... Luci QWRT modem TTL support
```

### Package Descriptions

**luci-app-qmodem**
LuCI support for QWRT Modem. This application provides a graphical user interface for QWRT routers, allowing users to easily manage and configure modem settings.

**Add Lua Luci Homepage**
Adds module information to the Lua LuCI homepage. This option allows users to display module information on the homepage of the LuCI interface.

**Add PCIe Modem SUPPORT**
Enables PCIe modem support. This option allows the system to recognize and use modems connected via the PCIe interface.

**Using Tom customized Quectel CM**
Utilizes Tom's customized Quectel CM, which adds the option for hop count, allowing the use of the quectel-cm tool for dialing without being limited to the default route.

**Using QWRT quectel-CM-5G**
This is a compatibility option for the quectel-cm package in the QWRT repository.

**Using Normal quectel-cm**
Uses the standard quectel-cm. This option is available if you do not wish to use Tom's customized version from other repositories.

**luci-app-qmodem-hc**
hc-g80 SIM switch. This application allows users to easily switch between different SIM cards in the hc-g80 CPE, with configurable watchdog functionality that automatically switches cards upon disconnection.

**luci-app-qmodem-mwan**
Luci QWRT modem MWAN support. This application provides a simple interface for managing multi-WAN settings, allowing users to load balance and failover between multiple networks.

**luci-app-qmodem-sms**
Luci QWRT modem SMS support. This application allows users to send and receive SMS messages via the modem, providing convenient communication functionality.

**luci-app-qmodem-ttl**
Luci QWRT modem TTL support. This option allows users to set the TTL and HL for a specific interface.

# Project Introduction

## Why Choose This Project

- **Stability**: Improved system stability through caching and reducing the number of AT commands.
- **Scalability**: Minimal API endpoints and unified backend design facilitate secondary development and expansion.
- **Reliability**: Function separation design ensures the stability of core functionalities, even if other features encounter issues.
- **Multi-module Support**: Modules are identified by their slots, with a one-to-one binding between modules and configurations, preventing confusion during reboots or hot-plugging.
- **SMS Support**: Supports concatenated SMS and sending messages in Chinese.
- **Multi-language Support**: Language resources are separated during development, allowing for the addition of required languages.
- **IPv6 Support**: Partial support for IPv6, tested with specific conditions (mobile card rm50xq qmi/rmnet/mbim driver, using quectel-CM-M for dialing, and employing extended prefix mode).

#### Newly Implemented AT Tool

* Although the tools sendat, sms_tool, and gl_modem_at perform well in most cases, they each have minor issues with timeout mechanisms, mhi_DUN, and SMS support. To elegantly consolidate all functionalities, I referenced these tools and created a comprehensive AT tool.
* Supports the `-t` option to set timeouts.
* Supports the `-o` option to select AT commands, send SMS, receive SMS, and delete SMS functionalities.
* Supports the `-b` option to set baud rate.

#### Modified Version of Quectel-CM

* The default version of quectel-cm does not support specifying hop counts for the default route, which can clear the default route and is not user-friendly for multi-WAN users. I added a patch to support hop count options.

#### Caching Mechanism

- **Reduced the Number of AT Commands**: By caching module information, the frequency of direct communication with the module is decreased, enhancing system stability.
- **Multi-window Support**: Multiple windows can be opened to view module information without causing the module to hang.

#### API Design

- **Minimized API Endpoints**: Exposes as few API endpoints as possible, with most module information and settings using the same endpoint, simplifying secondary development.
- **Unified Backend Program**: All non-dialing related module communications use a unified backend program for easier maintenance and expansion.

#### Function Separation

- **Decoupled Design**
  - Separates module information and settings, dialing, SMS sending and receiving, multi-WAN settings, and TTL settings.
  - Frontend and backend are decoupled, facilitating future upgrades to a C language backend and more advanced JS luci.
- **Stability Assurance**: Ensures that even if some functionalities fail, the primary internet stability remains unaffected.

### Main Program

The main program of the project is luci-app-qmodem (I apologize for including the backend program here), which includes three main functional blocks: module information, dialing overview, and module debugging. Since the main program is here, other functionalities depend on it.

#### Module Information

<img src="imgs/homepage.png" style="zoom: 25%;" alt="Homepage Display (Lua)" />

<img src="imgs/modem_info.png" style="zoom: 25%;" />

#### Advanced Module Settings

At the top of the page, there is a module selector that allows users to choose different modules. Once selected, users can configure dialing modes, frequency bands, IMEI settings, cell locking, and band locking, provided that the module supports these features.

<img src="imgs/modem_debug_lock_cell.png" style="zoom:25%;" />

<img src="imgs/modem_debug_lock_band.png" style="zoom:25%;" />

#### Dialing Overview

<img src="imgs/dial_overview.png" style="zoom:25%;" />

##### Global Configuration

Provides global configuration options, allowing users to uniformly configure modules.

- **Reload Dialing**: Reloads the module's configuration file to ensure settings take effect.
- **Dialing Master Switch**: Enables dialing only when activated.

##### Configuration List

Displays the current list of configured modules, providing detailed information about each module.

- **Module Location**: Shows the physical location or slot number of the module.
- **Status**: Displays the current status of the module (e.g., enabled, disabled).
- **Alias**: After setting an alias, the network interface name will be set to the alias, and the module selector and logs will also display the alias. Therefore, aliases must be unique and cannot contain spaces or special characters.

##### Dialing Status and Logs

After enabling dialing, the current dialing information and logs of the module are displayed in real-time, allowing users to monitor the module's operation and troubleshoot issues. Logs can be downloaded or cleared.

### SMS

Package name luci-app-qmodem-sms, this page is primarily for managing and sending SMS messages. At the top of the page, there is a module selector that allows users to choose different modules, displaying SMS information related to the selected module. Users can view and manage existing SMS records and send new messages to specified numbers.

![](imgs/modem_sms.png)

**SMS List**

The middle of the page displays an SMS list, with each SMS showing the **Sender**, **Time**, and **Content**. Each SMS has a delete button next to it for easy removal.

**Send SMS**

- **Phone Number**: Enter the recipient's phone number, such as 10086 or 8613012345678.
- **SMS Content**: Chinese SMS will be encoded using JS on the frontend, while ASCII SMS will be encoded on the backend.

### MWAN Configuration

This page is the **MWAN Configuration** interface, helping users manage multiple WAN connections by monitoring specific IPs to ensure network stability and reliability. Users can customize connection priorities and interfaces as needed to achieve load balancing or failover.

1. **Enable MWAN**

   - **Same Source Address**: When this box is checked, the router will use the same WAN port to handle traffic from the same source for a certain period.
2. **IPv4 Configuration**

   - **Interface**: Select the WAN interface to be added (e.g., `wan`, `usb0`, etc.) for configuring different network connections.
   - **Tracking IP**: Enter specific IP addresses or domain names for tracking.

     **Priority**: Set the priority of the connection, ranging from 1 to 255, with lower values indicating higher priority.

### QModem Settings

- **Disable Automatic Load/Remove Modules**: Disable all related functions.
- **Enable PCIe Module Scanning**: When checked, the system will scan the PCIe interface at startup (may take time).
- **Enable USB Module Scanning**: When checked, the system will scan the USB interface at startup (may take time).
- **Monitor Configured USB Interfaces**: The system will scan the USB ports in the slot configuration at startup and monitor USB hot-plug events.
- **Monitor Configured PCIe Interfaces**: The system will scan the PCIe ports in the slot configuration at startup.

##### Slot Configuration

This page allows users to set configurations for each slot.

1. **Slot Type**

   - Select the type of slot (PCIe/USB) for device identification.
2. **Slot ID**

   - Enter the unique identifier for the device (e.g., `0001:11:00.0[pcie]`) for identification.
3. **SIM Card Indicator Light**

   - Bind the slot to the corresponding indicator light to display the SIM card status.
4. **Network Indicator Light**

   - Bind the network status indicator light to monitor the connection status.
5. **Enable 5G Network Port Switching**

   - Some CPE devices have module interfaces set with network card chips, allowing modules to communicate with routers via PHY to enhance performance. Enabling this option allows modules that support network port switching to communicate with the host through the network interface.
6. **Associated USB**

   - The multifunctional M.2 interface supports both PCIe and USB protocols. Configuring this option allows the USB port to be associated with the PCIe port, enabling users to use USB serial drivers for AT communication when using modules that support both PCIe and USB.

## Development Plan


| Plan                                                                                     | Progress                |
| ---------------------------------------------------------------------------------------- | ----------------------- |
| Completely separate the backend program from luci-app                                    | 0                       |
| Fix the issue of quectel-CM randomly calling udhcpd and deleting the default route table | Nearly Complete         |
| Add PCIe module support                                                                  | Experimental Support    |
| Implement my own AT send/receive program                                                 | Nearly Complete         |
| Switch to JS luci                                                                        | 5%                      |
| Fix IPv6                                                                                 | Supported by quectel-cm |
| Optimize module scanning logic                                                           | Nearly Complete         |
| Module LED Display                                                                       | Nearly Complete         |

# Acknowledgments

During the development of the module management plugin, the following repositories were referenced:


| Project                                      |            Reference Content            |
| -------------------------------------------- | :-------------------------------------: |
| https://github.com/Siriling/5G-Modem-Support | Module list and some AT implementations |
| https://github.com/fujr/luci-app-4gmodem     |  Adopted many ideas from this project  |
| https://github.com/obsy/sms_tool             |         AT command sending tool         |
| https://github.com/gl-inet/gl-modem-at       |         AT command sending tool         |
| https://github.com/ouyangzq/sendat           |         AT command sending tool         |
