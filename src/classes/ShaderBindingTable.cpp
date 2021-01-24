/* Copyright (c) 2021, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ShaderBindingTable.h"

void ShaderBindingTable::create(vks::VulkanDevice* device, uint32_t handleCount, VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties)
{
	// Create buffer to hold all shader handles for the SBT
	VK_CHECK_RESULT(device->createBuffer(
		VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		this,
		rayTracingPipelineProperties.shaderGroupHandleSize * handleCount));
	// Get the strided address to be used when dispatching the rays
	VkBufferDeviceAddressInfoKHR bufferDeviceAI{};
	bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	bufferDeviceAI.buffer = buffer;
	const uint32_t handleSizeAligned = vks::tools::alignedSize(rayTracingPipelineProperties.shaderGroupHandleSize, rayTracingPipelineProperties.shaderGroupHandleAlignment);
	stridedDeviceAddressRegion.deviceAddress = vkGetBufferDeviceAddressKHR(device->logicalDevice, &bufferDeviceAI);;
	stridedDeviceAddressRegion.stride = handleSizeAligned;
	stridedDeviceAddressRegion.size = handleCount * handleSizeAligned;
	// Map persistent 
	map();
}

ShaderBindingTable::~ShaderBindingTable()
{
	destroy();
}
