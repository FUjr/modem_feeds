# Copyright (C) 2024 Tom <fjrcn@outlook.com>
# This is free software, licensed under the GNU General Public License v3.

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-5gmodem-hc
LUCI_TITLE:=hc-g50 sim switch
LUCI_PKGARCH:=all
PKG_VERSION:=2.0
PKG_LICENSE:=GPLv3
PKG_LINCESE_FILES:=LICENSE
PKF_MAINTAINER:=fujr
LUCI_DEPENDS:=+luci-app-5gmodem
		

define Package/luci-app-5gmodem-hc/conffiles
/etc/config/modem_sim
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
