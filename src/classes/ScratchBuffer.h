/* Copyright (c) 2021, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "volk/volk.h"
#include "VulkanDevice.h"
#include "VulkanTools.h"

struct ScratchBuffer {
	uint64_t deviceAddress = 0;
	VkBuffer handle = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	vks::VulkanDevice* device = nullptr;
	ScratchBuffer(vks::VulkanDevice* device, VkDeviceSize size);
	~ScratchBuffer();
};