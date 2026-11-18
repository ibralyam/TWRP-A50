#
# Copyright (C) 2024 The Android Open Source Project
# Copyright (C) 2024 The TWRP Open Source Project
#
# SPDX-License-Identifier: Apache-2.0
#

# VNDK
PRODUCT_TARGET_VNDK_VERSION := 31

# API
PRODUCT_SHIPPING_API_LEVEL := 28

# No dynamic partitions
PRODUCT_USE_DYNAMIC_PARTITIONS := false

# fastbootd
PRODUCT_PACKAGES += \
    android.hardware.fastboot@1.0-impl-mock \
    fastbootd

PRODUCT_PRODUCT_PROPERTIES += \
	ro.fastbootd.available=true

# Screen
TARGET_SCREEN_WIDTH := 1080
TARGET_SCREEN_HEIGHT := 2340

# Hybrid: marker for ibralyam hybrid TWRP (also set at runtime in init.recovery.exynos9610.rc)
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.ibralyam.twrp.hybrid=1
