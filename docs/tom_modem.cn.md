# 工具介绍

## 概述

这个工具是一个 AT 命令行界面，用于与调制解调器进行通信。它支持多种操作，包括发送和读取短信、设置波特率和数据位等。

## 使用方法

### 命令格式

```bash
usage: <tool_name> [options]
```

### 选项

- `-c, --at_cmd <AT command>`
  指定要发送的 AT 命令。
- `-d, --tty_dev <TTY device>`
  指定 TTY 设备 **必填**。
- `-b, --baud_rate <baud rate>`
  设置波特率，默认值为 115200。支持的波特率有：4800, 9600, 19200, 38400, 57600, 115200。
- `-B, --data_bits <data bits>`
  设置数据位，默认值为 8。支持的数据位有：5, 6, 7, 8。
- `-t, --timeout <timeout>`
  设置超时时间，默认值为 3 秒。
- `-o, --operation <operation>`指定操作类型，支持的操作有：

  - `at` (a) 无-o参数默认为at
  - `sms_read` (r)
  - `sms_send` (s)
  - `sms_delete` (d)
- `-D, --debug`
  启用调试模式，默认值为关闭。可以打印大量该工具的台哦是信息和与模组通信的原始数据。
- `-p, --sms_pdu <sms pdu>`
  指定 SMS PDU。
- `-i, --sms_index <sms index>`
  指定 SMS 索引。

### 示例

- 设置 AT 命令和波特率、数据位：

  ```bash
  <tool_name> -c ATI -d /dev/ttyUSB2 -b 115200 -B 8 -o at
  ```
- 普通 AT 模式：

  ```bash
  <tool_name> -c ATI -d /dev/ttyUSB2
  ```
- 读取短信：

  ```bash
  <tool_name> -d /dev/mhi_DUN -o r
  ```
