/*
* Assorted commonly used Vulkan helper functions
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <stdint.h>
#include <algorithm>
#include <vulkan/vk_cpp.hpp>
#include <glm/glm.hpp>

// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

#ifdef VK_CPP_HAS_UNRESTRICTED_UNIONS
#define VK_CLEAR_COLOR_TYPE vk::ClearColorValue
#else 
#define VK_CLEAR_COLOR_TYPE VkClearColorValue
#endif

namespace vkx {

    // Check if extension is globally available
    vk::Bool32 checkGlobalExtensionPresent(const char* extensionName);
    // Check if extension is present on the given device
    vk::Bool32 checkDeviceExtensionPresent(vk::PhysicalDevice physicalDevice, const char* extensionName);
    // Selected a suitable supported depth format starting with 32 bit down to 16 bit
    // Returns false if none of the depth formats in the list is supported by the device
     vk::Format getSupportedDepthFormat(vk::PhysicalDevice physicalDevice);

    // Put an image memory barrier for setting an image layout on the sub resource into the given command buffer
    void setImageLayout(
        vk::CommandBuffer cmdbuffer,
        vk::Image image,
        vk::ImageAspectFlags aspectMask,
        vk::ImageLayout oldImageLayout,
        vk::ImageLayout newImageLayout,
        vk::ImageSubresourceRange subresourceRange);
    // Uses a fixed sub resource layout with first mip level and layer
    void setImageLayout(
        vk::CommandBuffer cmdbuffer,
        vk::Image image,
        vk::ImageAspectFlags aspectMask,
        vk::ImageLayout oldImageLayout,
        vk::ImageLayout newImageLayout);

    // Load a text file (e.g. GLGL shader) into a std::string
    std::string readTextFile(const std::string& filename);

    // Load a binary file into a buffer (e.g. SPIR-V)
    std::vector<uint8_t> readBinaryFile(const std::string& filename);

    // Load a SPIR-V shader
#if defined(__ANDROID__)
    vk::ShaderModule loadShader(AAssetManager* assetManager, const char *fileName, vk::Device device, vk::ShaderStageFlagBits stage);
#else
    vk::ShaderModule loadShader(const std::string& filename, vk::Device device, vk::ShaderStageFlagBits stage);
#endif

    // Load a GLSL shader
    // Note : Only for testing purposes, support for directly feeding GLSL shaders into Vulkan
    // may be dropped at some point    
    vk::ShaderModule loadShaderGLSL(const std::string& filename, vk::Device device, vk::ShaderStageFlagBits stage);

    // Returns a pre-present image memory barrier
    // Transforms the image's layout from color attachment to present khr
    vk::ImageMemoryBarrier prePresentBarrier(vk::Image presentImage);

    // Returns a post-present image memory barrier
    // Transforms the image's layout back from present khr to color attachment
    vk::ImageMemoryBarrier postPresentBarrier(vk::Image presentImage);


    struct AllocatedResult {
        vk::Device device;
        vk::DeviceMemory memory;
        size_t allocSize{ 0 };
        void* mapped{ nullptr };

        template <typename T = void>
        inline T* map(size_t offset = 0, size_t size = VK_WHOLE_SIZE) {
            mapped = device.mapMemory(memory, offset, size, vk::MemoryMapFlags());
            return (T*)mapped;
        }

        inline void unmap() {
            device.unmapMemory(memory);
            mapped = nullptr;
        }

        inline void copy(size_t size, const void* data, size_t offset = 0) const {
            memcpy((uint8_t*)mapped + offset, data, size);
        }

        template<typename T>
        inline void copy(const T& data, size_t offset = 0) const {
            copy(sizeof(T), &data, offset);
        }

        template<typename T>
        inline void copy(const std::vector<T>& data, size_t offset = 0) const {
            copy(sizeof(T) * data.size(), data.data(), offset);
        }

        virtual void destroy() {
            if (mapped) {
                unmap();
            }
            if (memory) {
                device.freeMemory(memory);
                memory = vk::DeviceMemory();
            }
        }
    };
    struct CreateImageResult : public AllocatedResult{
        vk::Image image;
        vk::ImageView view;
        vk::Sampler sampler;
        vk::Format format{ vk::Format::eUndefined };
        size_t size{ 0 };

        void destroy() override {
            if (mapped) {
                unmap();
            }
            if (view) {
                device.destroyImageView(view);
                view = vk::ImageView();
            }
            if (image) {
                device.destroyImage(image);
                image = vk::Image();
            }
            AllocatedResult::destroy();
        }
    };

    struct CreateBufferResult : public AllocatedResult{
        vk::Buffer buffer;
        size_t size{ 0 };
        vk::DescriptorBufferInfo descriptor;

        void destroy() override {
            if (mapped) {
                unmap();
            }
            if (buffer) {
                device.destroyBuffer(buffer);
                buffer = vk::Buffer();
            }
            AllocatedResult::destroy();
        }

    };

    // Contains all vulkan objects
    // required for a uniform data object
    using UniformData = vkx::CreateBufferResult;

    // Contains often used vulkan object initializers
    // Save lot of VK_STRUCTURE_TYPE assignments
    // Some initializers are parameterized for convenience
    VK_CLEAR_COLOR_TYPE clearColor(const glm::vec4& v);

    vk::CommandBufferAllocateInfo commandBufferAllocateInfo( vk::CommandPool commandPool, vk::CommandBufferLevel level, uint32_t bufferCount);

    vk::FenceCreateInfo fenceCreateInfo(vk::FenceCreateFlags flags);

    vk::Viewport viewport(
        float width,
        float height,
        float minDepth = 0,
        float maxDepth = 1);

    vk::Rect2D rect2D(
        int32_t width,
        int32_t height,
        int32_t offsetX = 0,
        int32_t offsetY = 0);

    vk::BufferCreateInfo bufferCreateInfo(
        vk::BufferUsageFlags usage,
        vk::DeviceSize size);

    vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo(
        uint32_t poolSizeCount,
        vk::DescriptorPoolSize* pPoolSizes,
        uint32_t maxSets);

    vk::DescriptorPoolSize descriptorPoolSize(
        vk::DescriptorType type,
        uint32_t descriptorCount);

    vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding(
        vk::DescriptorType type,
        vk::ShaderStageFlags stageFlags,
        uint32_t binding);

    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(
        const vk::DescriptorSetLayoutBinding* pBindings,
        uint32_t bindingCount);

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo(
        const vk::DescriptorSetLayout* pSetLayouts,
        uint32_t setLayoutCount);

    vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo(
        vk::DescriptorPool descriptorPool,
        const vk::DescriptorSetLayout* pSetLayouts,
        uint32_t descriptorSetCount);

    vk::DescriptorImageInfo descriptorImageInfo(
        vk::Sampler sampler,
        vk::ImageView imageView,
        vk::ImageLayout imageLayout);

    vk::WriteDescriptorSet writeDescriptorSet(
        vk::DescriptorSet dstSet,
        vk::DescriptorType type,
        uint32_t binding,
        vk::DescriptorBufferInfo* bufferInfo);

    vk::WriteDescriptorSet writeDescriptorSet(
        vk::DescriptorSet dstSet,
        vk::DescriptorType type,
        uint32_t binding,
        vk::DescriptorImageInfo* imageInfo);

    vk::VertexInputBindingDescription vertexInputBindingDescription(
        uint32_t binding,
        uint32_t stride,
        vk::VertexInputRate inputRate);

    vk::VertexInputAttributeDescription vertexInputAttributeDescription(
        uint32_t binding,
        uint32_t location,
            vk::Format format,
        uint32_t offset);

    vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(
        vk::PrimitiveTopology topology,
        vk::PipelineInputAssemblyStateCreateFlags flags = vk::PipelineInputAssemblyStateCreateFlags(),
        vk::Bool32 primitiveRestartEnable = VK_FALSE);

    vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo(
        vk::PolygonMode polygonMode,
        vk::CullModeFlags cullMode,
        vk::FrontFace frontFace,
        vk::PipelineRasterizationStateCreateFlags flags = vk::PipelineRasterizationStateCreateFlags());

    vk::ColorComponentFlags fullColorWriteMask();

    vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState(
        vk::ColorComponentFlags colorWriteMask = fullColorWriteMask(),
        vk::Bool32 blendEnable = VK_FALSE);

    vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo(
        uint32_t attachmentCount,
        const vk::PipelineColorBlendAttachmentState* pAttachments);

    vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo(
        vk::Bool32 depthTestEnable,
        vk::Bool32 depthWriteEnable,
        vk::CompareOp depthCompareOp);

    vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(
        uint32_t viewportCount,
        uint32_t scissorCount,
        vk::PipelineViewportStateCreateFlags flags = vk::PipelineViewportStateCreateFlags());

    vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo(
        vk::SampleCountFlagBits rasterizationSamples,
        vk::PipelineMultisampleStateCreateFlags flags = vk::PipelineMultisampleStateCreateFlags());

    vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(
        const vk::DynamicState *pDynamicStates,
        uint32_t dynamicStateCount,
        vk::PipelineDynamicStateCreateFlags flags = vk::PipelineDynamicStateCreateFlags());

    vk::PipelineTessellationStateCreateInfo pipelineTessellationStateCreateInfo(
        uint32_t patchControlPoints);

    vk::GraphicsPipelineCreateInfo pipelineCreateInfo(
        vk::PipelineLayout layout,
        vk::RenderPass renderPass,
        vk::PipelineCreateFlags flags = vk::PipelineCreateFlags());

    vk::ComputePipelineCreateInfo computePipelineCreateInfo(
        vk::PipelineLayout layout,
        vk::PipelineCreateFlags flags = vk::PipelineCreateFlags());

    vk::PushConstantRange pushConstantRange(
        vk::ShaderStageFlags stageFlags,
        uint32_t size,
        uint32_t offset);
}
