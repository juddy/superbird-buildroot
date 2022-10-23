################################################################################
#
# amlogic cypress wifi driver
#
################################################################################

CYPRESS_WIFI_VERSION = $(call qstrip,$(BR2_PACKAGE_CYPRESS_WIFI_GIT_VERSION))
CYPRESS_WIFI_SITE = $(call qstrip,$(BR2_PACKAGE_CYPRESS_WIFI_GIT_REPO_URL))
CYPRESS_WIFI_SDIO_BUILD_VERSION = $(call qstrip, $(BR2_PACKAGE_CYPRESS_WIFI_SDIO_VERSION))

CYPRESS_WIFI_MODULE_DIR = kernel/cypress/wifi
CYPRESS_WIFI_INSTALL_DIR = $(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/$(CYPRESS_WIFI_MODULE_DIR)

ifeq ($(BR2_PACKAGE_CYPRESS_WIFI_LOCAL),y)
CYPRESS_WIFI_SITE = $(call qstrip,$(BR2_PACKAGE_CYPRESS_WIFI_LOCAL_PATH))
CYPRESS_WIFI_SITE_METHOD = local
endif
ifeq ($(BR2_PACKAGE_CYPRESS_WIFI_STANDALONE),y)
CYPRESS_WIFI_DEPENDENCIES = linux
endif

ifeq ($(BR2_PACKAGE_CYPRESS_WIFI_STANDALONE),y)
define CYPRESS_WIFI_BUILD_CMDS
	$(CYPRESS_WIFI_PCI_DRIVER_BUILD_CMDS)
	$(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(LINUX_DIR) M=$(@D)/$(CYPRESS_WIFI_SDIO_BUILD_VERSION) ARCH=$(KERNEL_ARCH) \
		CROSS_COMPILE=$(TARGET_KERNEL_CROSS) modules
endef
define CYPRESS_WIFI_INSTALL_TARGET_CMDS
	mkdir -p $(CYPRESS_WIFI_INSTALL_DIR)
	$(CYPRESS_WIFI_PCI_DRIVER_INSTALL_CMDS)
	$(INSTALL) -m 0666 $(@D)/$(CYPRESS_WIFI_SDIO_BUILD_VERSION)/cywdhd.ko $(CYPRESS_WIFI_INSTALL_DIR)/cywdhd.ko
	echo $(CYPRESS_WIFI_MODULE_DIR)/cywdhd.ko: >> $(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/modules.dep
endef
endif


$(eval $(generic-package))