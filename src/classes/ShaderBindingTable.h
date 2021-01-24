/* Copyright (c) 2021, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "volk/volk.h"
#include "VulkanDevice.h"
#include "VulkanTools.h"
#include "VulkanBuffer.h"

class ShaderBindingTable : public vks::Buffer {
public:
	VkStridedDeviceAddressRegionKHR stridedDeviceAddressRegion{};
	void create(vks::VulkanDevice *device, uint32_t handleCount, VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties);
	~ShaderBindingTable();
};