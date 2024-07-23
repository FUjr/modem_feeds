# Copyright (C) 2024 Tom <fjrcn@outlook.com>
# This is free software, licensed under the GNU General Public License v3.

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-5gmodem-ttl
LUCI_TITLE:=Luci modem ttl support
LUCI_PKGARCH:=all
PKG_VERSION:=2.0
PKG_LICENSE:=GPLv3
PKG_LINCESE_FILES:=LICENSE
PKF_MAINTAINER:=fujr
LUCI_DEPENDS:=+luci-app-5gmodem
		

define Package/luci-app-5gmodem-ttl/conffiles
/etc/config/modem_ttl
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
