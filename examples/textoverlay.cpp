/*
* Vulkan Example - Text overlay rendering on-top of an existing scene using a separate render pass
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanExampleBase.h"
#include "../external/stb/stb_font_consolas_24_latin1.inl"


// Vertex layout for this example
std::vector<vkx::VertexLayout> vertexLayout =
{
    vkx::VertexLayout::VERTEX_LAYOUT_POSITION,
    vkx::VertexLayout::VERTEX_LAYOUT_NORMAL,
    vkx::VertexLayout::VERTEX_LAYOUT_UV,
    vkx::VertexLayout::VERTEX_LAYOUT_COLOR,
};



class VulkanExample : public vkx::ExampleBase {
public:

    struct {
        vkx::Texture background;
        vkx::Texture cube;
    } textures;

    struct {
        vk::PipelineVertexInputStateCreateInfo inputState;
        std::vector<vk::VertexInputBindingDescription> bindingDescriptions;
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
    } vertices;

    struct {
        vkx::MeshBuffer cube;
    } meshes;

    struct {
        vkx::UniformData vsScene;
    } uniformData;

    struct {
        glm::mat4 projection;
        glm::mat4 model;
        glm::vec4 lightPos = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    } uboVS;

    struct {
        vk::Pipeline solid;
        vk::Pipeline background;
    } pipelines;

    vk::PipelineLayout pipelineLayout;
    vk::DescriptorSetLayout descriptorSetLayout;

    struct {
        vk::DescriptorSet background;
        vk::DescriptorSet cube;
    } descriptorSets;


    VulkanExample() : vkx::ExampleBase(ENABLE_VALIDATION) {
        zoom = -4.5f;
        zoomSpeed = 2.5f;
        rotation = { -25.0f, 0.0f, 0.0f };
        title = "Vulkan Example - Text overlay";
        // Disable text overlay of the example base class
        enableTextOverlay = true;
    }

    ~VulkanExample() {
        device.destroyPipeline(pipelines.solid, nullptr);
        device.destroyPipeline(pipelines.background, nullptr);
        device.destroyPipelineLayout(pipelineLayout, nullptr);
        device.destroyDescriptorSetLayout(descriptorSetLayout, nullptr);
        meshes.cube.destroy();
        textures.background.destroy();
        textures.cube.destroy();
        uniformData.vsScene.destroy();
    }

    void buildCommandBuffers() {
        vk::CommandBufferBeginInfo cmdBufInfo;

        vk::ClearValue clearValues[3];

        clearValues[0].color = vkx::clearColor({ 0.0f, 0.0f, 0.0f, 1.0f });
        clearValues[1].depthStencil = { 1.0f, 0 };

        vk::RenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.renderArea.extent.width = width;
        renderPassBeginInfo.renderArea.extent.height = height;
        renderPassBeginInfo.clearValueCount = 3;
        renderPassBeginInfo.pClearValues = clearValues;

        for (int32_t i = 0; i < drawCmdBuffers.size(); ++i) {
            // Set target frame buffer
            renderPassBeginInfo.framebuffer = frameBuffers[i];

            drawCmdBuffers[i].begin(cmdBufInfo);

            drawCmdBuffers[i].beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

            vk::Viewport viewport = vkx::viewport((float)width, (float)height, 0.0f, 1.0f);
            drawCmdBuffers[i].setViewport(0, viewport);

            vk::Rect2D scissor = vkx::rect2D(width, height, 0, 0);
            drawCmdBuffers[i].setScissor(0, scissor);

            drawCmdBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, descriptorSets.background, nullptr);

            vk::DeviceSize offsets = 0;
            drawCmdBuffers[i].bindVertexBuffers(VERTEX_BUFFER_BIND_ID, meshes.cube.vertices.buffer, offsets);
            drawCmdBuffers[i].bindIndexBuffer(meshes.cube.indices.buffer, 0, vk::IndexType::eUint32);

            // Background
            drawCmdBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.background);
            // Vertices are generated by the vertex shader
            drawCmdBuffers[i].draw(4, 1, 0, 0);

            // Cube
            drawCmdBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.solid);
            drawCmdBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, descriptorSets.cube, nullptr);
            drawCmdBuffers[i].drawIndexed(meshes.cube.indexCount, 1, 0, 0, 0);

            drawCmdBuffers[i].endRenderPass();

            drawCmdBuffers[i].end();
        }

        queue.waitIdle();
    }

    // Update the text buffer displayed by the text overlay
    void updateTextOverlay(void) {
        textOverlay->beginTextUpdate();

        textOverlay->addText(title, 5.0f, 5.0f, TextOverlay::alignLeft);

        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << (frameTimer * 1000.0f) << "ms (" << lastFPS << " fps)";
        textOverlay->addText(ss.str(), 5.0f, 25.0f, TextOverlay::alignLeft);

        textOverlay->addText(deviceProperties.deviceName, 5.0f, 45.0f, TextOverlay::alignLeft);

        textOverlay->addText("Press \"space\" to toggle text overlay", 5.0f, height - 20.0f, TextOverlay::alignLeft);

        // Display projected cube vertices
        for (int32_t x = -1; x <= 1; x += 2) {
            for (int32_t y = -1; y <= 1; y += 2) {
                for (int32_t z = -1; z <= 1; z += 2) {
                    std::stringstream vpos;
                    vpos << std::showpos << x << "/" << y << "/" << z;
                    glm::vec3 projected = glm::project(glm::vec3((float)x, (float)y, (float)z), uboVS.model, uboVS.projection, glm::vec4(0, 0, (float)width, (float)height));
                    textOverlay->addText(vpos.str(), projected.x, projected.y + (y > -1 ? 5.0f : -20.0f), TextOverlay::alignCenter);
                }
            }
        }

        // Display current model view matrix
        textOverlay->addText("model view matrix", width, 5.0f, TextOverlay::alignRight);

        for (uint32_t i = 0; i < 4; i++) {
            ss.str("");
            ss << std::fixed << std::setprecision(2) << std::showpos;
            ss << uboVS.model[0][i] << " " << uboVS.model[1][i] << " " << uboVS.model[2][i] << " " << uboVS.model[3][i];
            textOverlay->addText(ss.str(), width, 25.0f + (float)i * 20.0f, TextOverlay::alignRight);
        }

        glm::vec3 projected = glm::project(glm::vec3(0.0f), uboVS.model, uboVS.projection, glm::vec4(0, 0, (float)width, (float)height));
        textOverlay->addText("Uniform cube", projected.x, projected.y, TextOverlay::alignCenter);

#if defined(__ANDROID__)
        // toto
#else
        textOverlay->addText("Hold middle mouse button and drag to move", 5.0f, height - 40.0f, TextOverlay::alignLeft);
#endif
        textOverlay->endTextUpdate();
    }

    void draw() override {
        prepareFrame();
        std::vector<vk::CommandBuffer> submitCmdBuffers;
        submitCmdBuffers.push_back(drawCmdBuffers[currentBuffer]);
        submitCmdBuffers.push_back(textOverlay->cmdBuffers[currentBuffer]);
        drawCommandBuffers(submitCmdBuffers);
        submitFrame();
    }

    void loadTextures() {
        textures.background = textureLoader->loadTexture(getAssetPath() + "textures/skysphere_bc3.ktx", vk::Format::eBc3UnormBlock);
        textures.cube = textureLoader->loadTexture(getAssetPath() + "textures/round_window_bc3.ktx", vk::Format::eBc3UnormBlock);
    }

    void loadMeshes() {
        meshes.cube = loadMesh(getAssetPath() + "models/cube.dae", vertexLayout, 1.0f);
    }

    void setupVertexDescriptions() {
        // Binding description
        vertices.bindingDescriptions.resize(1);
        vertices.bindingDescriptions[0] =
            vkx::vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, vkx::vertexSize(vertexLayout), vk::VertexInputRate::eVertex);

        // Attribute descriptions
        vertices.attributeDescriptions.resize(4);
        // Location 0 : Position
        vertices.attributeDescriptions[0] =
            vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, vk::Format::eR32G32B32Sfloat, 0);
        // Location 1 : Normal
        vertices.attributeDescriptions[1] =
            vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, vk::Format::eR32G32B32Sfloat, sizeof(float) * 3);
        // Location 2 : Texture coordinates
        vertices.attributeDescriptions[2] =
            vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, vk::Format::eR32G32Sfloat, sizeof(float) * 6);
        // Location 3 : Color
        vertices.attributeDescriptions[3] =
            vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3, vk::Format::eR32G32B32Sfloat, sizeof(float) * 8);

        vertices.inputState = vk::PipelineVertexInputStateCreateInfo();
        vertices.inputState.vertexBindingDescriptionCount = vertices.bindingDescriptions.size();
        vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
        vertices.inputState.vertexAttributeDescriptionCount = vertices.attributeDescriptions.size();
        vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
    }

    void setupDescriptorPool() {
        std::vector<vk::DescriptorPoolSize> poolSizes =
        {
            vkx::descriptorPoolSize(vk::DescriptorType::eUniformBuffer, 2),
            vkx::descriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 2),
        };

        vk::DescriptorPoolCreateInfo descriptorPoolInfo =
            vkx::descriptorPoolCreateInfo(poolSizes.size(), poolSizes.data(), 2);

        descriptorPool = device.createDescriptorPool(descriptorPoolInfo, nullptr);
    }

    void setupDescriptorSetLayout() {
        std::vector<vk::DescriptorSetLayoutBinding> setLayoutBindings =
        {
            // Binding 0 : Vertex shader uniform buffer
            vkx::descriptorSetLayoutBinding(
            vk::DescriptorType::eUniformBuffer,
                vk::ShaderStageFlagBits::eVertex,
                0),
            // Binding 1 : Fragment shader combined sampler
            vkx::descriptorSetLayoutBinding(
                vk::DescriptorType::eCombinedImageSampler,
                vk::ShaderStageFlagBits::eFragment,
                1),
        };

        vk::DescriptorSetLayoutCreateInfo descriptorLayout =
            vkx::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), setLayoutBindings.size());

        descriptorSetLayout = device.createDescriptorSetLayout(descriptorLayout, nullptr);

        vk::PipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
            vkx::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);

        pipelineLayout = device.createPipelineLayout(pPipelineLayoutCreateInfo, nullptr);
    }

    void setupDescriptorSet() {
        vk::DescriptorSetAllocateInfo allocInfo =
            vkx::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);

        // Background
        descriptorSets.background = device.allocateDescriptorSets(allocInfo)[0];

        vk::DescriptorImageInfo texDescriptor =
            vkx::descriptorImageInfo(textures.background.sampler, textures.background.view, vk::ImageLayout::eGeneral);

        std::vector<vk::WriteDescriptorSet> writeDescriptorSets;

        // Binding 0 : Vertex shader uniform buffer
        writeDescriptorSets.push_back(
            vkx::writeDescriptorSet(descriptorSets.background, vk::DescriptorType::eUniformBuffer, 0, &uniformData.vsScene.descriptor));

        // Binding 1 : Color map 
        writeDescriptorSets.push_back(
            vkx::writeDescriptorSet(descriptorSets.background, vk::DescriptorType::eCombinedImageSampler, 1, &texDescriptor));

        device.updateDescriptorSets(writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);

        // Cube
        descriptorSets.cube = device.allocateDescriptorSets(allocInfo)[0];
        texDescriptor.sampler = textures.cube.sampler;
        texDescriptor.imageView = textures.cube.view;
        writeDescriptorSets[0].dstSet = descriptorSets.cube;
        writeDescriptorSets[1].dstSet = descriptorSets.cube;
        device.updateDescriptorSets(writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
    }

    void preparePipelines() {
        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState =
            vkx::pipelineInputAssemblyStateCreateInfo(vk::PrimitiveTopology::eTriangleList, vk::PipelineInputAssemblyStateCreateFlags(), VK_FALSE);

        vk::PipelineRasterizationStateCreateInfo rasterizationState =
            vkx::pipelineRasterizationStateCreateInfo(vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, vk::FrontFace::eClockwise);

        vk::PipelineColorBlendAttachmentState blendAttachmentState =
            vkx::pipelineColorBlendAttachmentState();

        vk::PipelineColorBlendStateCreateInfo colorBlendState =
            vkx::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

        vk::PipelineDepthStencilStateCreateInfo depthStencilState =
            vkx::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, vk::CompareOp::eLessOrEqual);

        vk::PipelineViewportStateCreateInfo viewportState =
            vkx::pipelineViewportStateCreateInfo(1, 1);

        vk::PipelineMultisampleStateCreateInfo multisampleState =
            vkx::pipelineMultisampleStateCreateInfo(vk::SampleCountFlagBits::e1);

        std::vector<vk::DynamicState> dynamicStateEnables = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };
        vk::PipelineDynamicStateCreateInfo dynamicState =
            vkx::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), dynamicStateEnables.size());

        // Wire frame rendering pipeline
        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages;

        shaderStages[0] = loadShader(getAssetPath() + "shaders/textoverlay/mesh.vert.spv", vk::ShaderStageFlagBits::eVertex);
        shaderStages[1] = loadShader(getAssetPath() + "shaders/textoverlay/mesh.frag.spv", vk::ShaderStageFlagBits::eFragment);

        vk::GraphicsPipelineCreateInfo pipelineCreateInfo =
            vkx::pipelineCreateInfo(pipelineLayout, renderPass);

        pipelineCreateInfo.pVertexInputState = &vertices.inputState;
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
        pipelineCreateInfo.pRasterizationState = &rasterizationState;
        pipelineCreateInfo.pColorBlendState = &colorBlendState;
        pipelineCreateInfo.pMultisampleState = &multisampleState;
        pipelineCreateInfo.pViewportState = &viewportState;
        pipelineCreateInfo.pDepthStencilState = &depthStencilState;
        pipelineCreateInfo.pDynamicState = &dynamicState;
        pipelineCreateInfo.stageCount = shaderStages.size();
        pipelineCreateInfo.pStages = shaderStages.data();

        pipelines.solid = device.createGraphicsPipelines(pipelineCache, pipelineCreateInfo, nullptr)[0];

        // Background rendering pipeline
        depthStencilState.depthTestEnable = VK_FALSE;
        depthStencilState.depthWriteEnable = VK_FALSE;

        rasterizationState.polygonMode = vk::PolygonMode::eFill;

        shaderStages[0] = loadShader(getAssetPath() + "shaders/textoverlay/background.vert.spv", vk::ShaderStageFlagBits::eVertex);
        shaderStages[1] = loadShader(getAssetPath() + "shaders/textoverlay/background.frag.spv", vk::ShaderStageFlagBits::eFragment);

        pipelines.background = device.createGraphicsPipelines(pipelineCache, pipelineCreateInfo, nullptr)[0];
    }

    // Prepare and initialize uniform buffer containing shader uniforms
    void prepareUniformBuffers() {
        // Vertex shader uniform buffer block
        uniformData.vsScene = createUniformBuffer(uboVS);
        uniformData.vsScene.map();
        updateUniformBuffers();
    }

    void updateUniformBuffers() {
        // Vertex shader
        uboVS.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 256.0f);
        glm::mat4 viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom));
        uboVS.model = viewMatrix * glm::translate(glm::mat4(), cameraPos);
        uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        uniformData.vsScene.copy(uboVS);
    }

    void prepare() {
        ExampleBase::prepare();
        loadTextures();
        loadMeshes();
        setupVertexDescriptions();
        prepareUniformBuffers();
        setupDescriptorSetLayout();
        preparePipelines();
        setupDescriptorPool();
        setupDescriptorSet();
        buildCommandBuffers();
        prepared = true;
    }

    virtual void render() {
        if (!prepared)
            return;
        draw();

        if (frameCounter == 0) {
            device.waitIdle();
            updateTextOverlay();
        }
    }

    virtual void viewChanged() {
        vkDeviceWaitIdle(device);
        updateUniformBuffers();
        updateTextOverlay();
    }

    virtual void windowResized() {
        updateTextOverlay();
    }

    void keyPressed(uint32_t keyCode) override {
        switch (keyCode) {
        case GLFW_KEY_KP_ADD:
        case GLFW_KEY_SPACE:
            textOverlay->visible = !textOverlay->visible;
        }
    }
};

RUN_EXAMPLE(VulkanExample)
