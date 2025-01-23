# 工具介绍

## 更新介绍

### 0.9.2

#### 特性变更

- 在AT通信中，优化了响应处理逻辑：在接收到初始响应后的特定时间内如果没有进一步的响应，则认为通信已结束。这种机制特别适用于那些可能无法正确返回终止符号的命令。（需要注意的是，当ATE功能启用时，命令回显会立即被读取，这可能导致对于执行时间较长的命令提前返回结果。）
- 在操作执行时，若是模组通讯异常（无返回、护法读写等），会在run_ops函数中返回-1，然后直接发送kill -9 命令（避免close时大量的花销），正常返回则返回0，若是与模组通讯无关的故障（如解析异常）应当返回>0的错误代码

#### 优化

- 更新了超时处理机制，不再依赖alarm信号，而是通过设置阻塞的读写操作，并结合vmin和vtime参数实现超时控制。
- 实现了组件间的一定程度解耦，提高了系统的灵活性和可维护性。

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

  - `at` (a) 无-o参数默认为at，命令为tom_modem -d /dev/mhi_DUN  -o b -c ATI时，会向/dev/mhi_DUN发送ATI\r\n (即会自动往命令追加\r\n)
  - ```binary_at``` (b) 需要发送控制字符（如^z）等，可使用16进制发送，如tom_modem -d /dev/mhi_DUN  -o b -c 4154490D0A 表示向/dev/mhi_DUN发送ATI\r\n。（注意，此选项不会自动追加\r\n 。）
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
