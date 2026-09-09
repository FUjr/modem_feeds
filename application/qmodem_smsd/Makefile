include $(TOPDIR)/rules.mk
include ../../version.mk

PKG_NAME:=qmodem-smsd
PKG_VERSION:=$(QMODEM_VERSION)
PKG_RELEASE:=$(QMODEM_RELEASE)
PKG_LICENSE:=GPL-2.0-or-later

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=QModem SMS service
  DEPENDS:=+libubus +libubox +libblobmsg-json +libjson-c +libuci +libsqlite3 +libqmodem-sms +tom_modem +ubus-at-daemon +qmodem-settings
endef

define Package/$(PKG_NAME)/description
  Event-driven QModem SMS service with direct and durable database modes.
endef

define Package/$(PKG_NAME)/conffiles
/etc/config/qmodem_sms
endef

define Build/Prepare
	$(RM) -r $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) $(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS) -I$(STAGING_DIR)/usr/include -Wall -Wextra -Werror -std=gnu11" \
		LDFLAGS="$(TARGET_LDFLAGS) -lubus -lubox -lblobmsg_json -ljson-c -luci -lsqlite3 -lqmodem-sms"
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d $(1)/etc/config $(1)/usr/share/rpcd/acl.d $(1)/etc/qmodem
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/qmodem-smsd $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/etc/init.d/qmodem-smsd $(1)/etc/init.d/
	$(INSTALL_CONF) ./files/etc/config/qmodem_sms $(1)/etc/config/
	$(INSTALL_DATA) ./files/usr/share/rpcd/acl.d/qmodem-sms.json $(1)/usr/share/rpcd/acl.d/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
