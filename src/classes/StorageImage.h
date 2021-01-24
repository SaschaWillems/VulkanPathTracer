/* Copyright (c) 2021, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "volk/volk.h"
#include "VulkanDevice.h"
#include "VulkanTools.h"

struct StorageImage {
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImage image = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkFormat format;
	vks::VulkanDevice* device = nullptr;
	void create(vks::VulkanDevice *device, VkQueue queue, VkFormat format, VkExtent3D extent);
	~StorageImage();
};