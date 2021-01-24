/* Copyright (c) 2021, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "volk/volk.h"
#include "VulkanDevice.h"
#include "VulkanTools.h"

struct AccelerationStructure {
	VkAccelerationStructureKHR handle;
	uint64_t deviceAddress = 0;
	VkDeviceMemory memory;
	VkBuffer buffer;
	vks::VulkanDevice* device = nullptr;
	void create(vks::VulkanDevice* device, VkAccelerationStructureTypeKHR type, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo);
	~AccelerationStructure();
};