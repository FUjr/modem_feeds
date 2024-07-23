# Copyright (C) 2024 Tom <fjrcn@outlook.com>
# This is free software, licensed under the GNU General Public License v3.

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-5modem-mwan-single-module
LUCI_TITLE:=Luci modem mwan support
LUCI_PKGARCH:=all
PKG_VERSION:=2.0
PKG_LICENSE:=GPLv3
PKG_LINCESE_FILES:=LICENSE
PKF_MAINTAINER:=fujr
LUCI_DEPENDS:=+luci-app-5gmodem +luci-app-mwan3
		

define Package/luci-app-5gmodem-mwan/conffiles
/etc/config/modem_mwan
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
