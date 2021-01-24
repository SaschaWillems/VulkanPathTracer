/* Copyright (c) 2021, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ScratchBuffer.h"

ScratchBuffer::ScratchBuffer(vks::VulkanDevice* device, VkDeviceSize size)
{
	this->device = device;
	// Buffer and memory
	VkBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = size;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &handle));
	VkMemoryRequirements memoryRequirements{};
	vkGetBufferMemoryRequirements(device->logicalDevice, handle, &memoryRequirements);
	VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
	memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = device->getMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memoryAllocateInfo, nullptr, &memory));
	VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, handle, memory, 0));
	// Buffer device address
	VkBufferDeviceAddressInfoKHR bufferDeviceAddresInfo{};
	bufferDeviceAddresInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	bufferDeviceAddresInfo.buffer = handle;
	deviceAddress = vkGetBufferDeviceAddressKHR(device->logicalDevice, &bufferDeviceAddresInfo);
}

ScratchBuffer::~ScratchBuffer()
{
	if (memory != VK_NULL_HANDLE) {
		vkFreeMemory(device->logicalDevice, memory, nullptr);
	}
	if (handle != VK_NULL_HANDLE) {
		vkDestroyBuffer(device->logicalDevice, handle, nullptr);
	}
}
