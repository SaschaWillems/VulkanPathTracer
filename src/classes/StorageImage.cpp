/* Copyright (c) 2021, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "StorageImage.h"

StorageImage::~StorageImage()
{
	if (device) {
		vkDestroyImageView(device->logicalDevice, view, nullptr);
		vkDestroyImage(device->logicalDevice, image, nullptr);
		vkFreeMemory(device->logicalDevice, memory, nullptr);
	}
}

void StorageImage::create(vks::VulkanDevice *device, VkQueue queue, VkFormat format, VkExtent3D extent)
{
	this->device = device;

	// Release ressources if image is to be recreated
	if (image != VK_NULL_HANDLE) {
		vkDestroyImageView(device->logicalDevice, view, nullptr);
		vkDestroyImage(device->logicalDevice, image, nullptr);
		vkFreeMemory(device->logicalDevice, memory, nullptr);
		image = VK_NULL_HANDLE;
	}

	VkImageCreateInfo imageInfo = vks::initializers::imageCreateInfo();
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = format;
	imageInfo.extent = extent;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageInfo, nullptr, &image));

	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);
	VkMemoryAllocateInfo memoryAllocateInfo = vks::initializers::memoryAllocateInfo();
	memoryAllocateInfo.allocationSize = memReqs.size;
	memoryAllocateInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memoryAllocateInfo, nullptr, &memory));
	VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, memory, 0));

	VkImageViewCreateInfo colorImageView = vks::initializers::imageViewCreateInfo();
	colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	colorImageView.format = format;
	colorImageView.subresourceRange = {};
	colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorImageView.subresourceRange.baseMipLevel = 0;
	colorImageView.subresourceRange.levelCount = 1;
	colorImageView.subresourceRange.baseArrayLayer = 0;
	colorImageView.subresourceRange.layerCount = 1;
	colorImageView.image = image;
	VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &colorImageView, nullptr, &view));

	VkCommandBuffer cmdBuffer = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	vks::tools::setImageLayout(cmdBuffer, image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
	device->flushCommandBuffer(cmdBuffer, queue);
}