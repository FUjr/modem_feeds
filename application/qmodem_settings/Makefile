include $(TOPDIR)/rules.mk
include ../../version.mk

PKG_NAME:=qmodem-settings
PKG_VERSION:=$(QMODEM_VERSION)
PKG_RELEASE:=$(QMODEM_RELEASE)
PKG_LICENSE:=GPL-2.0-or-later

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=QModem modem settings coordinator
  DEPENDS:=+ubus +ubus-at-daemon +jsonfilter +jq
endef

define Package/$(PKG_NAME)/description
  Serializes modem startup settings, workarounds, and URC subscriptions.
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d
	$(INSTALL_BIN) ./files/usr/sbin/qmodem-settings $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/etc/init.d/qmodem-settings $(1)/etc/init.d/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
