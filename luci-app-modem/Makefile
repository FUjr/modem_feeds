# Copyright (C) 2023 Siriling <siriling@qq.com>
# This is free software, licensed under the GNU General Public License v3.

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-modem
LUCI_TITLE:=LuCI support for Modem
LUCI_PKGARCH:=all
PKG_VERSION:=1.4.2
PKG_LICENSE:=GPLv3
PKG_LINCESE_FILES:=LICENSE
PKF_MAINTAINER:=siriling <siriling@qq.com>
LUCI_DEPENDS:=+luci-compat +kmod-usb-net +kmod-usb-net-cdc-ether +kmod-usb-acm \
		 +kmod-usb-net-rndis \
		+kmod-usb-net-sierrawireless +kmod-usb-ohci\
		+kmod-usb-serial-option +kmod-usb-wdm \
		+kmod-usb2 +kmod-usb3 \
		+kmod-usb-net-cdc-mbim \
		+usbutils \
		+kmod-pcie_mhi \
		+pciutils \
		+quectel-CM-5G \
		+modemmanager \
		+luci-proto-modemmanager \
		+sms-tool \
		+jq +grep +bc\

define Package/luci-app-modem/conffiles
/etc/config/modem
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
