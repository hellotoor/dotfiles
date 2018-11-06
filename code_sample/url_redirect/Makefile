include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=url_redirect
PKG_RELEASE:=1

#PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define KernelPackage/url_redirect
	SUBMENU:=Other modules
	TITLE:=URL redirect 
	AUTOLOAD:=$(call AutoLoad,81,url_redirect)
	FILES:=$(PKG_BUILD_DIR)/url_redirect.ko
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -R ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		ARCH="$(LINUX_KARCH)" \
		SUBDIRS="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="-g $(BUILDFLAGS)" \
		modules
endef

$(eval $(call KernelPackage,url_redirect))
