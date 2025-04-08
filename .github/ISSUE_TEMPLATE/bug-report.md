---
name: Bug Report
about: Create a report to help us improve
title: "[BUG]"
labels: bug
assignees: ''

---

以下是问题报告模板，包含中文和英文翻译：

---

**描述问题**  
提供模组型号、路由器平台、路由器系统发行版等信息  
**Describe the Issue**  
Provide module model, router platform, router system distribution, etc.

---

**复现方法**  
**Steps to Reproduce**  
1. Go to '...'  
2. Click on '...'  
3. Scroll down to '...'  
4. See the error

---

**期望行为**  
描述正常情况下应该是什么行为  
**Expected Behavior**  
Describe what the normal behavior should be.

---

**屏幕截图**  
**Screenshots**  

---

**日志信息**  
请在终端执行以下命令并将结果粘贴到此处  
**Log Information**  
Please execute the following commands in the terminal and paste the results below:

```bash
uci show qmodem
# 显示模组配置
# Show module configuration

uci show network
# 显示网络配置
# Show network configuration

logread
# 查看系统日志
# View system logs

dmesg
# 查看内核日志
# View kernel logs
```

如果是USB模组相关的问题，执行以下命令：  
**If the issue is related to USB modules, execute the following command:**

```bash
lsusb
# 列出USB设备
# List USB devices
```

如果是PCIe模组相关的问题，执行以下命令：  
**If the issue is related to PCIe modules, execute the following command:**

```bash
lspci
# 列出PCIe设备
# List PCIe devices
```

如果模组扫描存在问题，执行以下命令：  
**If there is an issue with module scanning, execute the following command:**

```bash
/usr/share/qmodem/modem_scan.sh scan
# 扫描模组
# Scan modules
```

---
