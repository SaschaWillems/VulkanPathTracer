/* Copyright (c) 2021, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "AccelerationStructure.h"

void AccelerationStructure::create(vks::VulkanDevice* device, VkAccelerationStructureTypeKHR type, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo)
{
	this->device = device;
	// Buffer and memory
	VkBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = buildSizeInfo.accelerationStructureSize;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &buffer));
	VkMemoryRequirements memoryRequirements{};
	vkGetBufferMemoryRequirements(device->logicalDevice, buffer, &memoryRequirements);
	VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
	memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
	VkMemoryAllocateInfo memoryAllocateInfo{};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = device->getMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memoryAllocateInfo, nullptr, &memory));
	VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, buffer, memory, 0));
	// Acceleration structure
	VkAccelerationStructureCreateInfoKHR accelerationStructureCreate_info{};
	accelerationStructureCreate_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	accelerationStructureCreate_info.buffer = buffer;
	accelerationStructureCreate_info.size = buildSizeInfo.accelerationStructureSize;
	accelerationStructureCreate_info.type = type;
	vkCreateAccelerationStructureKHR(device->logicalDevice, &accelerationStructureCreate_info, nullptr, &handle);
	// AS device address
	VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
	accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	accelerationDeviceAddressInfo.accelerationStructure = handle;
	deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device->logicalDevice, &accelerationDeviceAddressInfo);
}

AccelerationStructure::~AccelerationStructure()
{
	// @todo
	//vkFreeMemory(device->logicalDevice, memory, nullptr);
	//vkDestroyBuffer(device->logicalDevice, buffer, nullptr);
	//vkDestroyAccelerationStructureKHR(device->logicalDevice, handle, nullptr);
}
