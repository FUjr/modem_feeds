# SIP、SMSD 与 VoIP 配置

`qmodem-sipd` 是独立的实验性 SIP 传输服务。它可以只连接 `qmodem-voip`、只连接
`qmodem-smsd`，或同时连接两者。入站 SIP 和出站 SIP 拥有独立配置与 procd 实例，
可以分别启停，也可以同时运行。

## 安装和服务

```sh
apk add qmodem-voip qmodem-smsd qmodem-sipd
/etc/init.d/qmodem_voip enable
/etc/init.d/qmodem_smsd enable
/etc/init.d/qmodem_voip_sipd enable
```

三个服务可独立启动。启用了某个方向的 VoIP 连接时，sipd 会等待 `qmodem_voip`
就绪；SMS 接口则在收到 SIP MESSAGE 时动态检查 `qmodem.sms`。状态可通过以下命令查看：

```sh
ubus call qmodem_voip status
ubus call qmodem.sms status
ubus call qmodem.sip status
```

LuCI 页面地址如下（主机名可替换为设备管理地址）：

- SMS：`http://openwrt.lan/cgi-bin/luci/admin/modem/qmodem/sms`
- VoIP：`http://openwrt.lan/cgi-bin/luci/admin/modem/qmodem/qmodem-voip`
- SIPD：`http://openwrt.lan/cgi-bin/luci/admin/modem/qmodem/qmodem-sipd`

SIPD 使用独立页面和独立的 `qmodem_sip` 配置，不再出现在 VoIP 页面中。

从旧版升级时，原 `qmodem_voip.sip` 配置会按原模式迁移到 `qmodem_sip.inbound`
或 `qmodem_sip.outbound`，迁移成功后删除旧配置段和旧防火墙 include。

## QModem 与 SMSD

main 分支中新建的 modem 配置默认使用 ubus AT 通道。已有配置只有在缺少 `use_ubus`
时才会迁移为 `1`，显式配置的 `0` 会保留。

```sh
uci set qmodem.modem_1.use_ubus='1'
uci set qmodem.modem_1.sms_mode='database_poll'
uci commit qmodem
/etc/init.d/qmodem_smsd restart
```

SIP MESSAGE 转蜂窝短信必须使用 SMSD 数据库模式，并通过带幂等 `request_id` 的
`qmodem.sms send_managed` 调用。`sms_mode='direct'` 不支持该路径。

## VoIPD

`qmodem-voip` 只负责蜂窝通话状态、AT 控制和媒体，不再管理 SIP 监听或注册参数：

```sh
uci set qmodem_voip.main.enabled='1'
uci set qmodem_voip.main.web_enabled='0'
uci set qmodem_voip.main.interface='lan'
uci commit qmodem_voip
/etc/init.d/qmodem_voip restart
```

支持范围仍由 modem profile 决定；不支持的硬件会失败关闭，安装软件包或 SIP 注册成功
都不能替代真实设备上的双向音频验证。

## 入站 SIP

入站模式在指定 OpenWrt 网络接口上监听 UDP/TCP。只有启用入站方向时，防火墙脚本
才在该接口地址上开放 SIP 端口和 RTP 范围。

```sh
uci set qmodem_sip.inbound.enabled='1'
uci set qmodem_sip.inbound.interface='lan'
uci set qmodem_sip.inbound.listen_port='5060'
uci set qmodem_sip.inbound.transport='udp_tcp'
uci set qmodem_sip.inbound.voip_enabled='1'
uci set qmodem_sip.inbound.sms_enabled='1'
uci set qmodem_sip.inbound.sms_modem='modem_1'
uci set qmodem_sip.inbound.username='qmodem'
uci set qmodem_sip.inbound.password='REPLACE_WITH_A_STRONG_PASSWORD'
uci commit qmodem_sip
/etc/init.d/qmodem_voip_sipd restart
```

也可以生成一次性显示的新密码；密码会写入入站配置，但不会出现在状态接口中：

```sh
ubus call qmodem.sip generate_credentials '{"username":"qmodem"}'
```

收到经过认证的 `text/plain` SIP MESSAGE 后，Request-URI 的 user 部分会作为短信号码，
正文作为短信内容。只有 `send_managed` 返回成功时 sipd 才响应 SIP 200。

## 出站 SIP

出站模式仅支持带服务端证书校验的 TLS，并向指定 SIP 服务注册：

```sh
uci set qmodem_sip.outbound.enabled='1'
uci set qmodem_sip.outbound.interface='wan'
uci set qmodem_sip.outbound.listen_port='5061'
uci set qmodem_sip.outbound.server='pbx.example.net'
uci set qmodem_sip.outbound.port='5061'
uci set qmodem_sip.outbound.transport='tls'
uci set qmodem_sip.outbound.username='device-1001'
uci set qmodem_sip.outbound.password='REPLACE_ME'
uci set qmodem_sip.outbound.realm=''
uci set qmodem_sip.outbound.register_interval='300'
uci set qmodem_sip.outbound.voip_enabled='1'
uci set qmodem_sip.outbound.sms_enabled='0'
uci commit qmodem_sip
/etc/init.d/qmodem_voip_sipd restart
```

入站使用 `lan_sip`，出站使用 `external_sip` 参与 `qmodem_voip` 的呼叫归属仲裁；两者
可以同时启用，但同一时刻仍只有一个端点能接管一通蜂窝电话。

## SMS 转 SIP MESSAGE

`sms-forwarder-next` 的 `api_type='sip'` 不执行外部脚本，而是调用 `qmodem.sip
send_message`。目标方向必须正在运行；出站方向还必须已经注册，入站方向必须已有活跃
的本地 REGISTER 绑定。

```sh
uci set sms_forwarder.sms_forward.enable='1'
uci set sms_forwarder.sip_out='sms_forward_instance'
uci set sms_forwarder.sip_out.enable='1'
uci set sms_forwarder.sip_out.modem_cfg='modem_1'
uci set sms_forwarder.sip_out.poll_interval='30'
uci set sms_forwarder.sip_out.api_type='sip'
uci set sms_forwarder.sip_out.api_config='{"direction":"outbound","recipient_uri":"sip:sms@pbx.example.net"}'
uci commit sms_forwarder
/etc/init.d/sms_forwarder restart
```

转发成功以远端 SIP 2xx 响应为准。短信号码、内容、设备日志和配置备份不应提交到仓库。

### Asterisk MESSAGE 路由

Asterisk 需要为 QModem 和电话端点分别指定消息上下文。以下示例假定 QModem 端点为
`qmodem1001`，电话端点为 `1001`：

```ini
; pjsip.conf
[qmodem1001]
type=endpoint
message_context=qmodem-message
; 保留该端点已有的 transport、auth、aors 和媒体配置

[1001]
type=endpoint
message_context=phone-message
; 保留该端点已有的 transport、auth、aors 和媒体配置
```

```ini
; extensions.conf
[qmodem-message]
; 将 QModem 的真实短信发送方传给 SIP 客户端，客户端可直接点击回复
exten => 1001,1,Set(MESSAGE(from)=sip:${MESSAGE_DATA(X-QModem-SMS-From)}@pbx.example.net)
 same => n,MessageSend(pjsip:1001)
 same => n,NoOp(QModem message status: ${MESSAGE_SEND_STATUS})
 same => n,Hangup()

[phone-message]
exten => _X.,1,MessageSend(pjsip:PJSIP/${EXTEN}@qmodem1001)
 same => n,NoOp(Phone message status: ${MESSAGE_SEND_STATUS})
 same => n,Hangup()
```

`phone-message` 中的 `${EXTEN}` 必须原样保留到 QModem 的 SIP Request-URI；SIPD 会把
其中的 user 部分作为蜂窝短信收件号码。修改 bind mount 提供的配置文件后应重启 Asterisk
容器，再检查两个端点的 Contact 都恢复为 `Avail`。

### 故障诊断与验证

短信发送接口返回 `success` 只表示请求已进入发送流程；必须在设备日志中看到调制解调器返回
`+CMGS: <id>`，才算完成空口提交。若发送 PDU 以 `00` 开头，设备必须使用二进制长度写入，
不能用 C 字符串长度，否则会出现“接口成功但没有短信”的假成功。

长短信可能由多个 `+CMTI` 事件分段到达。数据库模式会保留尚未拼完整的分组，直到等待窗口
过期；同一个 UDH 引用在一条短信完成后再次出现时会新建分组，不会把新短信拼到旧消息中。

建议按以下顺序检查转发链路：

```sh
ubus call qmodem.sms status
ubus call qmodem.sip status
logread | grep 'qmodem_voip sip: outbound REGISTER'
ubus call qmodem.sms delivery_claim
```

出站转发要求 `qmodem.sip` 的 `outbound` 已运行且 `registered` 为 `true`；Asterisk 上
QModem 端点和电话端点的 Contact 也都应为 `Avail`。若日志显示 REGISTER 返回 503 或
`registered=false`，先执行 `/etc/init.d/qmodem_voip_sipd restart`，确认重新注册成功后再
领取并重试待投递记录。`delivery_complete` 只有在 SIP 端返回 2xx 后才应标记成功。

`send_managed` 和 SIP 转发仅适用于数据库模式；直连模式不提供持久化投递、重试或回执追踪。
