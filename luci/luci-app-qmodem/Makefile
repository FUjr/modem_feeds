# Copyright (C) 2024 Tom <fjrcn@outlook.com>
# This is free software, licensed under the GNU General Public License v3.

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-qmodem
LUCI_TITLE:=LuCI support for QWRT Modem
LUCI_PKGARCH:=all
PKG_VERSION:=2.4.3
PKG_LICENSE:=GPLv3
PKG_LINCESE_FILES:=LICENSE
PKG_MAINTAINER:=Tom <fjrcn@outlook.com>
LUCI_DEPENDS:=+luci-compat \
		 +kmod-usb-net-rndis \
		+kmod-usb-serial-option +kmod-usb-wdm \
		+kmod-usb2 +kmod-usb3 \
		+kmod-usb-serial-option +kmod-usb-serial +kmod-usb-serial-qualcomm \
		+kmod-usb-net +kmod-usb-acm \
		+kmod-usb-wdm +kmod-usb-net-qmi-wwan \
		+kmod-usb-net-cdc-ether \
		+kmod-usb-net-cdc-mbim \
		+kmod-usb-net-rndis \
		+kmod-usb-net-cdc-ncm +kmod-usb-net-huawei-cdc-ncm \
		+usbutils \
		+PACKAGE_luci-app-qmodem_INCLUDE_ADD_PCI_MHI:kmod-pcie_mhi \
		+PACKAGE_luci-app-qmodem_INCLUDE_ADD_PCI_MHI:pciutils \
		+PACKAGE_luci-app-qmodem_USE_TOM_CUSTOMIZED_QUECTEL_CM:quectel-CM-5G-M \
		+PACKAGE_luci-app-qmodem_USING_QWRT_QUECTEL_CM_5G:quectel-CM-5G \
		+PACKAGE_luci-app-qmodem_USING_NORMAL_QUECTEL_CM_5G:quectel-cm \
		+tom_modem \
		+jq +bc\
		+coreutils +coreutils-stat
		

define Package/luci-app-qmodem/conffiles
/etc/config/qmodem
endef

define Package/luci-app-qmodem/config
	config PACKAGE_luci-app-qmodem_with_lua_index_page
		bool "Include Lua Index Page"
		default n
	
	config PACKAGE_luci-app-qmodem_INCLUDE_ADD_PCI_SUPPORT
		bool "Add PCI SUPPORT"
		default n

	config PACKAGE_luci-app-qmodem_USE_TOM_CUSTOMIZED_QUECTEL_CM
		bool "Use Tom customized Quectel CM"
		default n

	config PACKAGE_luci-app-qmodem_USING_QWRT_QUECTEL_CM_5G
		bool "Using QWRT quectel-CM-5G"
		default y if Package_quectel-CM-5G
	
	config PACKAGE_luci-app-qmodem_USING_NORMAL_QUECTEL_CM
		bool "Using Normal quectel-cm"
		depends on !PACKAGE_luci-app-qmodem_USE_TOM_CUSTOMIZED_QUECTEL_CM
		default y if Package_quectel-cm
endef

define Package/luci-app-qmodem_with_lua_index_page
	TITLE:= QWRT Modem with Lua Luci Index Page
endef

define Package/luci-app-qmodem_with_lua_index_page/install
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view/admin_status/index
	$(INSTALL_DATA) ./files/modem_overview.htm $(1)/usr/lib/lua/luci/view/admin_status/index
endef

include $(TOPDIR)/feeds/luci/luci.mk
$(eval $(call BuildPackage,luci-app-qmodem_with_lua_index_page))
# call BuildPackage - OpenWrt buildroot signature
