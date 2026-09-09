include $(TOPDIR)/rules.mk
include ../../version.mk

PKG_NAME:=libqmodem-sms
PKG_VERSION:=$(QMODEM_VERSION)
PKG_RELEASE:=$(QMODEM_RELEASE)
PKG_LICENSE:=Apache-2.0

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=Shared QModem SMS codec
endef

define Package/$(PKG_NAME)/description
  Shared GSM PDU encoder and decoder used by QModem SMS clients.
endef

define Build/Prepare
	$(RM) -r $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) $(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS) -fPIC -Wall -Wextra -Werror -std=gnu99 -I." \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/qmodem-sms $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/qmodem-sms/pdu.h $(1)/usr/include/qmodem-sms/
	$(CP) $(PKG_BUILD_DIR)/libqmodem-sms.so.1.0.0 $(1)/usr/lib/
	ln -sf libqmodem-sms.so.1.0.0 $(1)/usr/lib/libqmodem-sms.so
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libqmodem-sms.so.1.0.0 $(1)/usr/lib/
	ln -sf libqmodem-sms.so.1.0.0 $(1)/usr/lib/libqmodem-sms.so.1
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
