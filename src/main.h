/*
* Vulkan hardware accelerated path tracing
*
* Copyright (C) 2021 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#define VOLK_IMPLEMENTATION
#include "volk/volk.h"

#include "VulkanApplication.h"
#include "VulkanglTFModel.h"
#include "StorageImage.h"
#include "ScratchBuffer.h"
#include "AccelerationStructure.h"
#include "ShaderBindingTable.h"

class VulkanPathTracer : public VulkanApplication
{
public:
	// Available features and properties
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties{};
	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};

	// Enabled features and properties
	VkPhysicalDeviceDescriptorIndexingFeaturesEXT enabledDescriptorIndexingFeatures{};
	VkPhysicalDeviceBufferDeviceAddressFeatures enabledBufferDeviceAddresFeatures{};
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTracingPipelineFeatures{};
	VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccelerationStructureFeatures{};

	std::vector<AccelerationStructure> bottomLevelAS{};
	AccelerationStructure topLevelAS{};

	std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{};
	struct ShaderBindingTables {
		ShaderBindingTable raygen;
		ShaderBindingTable miss;
		ShaderBindingTable hit;
	} shaderBindingTables;

	struct UniformData {
		glm::mat4 viewInverse;
		glm::mat4 projInverse;
		uint32_t vertexSize;
		uint32_t currentSamplesCount = 0;
		uint32_t samplesPerFrame = 4;
		uint32_t rayBounces = 8;
		uint32_t sky = true;
		float skyIntensity = 2.5f;
	} uniformData;
	vks::Buffer ubo;

	struct Options {
		int32_t maxSamples = 64 * 1024;
		int32_t samplesPerFrame = 4;
		int32_t rayBounces = 8;
		bool accumulate = true;
		bool sky = true;
		float skyIntensity = 5.0f;
	} options;

	StorageImage accumulationImage;
	StorageImage storageImage;
	bool accumulationReset = true;

	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;

	enum class MaterialType : uint32_t { 
		Lambertian = 0,
		Light = 1
	};
	struct Material {
		glm::vec4 baseColor;
		int32_t baseColorTextureIndex;
		int32_t normalTextureIndex;
		MaterialType type;
	};

	std::vector<vkglTF::Model> models;

	struct Scene {
		vks::Buffer materialBuffer;
		VkDescriptorSet descriptorSet;
		AccelerationStructure bottomLevelAS{};
		AccelerationStructure topLevelAS{};
	} scene;

	uint32_t sceneIndex = 3;

	struct SceneModelInfo {
		uint64_t vertices;
		uint64_t indices;
		uint64_t materials;
	};
	vks::Buffer sceneDescBuffer;

	VulkanPathTracer();
	~VulkanPathTracer();
	uint64_t getBufferDeviceAddress(VkBuffer buffer);
	auto createBottomLevelAccelerationInstance(uint32_t index);
	void createBottomLevelAccelerationStructure(vkglTF::Model& model);
	void createTopLevelAccelerationStructure();
	void createShaderBindingTables();
	void createDescriptorSets();
	void createRayTracingPipeline();
	void createMaterialBuffer();
	void createUniformBuffer();
	void createImages();
	void handleResize();
	void buildCommandBuffers();
	void updateUniformBuffers();
	void prepare();
	void resetAccumulation();
	void draw();
	virtual void render();
	void OnUpdateUIOverlay(vks::UIOverlay* overlay);
};

VulkanPathTracer* application;