/*
* Vulkan Example -  Mesh rendering and loading using ASSIMP
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanExampleBase.h"

using namespace vkx;

// Vertex layout used in this example
struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 color;
};

class VulkanExample : public ExampleBase {
public:
    bool wireframe = false;

    struct {
        vkx::Texture colorMap;
    } textures;

    struct {
        vk::PipelineVertexInputStateCreateInfo inputState;
        std::vector<vk::VertexInputBindingDescription> bindingDescriptions;
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
    } vertices;

    // Contains all buffers and information
    // necessary to represent a mesh for rendering purposes
    // This is for demonstration and learning purposes,
    // the other examples use a mesh loader class for easy access
    struct Mesh {
        CreateBufferResult vertices;
        CreateBufferResult indices;
        uint32_t indexCount;
    } mesh;

    struct {
        vkx::UniformData vsScene;
    } uniformData;

    struct {
        glm::mat4 projection;
        glm::mat4 model;
        glm::vec4 lightPos = glm::vec4(25.0f, 5.0f, 5.0f, 1.0f);
    } uboVS;

    struct {
        vk::Pipeline solid;
        vk::Pipeline wireframe;
    } pipelines;

    vk::PipelineLayout pipelineLayout;
    vk::DescriptorSet descriptorSet;
    vk::DescriptorSetLayout descriptorSetLayout;

    VulkanExample() : vkx::ExampleBase(ENABLE_VALIDATION) {
        zoom = -5.5f;
        zoomSpeed = 2.5f;
        rotationSpeed = 0.5f;
        rotation = { -0.5f, -112.75f, 0.0f };
        cameraPos = { 0.1f, 1.1f, 0.0f };
        title = "Vulkan Example - Mesh rendering";
    }

    ~VulkanExample() {
        // Clean up used Vulkan resources 
        // Note : Inherited destructor cleans up resources stored in base class
        device.destroyPipeline(pipelines.solid);
        device.destroyPipelineLayout(pipelineLayout);
        device.destroyDescriptorSetLayout(descriptorSetLayout);

        // Destroy and free mesh resources 
        mesh.vertices.destroy();
        mesh.indices.destroy();
        textures.colorMap.destroy();
        uniformData.vsScene.destroy();
    }

    void reBuildCommandBuffers() {
        if (!checkCommandBuffers()) {
            destroyCommandBuffers();
            createCommandBuffers();
        }
        buildCommandBuffers();
    }

    void buildCommandBuffers() {
        vk::CommandBufferBeginInfo cmdBufInfo;

        vk::ClearValue clearValues[2];
        clearValues[0].color = defaultClearColor;
        clearValues[1].depthStencil = { 1.0f, 0 };

        vk::RenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = width;
        renderPassBeginInfo.renderArea.extent.height = height;
        renderPassBeginInfo.clearValueCount = 2;
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

            drawCmdBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, descriptorSet, nullptr);
            drawCmdBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, wireframe ? pipelines.wireframe : pipelines.solid);

            vk::DeviceSize offsets = 0;
            // Bind mesh vertex buffer
            drawCmdBuffers[i].bindVertexBuffers(VERTEX_BUFFER_BIND_ID, mesh.vertices.buffer, offsets);
            // Bind mesh index buffer
            drawCmdBuffers[i].bindIndexBuffer(mesh.indices.buffer, 0, vk::IndexType::eUint32);
            // Render mesh vertex buffer using it's indices
            drawCmdBuffers[i].drawIndexed(mesh.indexCount, 1, 0, 0, 0);

            drawCmdBuffers[i].endRenderPass();

            drawCmdBuffers[i].end();
        }
    }

    // Load a mesh based on data read via assimp 
    // The other example will use the VulkanMesh loader which has some additional functionality for loading meshes
    void loadMesh() {
        vkx::MeshLoader* meshLoader = new vkx::MeshLoader();
#if defined(__ANDROID__)
        meshLoader->assetManager = androidApp->activity->assetManager;
#endif
        meshLoader->load(getAssetPath() + "models/voyager/voyager.dae");

        // Generate vertex buffer
        float scale = 1.0f;
        std::vector<Vertex> vertexBuffer;
        // Iterate through all meshes in the file
        // and extract the vertex information used in this demo
        for (uint32_t m = 0; m < meshLoader->m_Entries.size(); m++) {
            for (uint32_t i = 0; i < meshLoader->m_Entries[m].Vertices.size(); i++) {
                Vertex vertex;

                vertex.pos = meshLoader->m_Entries[m].Vertices[i].m_pos * scale;
                vertex.normal = meshLoader->m_Entries[m].Vertices[i].m_normal;
                vertex.uv = meshLoader->m_Entries[m].Vertices[i].m_tex;
                vertex.color = meshLoader->m_Entries[m].Vertices[i].m_color;

                vertexBuffer.push_back(vertex);
            }
        }
        uint32_t vertexBufferSize = vertexBuffer.size() * sizeof(Vertex);

        // Generate index buffer from loaded mesh file
        std::vector<uint32_t> indexBuffer;
        for (uint32_t m = 0; m < meshLoader->m_Entries.size(); m++) {
            uint32_t indexBase = indexBuffer.size();
            for (uint32_t i = 0; i < meshLoader->m_Entries[m].Indices.size(); i++) {
                indexBuffer.push_back(meshLoader->m_Entries[m].Indices[i] + indexBase);
            }
        }
        uint32_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
        mesh.indexCount = indexBuffer.size();

        // Static mesh should always be device local
        // Vertex data
        mesh.vertices = stageToDeviceBuffer(vk::BufferUsageFlagBits::eVertexBuffer, vertexBuffer);
        // Index data
        mesh.indices = stageToDeviceBuffer(vk::BufferUsageFlagBits::eIndexBuffer, indexBuffer);

        delete(meshLoader);
    }

    void loadTextures() {
        textures.colorMap = textureLoader->loadTexture(
            getAssetPath() + "models/voyager/voyager.ktx",
             vk::Format::eBc3UnormBlock);
    }

    void setupVertexDescriptions() {
        // Binding description
        vertices.bindingDescriptions.resize(1);
        vertices.bindingDescriptions[0] =
            vkx::vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, sizeof(Vertex), vk::VertexInputRate::eVertex);

        // Attribute descriptions
        // Describes memory layout and shader positions
        vertices.attributeDescriptions.resize(4);
        // Location 0 : Position
        vertices.attributeDescriptions[0] =
            vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0,  vk::Format::eR32G32B32Sfloat, 0);
        // Location 1 : Normal
        vertices.attributeDescriptions[1] =
            vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1,  vk::Format::eR32G32B32Sfloat, sizeof(float) * 3);
        // Location 2 : Texture coordinates
        vertices.attributeDescriptions[2] =
            vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2,  vk::Format::eR32G32Sfloat, sizeof(float) * 6);
        // Location 3 : Color
        vertices.attributeDescriptions[3] =
            vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3,  vk::Format::eR32G32B32Sfloat, sizeof(float) * 8);

        vertices.inputState;
        vertices.inputState.vertexBindingDescriptionCount = vertices.bindingDescriptions.size();
        vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
        vertices.inputState.vertexAttributeDescriptionCount = vertices.attributeDescriptions.size();
        vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
    }

    void setupDescriptorPool() {
        // Example uses one ubo and one combined image sampler
        std::vector<vk::DescriptorPoolSize> poolSizes =
        {
            vkx::descriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
            vkx::descriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1),
        };

        vk::DescriptorPoolCreateInfo descriptorPoolInfo =
            vkx::descriptorPoolCreateInfo(poolSizes.size(), poolSizes.data(), 1);

        descriptorPool = device.createDescriptorPool(descriptorPoolInfo);
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

        descriptorSetLayout = device.createDescriptorSetLayout(descriptorLayout);

        vk::PipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
            vkx::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);

        pipelineLayout = device.createPipelineLayout(pPipelineLayoutCreateInfo);
    }

    void setupDescriptorSet() {
        vk::DescriptorSetAllocateInfo allocInfo =
            vkx::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);

        descriptorSet = device.allocateDescriptorSets(allocInfo)[0];

        vk::DescriptorImageInfo texDescriptor =
            vkx::descriptorImageInfo(textures.colorMap.sampler, textures.colorMap.view, vk::ImageLayout::eGeneral);

        std::vector<vk::WriteDescriptorSet> writeDescriptorSets =
        {
            // Binding 0 : Vertex shader uniform buffer
            vkx::writeDescriptorSet(
            descriptorSet,
                vk::DescriptorType::eUniformBuffer,
                0,
                &uniformData.vsScene.descriptor),
            // Binding 1 : Color map 
            vkx::writeDescriptorSet(
                descriptorSet,
                vk::DescriptorType::eCombinedImageSampler,
                1,
                &texDescriptor)
        };

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

        // Solid rendering pipeline
        // Load shaders
        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages;

        shaderStages[0] = loadShader(getAssetPath() + "shaders/mesh/mesh.vert.spv", vk::ShaderStageFlagBits::eVertex);
        shaderStages[1] = loadShader(getAssetPath() + "shaders/mesh/mesh.frag.spv", vk::ShaderStageFlagBits::eFragment);

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

        // Wire frame rendering pipeline
        rasterizationState.polygonMode = vk::PolygonMode::eLine;
        rasterizationState.lineWidth = 1.0f;

        pipelines.wireframe = device.createGraphicsPipelines(pipelineCache, pipelineCreateInfo, nullptr)[0];
    }

    // Prepare and initialize uniform buffer containing shader uniforms
    void prepareUniformBuffers() {
        // Vertex shader uniform buffer block
        uniformData.vsScene = createUniformBuffer(uboVS);
        updateUniformBuffers();
    }

    void updateUniformBuffers() {
        uboVS.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 256.0f);
        glm::mat4 viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom));

        uboVS.model = viewMatrix * glm::translate(glm::mat4(), cameraPos);
        uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

        void *pData = device.mapMemory(uniformData.vsScene.memory, 0, sizeof(uboVS), vk::MemoryMapFlags());
        memcpy(pData, &uboVS, sizeof(uboVS));
        device.unmapMemory(uniformData.vsScene.memory);
    }

    void prepare() {
        ExampleBase::prepare();
        loadTextures();
        loadMesh();
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
    }

    virtual void viewChanged() {
        vkDeviceWaitIdle(device);
        updateUniformBuffers();
    }

    virtual void keyPressed(uint32_t keyCode) {
        switch (keyCode) {
        case GLFW_KEY_W:
        case GAMEPAD_BUTTON_A:
            wireframe = !wireframe;
            reBuildCommandBuffers();
            break;
        }
    }
};

RUN_EXAMPLE(VulkanExample)
