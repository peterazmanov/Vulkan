/*
* Text overlay class for displaying debug information
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <sstream>
#include <iomanip>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "vulkanTools.h"
#include "vulkanDebug.h"

#include "../external/stb/stb_font_consolas_24_latin1.inl"

// Defines for the STB font used
// STB font files can be found at http://nothings.org/stb/font/
#define STB_FONT_NAME stb_font_consolas_24_latin1
#define STB_FONT_WIDTH STB_FONT_consolas_24_latin1_BITMAP_WIDTH
#define STB_FONT_HEIGHT STB_FONT_consolas_24_latin1_BITMAP_HEIGHT 
#define STB_FIRST_CHAR STB_FONT_consolas_24_latin1_FIRST_CHAR
#define STB_NUM_CHARS STB_FONT_consolas_24_latin1_NUM_CHARS

// Max. number of chars the text overlay buffer can hold
#define MAX_CHAR_COUNT 1024

namespace vkx {
    // Mostly self-contained text overlay class
    // todo : comment
    class TextOverlay {
    private:
        Context context;
        vk::Format colorFormat;
        vk::Format depthFormat;

        uint32_t *frameBufferWidth;
        uint32_t *frameBufferHeight;

        CreateImageResult texture;
        CreateBufferResult vertexBuffer;

        vk::DescriptorPool descriptorPool;
        vk::DescriptorSetLayout descriptorSetLayout;
        vk::DescriptorSet descriptorSet;
        vk::PipelineLayout pipelineLayout;
        vk::PipelineCache pipelineCache;
        vk::Pipeline pipeline;
        vk::RenderPass renderPass;
        std::vector<vk::Framebuffer*> frameBuffers;
        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

        // Pointer to mapped vertex buffer
        glm::vec4 *mapped = nullptr;

        stb_fontchar stbFontData[STB_NUM_CHARS];
        uint32_t numLetters;

    public:

        enum TextAlign { alignLeft, alignCenter, alignRight };

        bool visible = true;
        bool invalidated = false;

        std::vector<vk::CommandBuffer> cmdBuffers;

        TextOverlay(
            Context context,
            std::vector<vk::Framebuffer> &framebuffers,
            vk::Format colorFormat,
            vk::Format depthFormat,
            uint32_t *framebufferwidth,
            uint32_t *framebufferheight,
            std::vector<vk::PipelineShaderStageCreateInfo> shaderstages) {
            this->colorFormat = colorFormat;
            this->depthFormat = depthFormat;
            this->context = context;
            this->frameBuffers.resize(framebuffers.size());
            for (uint32_t i = 0; i < framebuffers.size(); i++) {
                this->frameBuffers[i] = &framebuffers[i];
            }

            this->shaderStages = shaderstages;

            this->frameBufferWidth = framebufferwidth;
            this->frameBufferHeight = framebufferheight;
            cmdBuffers.resize(framebuffers.size());
            prepareResources();
            prepareRenderPass();
            preparePipeline();
        }

        ~TextOverlay() {
            // Free up all Vulkan resources requested by the text overlay
            texture.destroy();
            vertexBuffer.destroy();

            context.device.destroyDescriptorSetLayout(descriptorSetLayout);
            context.device.destroyDescriptorPool(descriptorPool);
            context.device.destroyPipelineLayout(pipelineLayout);
            context.device.destroyPipelineCache(pipelineCache);
            context.device.destroyPipeline(pipeline);
            context.device.destroyRenderPass(renderPass);
            context.device.freeCommandBuffers(context.getCommandPool(), cmdBuffers.size(), cmdBuffers.data());
        }

        // Prepare all vulkan resources required to render the font
        // The text overlay uses separate resources for descriptors (pool, sets, layouts), pipelines and command buffers
        void prepareResources() {
            static unsigned char font24pixels[STB_FONT_HEIGHT][STB_FONT_WIDTH];
            STB_FONT_NAME(stbFontData, font24pixels, STB_FONT_HEIGHT);

            // Command buffer

            // Pool
            vk::CommandPoolCreateInfo cmdPoolInfo;
            cmdPoolInfo.queueFamilyIndex = 0; // todo : pass from example base / swap chain
            cmdPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

            vk::CommandBufferAllocateInfo cmdBufAllocateInfo;
            cmdBufAllocateInfo.commandPool = context.getCommandPool();
            cmdBufAllocateInfo.commandBufferCount = frameBuffers.size();
            cmdBuffers = context.device.allocateCommandBuffers(cmdBufAllocateInfo);

            // Vertex buffer
            vk::DeviceSize bufferSize = MAX_CHAR_COUNT * sizeof(glm::vec4);
            vertexBuffer = context.createBuffer(vk::BufferUsageFlagBits::eVertexBuffer, bufferSize);

            // Font texture
            vk::ImageCreateInfo imageInfo;
            imageInfo.imageType = vk::ImageType::e2D;
            imageInfo.format = vk::Format::eR8Unorm;
            imageInfo.extent.width = STB_FONT_WIDTH;
            imageInfo.extent.height = STB_FONT_HEIGHT;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.tiling = vk::ImageTiling::eOptimal;
            imageInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;


            texture = context.createImage(imageInfo, vk::MemoryPropertyFlagBits::eDeviceLocal);

            // Staging
            auto staging = context.createBuffer(vk::BufferUsageFlagBits::eTransferSrc, STB_FONT_WIDTH * STB_FONT_HEIGHT, &font24pixels[0][0]);

            // Copy to image
            context.withPrimaryCommandBuffer([&](const vk::CommandBuffer& copyCmd) {
                // Prepare for transfer
                setImageLayout(copyCmd, texture.image, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
                vk::BufferImageCopy bufferCopyRegion;
                bufferCopyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                bufferCopyRegion.imageSubresource.mipLevel = 0;
                bufferCopyRegion.imageSubresource.layerCount = 1;
                bufferCopyRegion.imageExtent.width = STB_FONT_WIDTH;
                bufferCopyRegion.imageExtent.height = STB_FONT_HEIGHT;
                bufferCopyRegion.imageExtent.depth = 1;
                copyCmd.copyBufferToImage(staging.buffer, texture.image, vk::ImageLayout::eTransferDstOptimal, bufferCopyRegion);
                // Prepare for shader read
                setImageLayout(copyCmd, texture.image, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
            });

            staging.destroy();

            {
                vk::ImageViewCreateInfo imageViewInfo;
                imageViewInfo.image = texture.image;
                imageViewInfo.viewType = vk::ImageViewType::e2D;
                imageViewInfo.format = imageInfo.format;
                imageViewInfo.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB,    vk::ComponentSwizzle::eA };
                imageViewInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

                texture.view = context.device.createImageView(imageViewInfo);

            }

            // vk::Sampler
            {
                vk::SamplerCreateInfo samplerInfo;
                samplerInfo.magFilter = vk::Filter::eLinear;
                samplerInfo.minFilter = vk::Filter::eLinear;
                samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
                samplerInfo.maxLod = 1.0f;
                samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;
                texture.sampler = context.device.createSampler(samplerInfo);
            }

            // Descriptor
            // Font uses a separate descriptor pool
            std::array<vk::DescriptorPoolSize, 1> poolSizes;
            poolSizes[0] = descriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1);

            vk::DescriptorPoolCreateInfo descriptorPoolInfo =
                descriptorPoolCreateInfo(
                    poolSizes.size(),
                    poolSizes.data(),
                    1);

            descriptorPool = context.device.createDescriptorPool(descriptorPoolInfo);

            // Descriptor set layout
            std::array<vk::DescriptorSetLayoutBinding, 1> setLayoutBindings;
            setLayoutBindings[0] = descriptorSetLayoutBinding(vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 0);

            vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutInfo =
                descriptorSetLayoutCreateInfo(
                    setLayoutBindings.data(),
                    setLayoutBindings.size());

            descriptorSetLayout = context.device.createDescriptorSetLayout(descriptorSetLayoutInfo);

            // vk::Pipeline layout
            vk::PipelineLayoutCreateInfo pipelineLayoutInfo =
                pipelineLayoutCreateInfo(
                    &descriptorSetLayout,
                    1);

            pipelineLayout = context.device.createPipelineLayout(pipelineLayoutInfo);

            // Descriptor set
            vk::DescriptorSetAllocateInfo descriptorSetAllocInfo =
                descriptorSetAllocateInfo(
                    descriptorPool,
                    &descriptorSetLayout,
                    1);

            descriptorSet = context.device.allocateDescriptorSets(descriptorSetAllocInfo)[0];

            vk::DescriptorImageInfo texDescriptor =
                descriptorImageInfo(
                    texture.sampler,
                    texture.view,
                    vk::ImageLayout::eGeneral);

            std::array<vk::WriteDescriptorSet, 1> writeDescriptorSets;
            writeDescriptorSets[0] = writeDescriptorSet(descriptorSet, vk::DescriptorType::eCombinedImageSampler, 0, &texDescriptor);
            context.device.updateDescriptorSets(writeDescriptorSets, nullptr);

            // vk::Pipeline cache
            vk::PipelineCacheCreateInfo pipelineCacheCreateInfo;
            pipelineCache = context.device.createPipelineCache(pipelineCacheCreateInfo);
        }

        // Prepare a separate pipeline for the font rendering decoupled from the main application
        void preparePipeline() {
            vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState =
                pipelineInputAssemblyStateCreateInfo(vk::PrimitiveTopology::eTriangleStrip);

            vk::PipelineRasterizationStateCreateInfo rasterizationState =
                pipelineRasterizationStateCreateInfo(
                    vk::PolygonMode::eFill,
                    vk::CullModeFlagBits::eBack,
                    vk::FrontFace::eClockwise);

            // Enable blending
            vk::ColorComponentFlags allFlags(
                vk::ColorComponentFlagBits::eR |
                vk::ColorComponentFlagBits::eG |
                vk::ColorComponentFlagBits::eB |
                vk::ColorComponentFlagBits::eA);

            vk::PipelineColorBlendAttachmentState blendAttachmentState =
                pipelineColorBlendAttachmentState(allFlags, VK_TRUE);

            blendAttachmentState.srcColorBlendFactor = vk::BlendFactor::eOne;
            blendAttachmentState.dstColorBlendFactor = vk::BlendFactor::eOne;
            blendAttachmentState.colorBlendOp = vk::BlendOp::eAdd;
            blendAttachmentState.srcAlphaBlendFactor = vk::BlendFactor::eOne;
            blendAttachmentState.dstAlphaBlendFactor = vk::BlendFactor::eOne;
            blendAttachmentState.alphaBlendOp = vk::BlendOp::eAdd;
            blendAttachmentState.colorWriteMask = allFlags;

            vk::PipelineColorBlendStateCreateInfo colorBlendState =
                pipelineColorBlendStateCreateInfo(
                    1,
                    &blendAttachmentState);

            vk::PipelineDepthStencilStateCreateInfo depthStencilState =
                pipelineDepthStencilStateCreateInfo(
                    VK_FALSE,
                    VK_FALSE,
                    vk::CompareOp::eLessOrEqual);

            vk::PipelineViewportStateCreateInfo viewportState =
                pipelineViewportStateCreateInfo(1, 1);

            vk::PipelineMultisampleStateCreateInfo multisampleState =
                pipelineMultisampleStateCreateInfo(
                    vk::SampleCountFlagBits::e1);

            std::vector<vk::DynamicState> dynamicStateEnables = {
                vk::DynamicState::eViewport,
                vk::DynamicState::eScissor
            };

            vk::PipelineDynamicStateCreateInfo dynamicState =
                pipelineDynamicStateCreateInfo(
                    dynamicStateEnables.data(),
                    dynamicStateEnables.size());

            std::array<vk::VertexInputBindingDescription, 2> vertexBindings = {};
            vertexBindings[0] = vertexInputBindingDescription(0, sizeof(glm::vec4), vk::VertexInputRate::eVertex);
            vertexBindings[1] = vertexInputBindingDescription(1, sizeof(glm::vec4), vk::VertexInputRate::eVertex);

            std::array<vk::VertexInputAttributeDescription, 2> vertexAttribs = {};
            // Position
            vertexAttribs[0] = vertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, 0);
            // UV
            vertexAttribs[1] = vertexInputAttributeDescription(1, 1, vk::Format::eR32G32Sfloat, sizeof(glm::vec2));

            vk::PipelineVertexInputStateCreateInfo inputState;
            inputState.vertexBindingDescriptionCount = vertexBindings.size();
            inputState.pVertexBindingDescriptions = vertexBindings.data();
            inputState.vertexAttributeDescriptionCount = vertexAttribs.size();
            inputState.pVertexAttributeDescriptions = vertexAttribs.data();

            vk::GraphicsPipelineCreateInfo pipelineCreateInfo =
                vkx::pipelineCreateInfo(
                    pipelineLayout,
                    renderPass);

            pipelineCreateInfo.pVertexInputState = &inputState;
            pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
            pipelineCreateInfo.pRasterizationState = &rasterizationState;
            pipelineCreateInfo.pColorBlendState = &colorBlendState;
            pipelineCreateInfo.pMultisampleState = &multisampleState;
            pipelineCreateInfo.pViewportState = &viewportState;
            pipelineCreateInfo.pDepthStencilState = &depthStencilState;
            pipelineCreateInfo.pDynamicState = &dynamicState;
            pipelineCreateInfo.stageCount = shaderStages.size();
            pipelineCreateInfo.pStages = shaderStages.data();

            pipeline = context.device.createGraphicsPipelines(pipelineCache, pipelineCreateInfo, nullptr)[0];
        }

        // Prepare a separate render pass for rendering the text as an overlay
        void prepareRenderPass() {
            vk::AttachmentDescription attachments[2] = {};

            // Color attachment
            attachments[0].format = colorFormat;
            attachments[0].samples = vk::SampleCountFlagBits::e1;
            // Don't clear the framebuffer (like the renderpass from the example does)
            attachments[0].loadOp = vk::AttachmentLoadOp::eLoad;
            attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
            attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            attachments[0].initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
            attachments[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

            // Depth attachment
            attachments[1].format = depthFormat;
            attachments[1].samples = vk::SampleCountFlagBits::e1;
            attachments[1].loadOp = vk::AttachmentLoadOp::eDontCare;
            attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
            attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            attachments[1].initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

            vk::AttachmentReference colorReference;
            colorReference.attachment = 0;
            colorReference.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentReference depthReference;
            depthReference.attachment = 1;
            depthReference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

            vk::SubpassDescription subpass;
            subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorReference;
            subpass.pDepthStencilAttachment = &depthReference;

            vk::RenderPassCreateInfo renderPassInfo;
            renderPassInfo.attachmentCount = 2;
            renderPassInfo.pAttachments = attachments;
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;

            renderPass = context.device.createRenderPass(renderPassInfo);
        }

        // Map buffer 
        void beginTextUpdate() {
            mapped = vertexBuffer.map<glm::vec4>();
            numLetters = 0;
        }

        // Add text to the current buffer
        // todo : drop shadow? color attribute?
        void addText(std::string text, float x, float y, TextAlign align) {
            assert(mapped != nullptr);

            const float charW = 1.5f / *frameBufferWidth;
            const float charH = 1.5f / *frameBufferHeight;

            float fbW = (float)*frameBufferWidth;
            float fbH = (float)*frameBufferHeight;
            x = (x / fbW * 2.0f) - 1.0f;
            y = (y / fbH * 2.0f) - 1.0f;

            // Calculate text width
            float textWidth = 0;
            for (auto letter : text) {
                stb_fontchar *charData = &stbFontData[(uint32_t)letter - STB_FIRST_CHAR];
                textWidth += charData->advance * charW;
            }

            switch (align) {
            case alignRight:
                x -= textWidth;
                break;
            case alignCenter:
                x -= textWidth / 2.0f;
                break;
            case alignLeft:
                break;
            }

            // Generate a uv mapped quad per char in the new text
            for (auto letter : text) {
                stb_fontchar *charData = &stbFontData[(uint32_t)letter - STB_FIRST_CHAR];

                mapped->x = (x + (float)charData->x0 * charW);
                mapped->y = (y + (float)charData->y0 * charH);
                mapped->z = charData->s0;
                mapped->w = charData->t0;
                mapped++;

                mapped->x = (x + (float)charData->x1 * charW);
                mapped->y = (y + (float)charData->y0 * charH);
                mapped->z = charData->s1;
                mapped->w = charData->t0;
                mapped++;

                mapped->x = (x + (float)charData->x0 * charW);
                mapped->y = (y + (float)charData->y1 * charH);
                mapped->z = charData->s0;
                mapped->w = charData->t1;
                mapped++;

                mapped->x = (x + (float)charData->x1 * charW);
                mapped->y = (y + (float)charData->y1 * charH);
                mapped->z = charData->s1;
                mapped->w = charData->t1;
                mapped++;

                x += charData->advance * charW;

                numLetters++;
            }
        }

        // Unmap buffer and update command buffers
        void endTextUpdate() {
            vertexBuffer.unmap();
            mapped = nullptr;
            updateCommandBuffers();
        }

        // Needs to be called by the application
        void updateCommandBuffers() {
            vk::CommandBufferBeginInfo cmdBufInfo;

            vk::ClearValue clearValues[1];
            clearValues[0].color = clearColor({ 0.0f, 0.0f, 0.0f, 0.0f });

            vk::RenderPassBeginInfo renderPassBeginInfo;
            renderPassBeginInfo.renderPass = renderPass;
            renderPassBeginInfo.renderArea.extent.width = *frameBufferWidth;
            renderPassBeginInfo.renderArea.extent.height = *frameBufferHeight;
            renderPassBeginInfo.clearValueCount = 1;
            renderPassBeginInfo.pClearValues = clearValues;

            for (int32_t i = 0; i < cmdBuffers.size(); ++i) {
                renderPassBeginInfo.framebuffer = *frameBuffers[i];

                cmdBuffers[i].begin(cmdBufInfo);
                {
                    debug::marker::Marker(cmdBuffers[i], "Text overlay", glm::vec4(1.0f, 0.94f, 0.3f, 1.0f));
                    cmdBuffers[i].beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
                    vk::Viewport viewport = vkx::viewport((float)*frameBufferWidth, (float)*frameBufferHeight, 0.0f, 1.0f);
                    cmdBuffers[i].setViewport(0, viewport);
                    vk::Rect2D scissor = vkx::rect2D(*frameBufferWidth, *frameBufferHeight, 0, 0);
                    cmdBuffers[i].setScissor(0, scissor);
                    cmdBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
                    cmdBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, descriptorSet, nullptr);
                    vk::DeviceSize offsets = 0;
                    cmdBuffers[i].bindVertexBuffers(0, vertexBuffer.buffer, offsets);
                    cmdBuffers[i].bindVertexBuffers(1, vertexBuffer.buffer, offsets);
                    for (uint32_t j = 0; j < numLetters; j++) {
                        cmdBuffers[i].draw(4, 1, j * 4, 0);
                    }
                    cmdBuffers[i].endRenderPass();
                }


                cmdBuffers[i].end();
            }
        }

        // Submit the text command buffers to a queue
        void submit(vk::Queue queue, uint32_t bufferindex, vk::SubmitInfo submitInfo) {
            if (!visible) {
                return;
            }

            submitInfo.pCommandBuffers = &cmdBuffers[bufferindex];
            submitInfo.commandBufferCount = 1;

            queue.submit(submitInfo, VK_NULL_HANDLE);
        }

        void reallocateCommandBuffers() {
            context.device.freeCommandBuffers(context.getCommandPool(), cmdBuffers);
            vk::CommandBufferAllocateInfo cmdBufAllocateInfo;
            cmdBufAllocateInfo.commandPool = context.getCommandPool();
            cmdBufAllocateInfo.commandBufferCount = frameBuffers.size();
            cmdBuffers = context.device.allocateCommandBuffers(cmdBufAllocateInfo);
        }
    };
}
