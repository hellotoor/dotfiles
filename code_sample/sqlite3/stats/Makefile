include $(TOPDIR)/rules.mk

PKG_NAME:=stats
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/stats
 SECTION:=utils
 CATEGORY:=Utilities
 #URL:=
 DEPENDS:= +libsqlite3
 TITLE:=Ebang Statistic Daemon 
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Configure
	$(CP) ./src/stats.c $(PKG_BUILD_DIR)/
endef

define Build/Compile
	(cd $(PKG_BUILD_DIR); \
	$(TARGET_CC)  stats.c -o stats -g -Wall -I$(STAGING_DIR)/usr/include -L$(STAGING_DIR)/usr/lib -lsqlite3 ;)
endef

define Package/stats/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/stats $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,stats))
