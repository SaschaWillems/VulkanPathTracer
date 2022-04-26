/*
* Vulkan hardware accelerated path tracing
*
* Copyright (C) 2021 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

/*
* Some great ressources: 
*	- https://raytracing.github.io/ 
*	- https://blog.demofox.org/ (Casual Shadertoy Path Tracing series)
*/

#include "main.h"

VulkanPathTracer::VulkanPathTracer() : VulkanApplication()
{
	title = "Hardware accelerated path tracing";
	settings.overlay = true;
	camera.rotationSpeed *= 0.25f;
	camera.type = Camera::CameraType::firstperson;
	camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 512.0f);

	// Enabled required extensions
	enabledDeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
	// Required by VK_KHR_acceleration_structure
	enabledDeviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
	// Required for VK_KHR_ray_tracing_pipeline
	enabledDeviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
	// Required by VK_KHR_spirv_1_4
	enabledDeviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

	// Enable required features
	// Anisotropic filtering
	enabledFeatures.samplerAnisotropy = VK_TRUE;
	// Int64
	enabledFeatures.shaderInt64 = VK_TRUE;
	// Buffer device address
	enabledBufferDeviceAddresFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	enabledBufferDeviceAddresFeatures.bufferDeviceAddress = VK_TRUE;
	// Ray tracing pipeline
	enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
	enabledRayTracingPipelineFeatures.pNext = &enabledBufferDeviceAddresFeatures;
	// Acceleration structure
	enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
	enabledAccelerationStructureFeatures.pNext = &enabledRayTracingPipelineFeatures;
	// Descriptor indexing
	enabledDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
	enabledDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
	enabledDescriptorIndexingFeatures.pNext = &enabledAccelerationStructureFeatures;
	deviceCreatepNextChain = &enabledDescriptorIndexingFeatures;
}

VulkanPathTracer::~VulkanPathTracer()
{
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	ubo.destroy();
}

uint64_t VulkanPathTracer::getBufferDeviceAddress(VkBuffer buffer)
{
	VkBufferDeviceAddressInfoKHR bufferDeviceAI{};
	bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	bufferDeviceAI.buffer = buffer;
	return vkGetBufferDeviceAddressKHR(vulkanDevice->logicalDevice, &bufferDeviceAI);
}

auto VulkanPathTracer::createBottomLevelAccelerationInstance(uint32_t index)
{
	VkTransformMatrixKHR transformMatrix = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f };
	AccelerationStructure& blas = bottomLevelAS[index];

	VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
	accelerationDeviceAddressInfo.accelerationStructure = blas.handle;
	auto deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &accelerationDeviceAddressInfo);

	VkAccelerationStructureInstanceKHR blasInstance{};
	blasInstance.transform = transformMatrix;
	blasInstance.instanceCustomIndex = index;
	blasInstance.mask = 0xFF;
	blasInstance.instanceShaderBindingTableRecordOffset = 0;
	blasInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	blasInstance.accelerationStructureReference = deviceAddress;

	return blasInstance;
}

//Create the bottom level acceleration structure contains the scene's actual geometry (vertices, triangles)
void VulkanPathTracer::createBottomLevelAccelerationStructure(vkglTF::Model& model)
{
	VkTransformMatrixKHR transformMatrix = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f };
	vks::Buffer transformMatrixBuffer;
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&transformMatrixBuffer,
		sizeof(VkTransformMatrixKHR),
		&transformMatrix));

	uint32_t numTriangles = static_cast<uint32_t>(model.indices.count) / 3;

	VkAccelerationStructureGeometryKHR accelerationStructureGeometry = vks::initializers::accelerationStructureGeometryKHR();
	accelerationStructureGeometry.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
	accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	accelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	accelerationStructureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	accelerationStructureGeometry.geometry.triangles.vertexData.deviceAddress = getBufferDeviceAddress(model.vertices.buffer);
	accelerationStructureGeometry.geometry.triangles.maxVertex = model.vertices.count;
	accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(vkglTF::Vertex);
	accelerationStructureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
	accelerationStructureGeometry.geometry.triangles.indexData.deviceAddress = getBufferDeviceAddress(model.indices.buffer);
	accelerationStructureGeometry.geometry.triangles.transformData.deviceAddress = getBufferDeviceAddress(transformMatrixBuffer.buffer);
	accelerationStructureGeometry.geometry.triangles.transformData.hostAddress = nullptr;

	// Get size info
	VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo = vks::initializers::accelerationStructureBuildGeometryInfoKHR();
	accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationStructureBuildGeometryInfo.geometryCount = 1;
	accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo = vks::initializers::accelerationStructureBuildSizesInfoKHR();
	vkGetAccelerationStructureBuildSizesKHR(
		device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&accelerationStructureBuildGeometryInfo,
		&numTriangles,
		&accelerationStructureBuildSizesInfo);

	AccelerationStructure blas;
	blas.create(vulkanDevice, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, accelerationStructureBuildSizesInfo);

	// Create a small scratch buffer used during build of the bottom level acceleration structure
	ScratchBuffer scratchBuffer(vulkanDevice, accelerationStructureBuildSizesInfo.buildScratchSize);

	VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo = vks::initializers::accelerationStructureBuildGeometryInfoKHR();
	accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	accelerationBuildGeometryInfo.dstAccelerationStructure = blas.handle;
	accelerationBuildGeometryInfo.geometryCount = 1;
	accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
	accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

	VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
	accelerationStructureBuildRangeInfo.primitiveCount = numTriangles;
	accelerationStructureBuildRangeInfo.primitiveOffset = 0;
	accelerationStructureBuildRangeInfo.firstVertex = 0;
	accelerationStructureBuildRangeInfo.transformOffset = 0;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

	if (accelerationStructureFeatures.accelerationStructureHostCommands)
	{
		// Implementation supports building acceleration structure building on host
		vkBuildAccelerationStructuresKHR(
			device,
			VK_NULL_HANDLE,
			1,
			&accelerationBuildGeometryInfo,
			accelerationBuildStructureRangeInfos.data());
	}
	else
	{
		// Acceleration structure needs to be build on the device
		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdBuildAccelerationStructuresKHR(
			commandBuffer,
			1,
			&accelerationBuildGeometryInfo,
			accelerationBuildStructureRangeInfos.data());
		vulkanDevice->flushCommandBuffer(commandBuffer, queue);
	}

	bottomLevelAS.push_back(std::move(blas));
}

// The top level acceleration structure contains the scene's object instances
void VulkanPathTracer::createTopLevelAccelerationStructure()
{
	VkTransformMatrixKHR transformMatrix = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f };

	std::vector<VkAccelerationStructureInstanceKHR> blasInstances{};
	for (uint32_t i = 0; i < bottomLevelAS.size(); i++) {
		blasInstances.push_back(createBottomLevelAccelerationInstance(i));
	}

	// Buffer for instance data
	vks::Buffer instancesBuffer;
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&instancesBuffer,
		sizeof(VkAccelerationStructureInstanceKHR) * blasInstances.size(),
		blasInstances.data()));

	VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
	instanceDataDeviceAddress.deviceAddress = getBufferDeviceAddress(instancesBuffer.buffer);

	VkAccelerationStructureGeometryKHR accelerationStructureGeometry = vks::initializers::accelerationStructureGeometryKHR();
	accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	accelerationStructureGeometry.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
	accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
	accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

	// Get size info
	VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo = vks::initializers::accelerationStructureBuildGeometryInfoKHR();
	accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationStructureBuildGeometryInfo.geometryCount = 1;
	accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

	uint32_t primitive_count = static_cast<uint32_t>(blasInstances.size());

	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo = vks::initializers::accelerationStructureBuildSizesInfoKHR();
	vkGetAccelerationStructureBuildSizesKHR(
		device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&accelerationStructureBuildGeometryInfo,
		&primitive_count,
		&accelerationStructureBuildSizesInfo);

	topLevelAS.create(vulkanDevice, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, accelerationStructureBuildSizesInfo);

	// Create a small scratch buffer used during build of the top level acceleration structure
	ScratchBuffer scratchBuffer(vulkanDevice, accelerationStructureBuildSizesInfo.buildScratchSize);

	VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo = vks::initializers::accelerationStructureBuildGeometryInfoKHR();
	accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	accelerationBuildGeometryInfo.dstAccelerationStructure = topLevelAS.handle;
	accelerationBuildGeometryInfo.geometryCount = 1;
	accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
	accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

	VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
	accelerationStructureBuildRangeInfo.primitiveCount = primitive_count;
	accelerationStructureBuildRangeInfo.primitiveOffset = 0;
	accelerationStructureBuildRangeInfo.firstVertex = 0;
	accelerationStructureBuildRangeInfo.transformOffset = 0;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

	if (accelerationStructureFeatures.accelerationStructureHostCommands)
	{
		// Implementation supports building acceleration structure building on host
		vkBuildAccelerationStructuresKHR(
			device,
			VK_NULL_HANDLE,
			1,
			&accelerationBuildGeometryInfo,
			accelerationBuildStructureRangeInfos.data());
	}
	else
	{
		// Acceleration structure needs to be build on the device
		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdBuildAccelerationStructuresKHR(
			commandBuffer,
			1,
			&accelerationBuildGeometryInfo,
			accelerationBuildStructureRangeInfos.data());
		vulkanDevice->flushCommandBuffer(commandBuffer, queue);
	}

	instancesBuffer.destroy();
}

// Create the Shader Binding Tables that binds the programs and top-level acceleration structure
// SBT Layout:
// 0: raygen
// 1: miss
// 2: hit (closest + any)
void VulkanPathTracer::createShaderBindingTables() {
	const uint32_t handleSize = rayTracingPipelineProperties.shaderGroupHandleSize;
	const uint32_t handleSizeAligned = vks::tools::alignedSize(rayTracingPipelineProperties.shaderGroupHandleSize, rayTracingPipelineProperties.shaderGroupHandleAlignment);
	const uint32_t groupCount = static_cast<uint32_t>(shaderGroups.size());
	const uint32_t sbtSize = groupCount * handleSizeAligned;

	std::vector<uint8_t> shaderHandleStorage(sbtSize);
	VK_CHECK_RESULT(vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()));

	shaderBindingTables.raygen.create(vulkanDevice, 1, rayTracingPipelineProperties);
	shaderBindingTables.miss.create(vulkanDevice, 1, rayTracingPipelineProperties);
	shaderBindingTables.hit.create(vulkanDevice, 1, rayTracingPipelineProperties);

	// Copy handles
	memcpy(shaderBindingTables.raygen.mapped, shaderHandleStorage.data(), handleSize);
	memcpy(shaderBindingTables.miss.mapped, shaderHandleStorage.data() + handleSizeAligned, handleSize);
	memcpy(shaderBindingTables.hit.mapped, shaderHandleStorage.data() + handleSizeAligned * 2, handleSize);
}

// Create the descriptor sets used for the ray tracing dispatch
void VulkanPathTracer::createDescriptorSets()
{
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 }
	};
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &scene.descriptorSet));

	VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo = vks::initializers::writeDescriptorSetAccelerationStructureKHR();
	descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
	descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelAS.handle;

	VkWriteDescriptorSet accelerationStructureWrite{};
	accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	// The specialized acceleration structure descriptor has to be chained
	accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
	accelerationStructureWrite.dstSet = scene.descriptorSet;
	accelerationStructureWrite.dstBinding = 0;
	accelerationStructureWrite.descriptorCount = 1;
	accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	VkDescriptorImageInfo storageImageDescriptor{ VK_NULL_HANDLE, storageImage.view, VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorImageInfo accumImageDescriptor{ VK_NULL_HANDLE, accumulationImage.view, VK_IMAGE_LAYOUT_GENERAL };

	std::vector<VkDescriptorBufferInfo> vBufferInfos(models.size());
	std::vector<VkDescriptorBufferInfo> iBufferInfos(models.size());
	for (auto& model : models) {
		vBufferInfos.push_back({ model.vertices.buffer, 0, VK_WHOLE_SIZE });
		iBufferInfos.push_back({ model.indices.buffer, 0, VK_WHOLE_SIZE });
	}

	// Binding points:
	// 0: Top level acceleration structure
	// 1: Ray tracing result image
	// 2: Ray tracing accumulation image
	// 3: Uniform data
	// 4: Scene descriptors with buffer device addresses

	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		accelerationStructureWrite,
		vks::initializers::writeDescriptorSet(scene.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImageDescriptor),
		vks::initializers::writeDescriptorSet(scene.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2, &accumImageDescriptor),
		vks::initializers::writeDescriptorSet(scene.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3, &ubo.descriptor),
		vks::initializers::writeDescriptorSet(scene.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, &sceneDescBuffer.descriptor),
	};

	std::vector<VkDescriptorImageInfo> imageInfos{};
	if (models[0].textures.size() > 0) {
		for (auto& model : models) {
			for (size_t i = 0; i < model.textures.size(); i++) {
				imageInfos.push_back(model.textures[i].descriptor);
			}
			writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(scene.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, imageInfos.data(), static_cast<uint32_t>(imageInfos.size())));
		}
	}
	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
}

// Create our ray tracing pipeline
void VulkanPathTracer::createRayTracingPipeline()
{
	// Binding points:
	// 0: Top level acceleration structure
	// 1: Ray tracing result image
	// 2: Ray tracing accumulation image
	// 3: Uniform data
	// 4: Scene descriptors with buffer device addresses
	// 5: Scene textures (optional)

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		vks::initializers::descriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR),
		vks::initializers::descriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR),
		vks::initializers::descriptorSetLayoutBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR),
		vks::initializers::descriptorSetLayoutBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR),
		vks::initializers::descriptorSetLayoutBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, static_cast<uint32_t>(models.size())),
	};
	if (models[0].textures.size() > 0) {
		uint32_t texCount{ 0 };
		for (auto& model : models) {
			texCount += static_cast<uint32_t>(model.textures.size());
		}
		setLayoutBindings.push_back(vks::initializers::descriptorSetLayoutBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, texCount));
	}
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

	VkPipelineLayoutCreateInfo pPipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCI, nullptr, &pipelineLayout));

	// Setup ray tracing shader groups
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	// Ray generation group
	{
		shaderStages.push_back(loadShader(getShadersPath() + "raygen.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
		VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
		shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
		shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
		shaderGroups.push_back(shaderGroup);
	}

	// Miss group
	{
		shaderStages.push_back(loadShader(getShadersPath() + "miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
		VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
		shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
		shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
		shaderGroups.push_back(shaderGroup);
	}

	// Closest hit + any hit group
	{
		shaderStages.push_back(loadShader(getShadersPath() + "closesthit.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));
		VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
		shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
		shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
		shaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
		shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
		shaderStages.push_back(loadShader(getShadersPath() + "anyhit.rahit.spv", VK_SHADER_STAGE_ANY_HIT_BIT_KHR));
		shaderGroup.anyHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
		shaderGroups.push_back(shaderGroup);
	}

	VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCI = vks::initializers::rayTracingPipelineCreateInfoKHR();
	rayTracingPipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	rayTracingPipelineCI.pStages = shaderStages.data();
	rayTracingPipelineCI.groupCount = static_cast<uint32_t>(shaderGroups.size());
	rayTracingPipelineCI.pGroups = shaderGroups.data();
	rayTracingPipelineCI.maxPipelineRayRecursionDepth = 1;
	rayTracingPipelineCI.layout = pipelineLayout;
	VK_CHECK_RESULT(vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &pipeline));
}

// Create and fill a buffer for passing the glTF materials to the shaders
void VulkanPathTracer::createMaterialBuffer()
{
	uint32_t textureOffset{ 0 };
	std::vector<Material> materials;
	for (auto& model : models) {
		for (vkglTF::Material mat : model.materials) {
			Material material;
			material.baseColor = mat.baseColorFactor;
			material.baseColor = glm::vec4(1.0f);
			material.baseColorTextureIndex = mat.baseColorTexture ? mat.baseColorTexture->index + textureOffset : -1;
			material.normalTextureIndex = mat.normalTexture ? mat.normalTexture->index + textureOffset : -1;
			material.type = MaterialType::Lambertian;
			if (mat.name == "Light") {
				material.type = MaterialType::Light;
			}
			materials.push_back(material);
		}
		textureOffset += model.textures.size();
	}

	const VkDeviceSize bufferSize = sizeof(Material) * materials.size();
	vks::Buffer staging;

	// Stage to VRAM
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging, bufferSize, materials.data()));
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &scene.materialBuffer, bufferSize));
	vulkanDevice->copyBuffer(&staging, &scene.materialBuffer, queue);

	// @todo: destroy staging
}

// Create and fill a uniform buffer for passing camera properties to the shaders
void VulkanPathTracer::createUniformBuffer()
{
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &ubo, sizeof(uniformData), &uniformData));
	VK_CHECK_RESULT(ubo.map());
	updateUniformBuffers();
}

void VulkanPathTracer::createImages()
{
	storageImage.create(vulkanDevice, queue, swapChain.colorFormat, { width, height, 1 });
	accumulationImage.create(vulkanDevice, queue, VK_FORMAT_R32G32B32A32_SFLOAT, { width, height, 1 });
}

// If the window has been resized, we need to recreate the storage image and it's descriptor
void VulkanPathTracer::handleResize()
{
	// Recreate image
	createImages();
	// Update descriptor
	// @todo
	//VkDescriptorImageInfo storageImageDescriptor{ VK_NULL_HANDLE, storageImage.view, VK_IMAGE_LAYOUT_GENERAL };
	//VkWriteDescriptorSet resultImageWrite = vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImageDescriptor);
	//vkUpdateDescriptorSets(device, 1, &resultImageWrite, 0, VK_NULL_HANDLE);
}

// Command buffer generation
void VulkanPathTracer::buildCommandBuffers()
{
	if (resized)
	{
		handleResize();
	}

	VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

	VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	VkClearValue clearValues[2];
	VkViewport viewport;
	VkRect2D scissor;

	for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
	{
		VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

		clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 1.0f } };;
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = frameBuffers[i];
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		VkStridedDeviceAddressRegionKHR emptySbtEntry = {};

		// Dispatch the ray tracing commands
		vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
		vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &scene.descriptorSet, 0, 0);

		vkCmdTraceRaysKHR(
			drawCmdBuffers[i],
			&shaderBindingTables.raygen.stridedDeviceAddressRegion,
			&shaderBindingTables.miss.stridedDeviceAddressRegion,
			&shaderBindingTables.hit.stridedDeviceAddressRegion,
			&emptySbtEntry,
			width,
			height,
			1);

		// Copy ray tracing output to swap chain image
		vks::tools::setImageLayout(drawCmdBuffers[i], swapChain.images[i], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
		vks::tools::setImageLayout(drawCmdBuffers[i], storageImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, subresourceRange);

		VkImageCopy copyRegion{};
		copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		copyRegion.srcOffset = { 0, 0, 0 };
		copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		copyRegion.dstOffset = { 0, 0, 0 };
		copyRegion.extent = { width, height, 1 };
		vkCmdCopyImage(drawCmdBuffers[i], storageImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapChain.images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		vks::tools::setImageLayout(drawCmdBuffers[i], swapChain.images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, subresourceRange);
		vks::tools::setImageLayout(drawCmdBuffers[i], storageImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, subresourceRange);

		if (UIOverlay.visible) {
			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			drawUI(drawCmdBuffers[i]);
			vkCmdEndRenderPass(drawCmdBuffers[i]);
		}

		VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
	}
}

void VulkanPathTracer::updateUniformBuffers()
{
	uniformData.projInverse = glm::inverse(camera.matrices.perspective);
	uniformData.viewInverse = glm::inverse(camera.matrices.view);
	uniformData.vertexSize = sizeof(vkglTF::Vertex);
	uniformData.samplesPerFrame = options.samplesPerFrame;
	uniformData.rayBounces = options.rayBounces;
	uniformData.sky = (options.sky == 1);
	uniformData.skyIntensity = options.skyIntensity;
	memcpy(ubo.mapped, &uniformData, sizeof(uniformData));
}

void VulkanPathTracer::prepare()
{
	VulkanApplication::prepare();

	// Instead of a simple triangle, we'll be loading a more complex scene for this example
	// The shaders are accessing the vertex and index buffers of the scene, so the proper usage flag has to be set on the vertex and index buffers for the scene
	vkglTF::memoryPropertyFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;

	sceneIndex = 3;

	if (sceneIndex == 0) {
		camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
		camera.setTranslation(glm::vec3(0.0f, 1.0f, -3.0f));
		options.sky = false;
		models.resize(1);
		models[0].loadFromFile(getAssetPath() + "models/CornellBox-Original.gltf", vulkanDevice, queue, glTFLoadingFlags);
	}

	if (sceneIndex == 1) {
		camera.setTranslation(glm::vec3(-9.7f, 0.9f, 0.3f));
		camera.setRotation(glm::vec3(4.75f, -90.0f, 0.0f));
		options.skyIntensity = 7.5f;
		models.resize(1);
		models[0].loadFromFile(getAssetPath() + "models/sponza/Sponza.gltf", vulkanDevice, queue, glTFLoadingFlags);
	}

	if (sceneIndex == 2) {
		camera.setRotation(glm::vec3(-7.5f, -27.25f, 0.0f));
		camera.setTranslation(glm::vec3(-3.0f, 3.5f, -17.0f));
		options.skyIntensity = 1.0f;
		models.resize(1);
		models[0].loadFromFile(getAssetPath() + "models/picapica/scene.gltf", vulkanDevice, queue, glTFLoadingFlags);
	}

	if (sceneIndex == 3) {
		camera.setTranslation(glm::vec3(-9.7f, 0.9f, 0.3f));
		camera.setRotation(glm::vec3(4.75f, -90.0f, 0.0f));
		options.skyIntensity = 7.5f;
		models.resize(3);
		models[0].loadFromFile(getAssetPath() + "models/new_sponza/NewSponza_Main_Blender_glTF.gltf", vulkanDevice, queue, glTFLoadingFlags);
		models[1].loadFromFile(getAssetPath() + "models/new_sponza_curtains/NewSponza_Curtains_glTF.gltf", vulkanDevice, queue, glTFLoadingFlags);
		models[2].loadFromFile(getAssetPath() + "models/new_sponza_ivy/NewSponza_IvyGrowth_glTF.gltf", vulkanDevice, queue, glTFLoadingFlags);
	}


	// Get ray tracing related properties and features
	rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	VkPhysicalDeviceProperties2 deviceProperties2{};
	deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProperties2.pNext = &rayTracingPipelineProperties;
	vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

	accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	VkPhysicalDeviceFeatures2 deviceFeatures2{};
	deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures2.pNext = &accelerationStructureFeatures;
	vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

	// Create the acceleration structures used to render the ray traced scene
	for (vkglTF::Model& model : models) {
		createBottomLevelAccelerationStructure(model);
	}
	createTopLevelAccelerationStructure();
	createMaterialBuffer();

	// Create buffer references for the models in the scene
	uint32_t matIndexOffset{ 0 };
	std::vector<SceneModelInfo> sceneModelInfos;
	for (auto& model : models) {
		SceneModelInfo info{};
		info.vertices = getBufferDeviceAddress(model.vertices.buffer);
		info.indices = getBufferDeviceAddress(model.indices.buffer);
		info.materials = getBufferDeviceAddress(scene.materialBuffer.buffer) +(matIndexOffset * sizeof(Material));
		matIndexOffset += static_cast<uint32_t>(model.materials.size());
		sceneModelInfos.emplace_back(info);
	}
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&sceneDescBuffer,
		sizeof(SceneModelInfo) * static_cast<uint32_t>(sceneModelInfos.size()),
		sceneModelInfos.data()))

	createImages();
	createUniformBuffer();
	createRayTracingPipeline();
	createShaderBindingTables();
	createDescriptorSets();
	buildCommandBuffers();
	updateUniformBuffers();

	if (vks::debugmarker::active) {
		vks::debugmarker::setBufferName(device, scene.materialBuffer.buffer, "Material buffer");
	}

	prepared = true;
}

void VulkanPathTracer::resetAccumulation()
{
	accumulationReset = true;
}

void VulkanPathTracer::draw()
{
	if (accumulationReset || !options.accumulate) {
		uniformData.currentSamplesCount = 0;
		accumulationReset = false;
	}
	if (uniformData.currentSamplesCount < options.maxSamples) {
		uniformData.currentSamplesCount += options.samplesPerFrame;
	}

	VulkanApplication::prepareFrame();
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
	VulkanApplication::submitFrame();
		
	updateUniformBuffers();
}

void VulkanPathTracer::render()
{
	if (!prepared)
		return;
	if (camera.updated) {
		resetAccumulation();
	}
	draw();
}

void VulkanPathTracer::OnUpdateUIOverlay(vks::UIOverlay* overlay)
{
	if (overlay->checkBox("Accumulate frames", &options.accumulate)) {
		resetAccumulation();
	}
	if (overlay->sliderInt("Samples per frame", &options.samplesPerFrame, 2, 16)) {
		resetAccumulation();
	}
	if (overlay->sliderInt("Bounces", &options.rayBounces, 0, 32)) {
		resetAccumulation();
	}
	if (overlay->checkBox("Sky", &options.sky)) {
		resetAccumulation();
	}
	if (overlay->sliderFloat("Sky intensity", &options.skyIntensity, 0.1f, 8.0f)) {
		resetAccumulation();
	}
}

// Platform-specific application setup
#if defined(_WIN32)
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)						\
{																									\
	if (application != NULL)																		\
	{																								\
		application->handleMessages(hWnd, uMsg, wParam, lParam);									\
	}																								\
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));												\
}																									\
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)									\
{																									\
	for (int32_t i = 0; i < __argc; i++) { VulkanPathTracer::args.push_back(__argv[i]); };  			\
	application = new VulkanPathTracer();															\
	application->initVulkan();																	\
	application->setupWindow(hInstance, WndProc);													\
	application->prepare();																		\
	application->renderLoop();																	\
	delete(application);																			\
	return 0;																						\
}
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
void android_main(android_app* state)																\
{																									\
	application = new application();															\
	state->userData = application;																\
	state->onAppCmd = VulkanPathTracer::handleAppCommand;												\
	state->onInputEvent = VulkanPathTracer::handleAppInput;											\
	androidApp = state;																				\
	vks::android::getDeviceConfig();																\
	application->renderLoop();																	\
	delete(application);																			\
}
#elif defined(_DIRECT2DISPLAY)
static void handleEvent()                                											\
{																									\
}																									\
int main(const int argc, const char *argv[])													    \
{																									\
	for (size_t i = 0; i < argc; i++) { VulkanPathTracer::args.push_back(argv[i]); };  				\
	application = new VulkanPathTracer();															\
	application->initVulkan();																	\
	application->prepare();																		\
	application->renderLoop();																	\
	delete(application);																			\
	return 0;																						\
}
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
static void handleEvent(const DFBWindowEvent *event)												\
{																									\
	if (application != NULL)																		\
	{																								\
		application->handleEvent(event);															\
	}																								\
}																									\
int main(const int argc, const char *argv[])													    \
{																									\
	for (size_t i = 0; i < argc; i++) { VulkanPathTracer::args.push_back(argv[i]); };  				\
	application = new VulkanPathTracer();															\
	application->initVulkan();																	\
	application->setupWindow();					 												\
	application->prepare();																		\
	application->renderLoop();																	\
	delete(application);																			\
	return 0;																						\
}
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
int main(const int argc, const char *argv[])													    \
{																									\
	for (size_t i = 0; i < argc; i++) { VulkanPathTracer::args.push_back(argv[i]); };  				\
	application = new VulkanPathTracer();															\
	application->initVulkan();																	\
	application->setupWindow();					 												\
	application->prepare();																		\
	application->renderLoop();																	\
	delete(application);																			\
	return 0;																						\
}
#elif defined(VK_USE_PLATFORM_XCB_KHR)
static void handleEvent(const xcb_generic_event_t *event)											\
{																									\
	if (application != NULL)																		\
	{																								\
		application->handleEvent(event);															\
	}																								\
}																									\
int main(const int argc, const char *argv[])													    \
{																									\
	for (size_t i = 0; i < argc; i++) { VulkanPathTracer::args.push_back(argv[i]); };  				\
	application = new VulkanPathTracer();															\
	application->initVulkan();																	\
	application->setupWindow();					 												\
	application->prepare();																		\
	application->renderLoop();																	\
	delete(application);																			\
	return 0;																						\
}
#endif
