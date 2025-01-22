# 增加模组支持文档

## 添加已有Vendor

### 已支持Vendor
- **Quectel**
- **Fibocom**

1. 首先参考其他模组配置，往```/usr/share/qmodem/modem_support.json```添加对应型号
2. modem_name应与 ```/usr/share/qmodem/modem_scan.sh``` 的```get_modem_model()```函数获取的一致，适当时可修改```get_modem_model()```函数
3. 需要屏蔽高级设置中的部分功能时候，可参考 ```modem_ctrl.sh``` 的 ```get_disabled_features``` case,修改根据需要实现这些函数或补充这些函数的配置信息
    - vendor_get_disabled_features
    - get_modem_disabled_features
    - get_global_disabled_features

### 未支持的Vendor
1. 首先参考其他模组配置，往```/usr/share/qmodem/modem_support.json``` 添加对应型号
2. 参考```sierra.sh``` 实现```base_info、sim_info、network_info```，即可完成首页信息展示。其中```SIM Status```项应实现，否则会显示无SIM警告
2. mode/lockband/imei/lockcell/lockact等功能，需要参考 ```/etc/modem/modem_ctrl.sh``` 实现对应功能所调用的case。如无法实现，可参考前面的教程，将不支持的feature屏蔽
