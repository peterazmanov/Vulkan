/*
* Vulkan Example base class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#if defined(__ANDROID__)
#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android_native_app_glue.h>
#include "vulkanandroid.h"
#else 
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32 1
#else 
#define GLFW_EXPOSE_NATIVE_X11 1
#define GLFW_EXPOSE_NATIVE_GLX 1
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <array>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <iomanip>
#include <random>
#include <string>
#include <sstream>
#include <streambuf>
#include <thread>
#include <vector>
#include <initializer_list>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

using glm::vec3;
using glm::vec4;
using glm::mat3;
using glm::mat4;
using glm::quat;

#include <vulkan/vulkan.h>

#include "vulkanTools.h"
#include "vulkanDebug.h"
#include "vulkanShaders.h"

#include "vulkanContext.hpp"
#include "vulkanSwapChain.hpp"
#include "vulkanTextureLoader.hpp"
#include "vulkanMeshLoader.hpp"
#include "vulkanTextOverlay.hpp"

#define GAMEPAD_BUTTON_A 0x1000
#define GAMEPAD_BUTTON_B 0x1001
#define GAMEPAD_BUTTON_X 0x1002
#define GAMEPAD_BUTTON_Y 0x1003
#define GAMEPAD_BUTTON_L1 0x1004
#define GAMEPAD_BUTTON_R1 0x1005
#define GAMEPAD_BUTTON_START 0x1006

#define VERTEX_BUFFER_BIND_ID 0
#define INSTANCE_BUFFER_BIND_ID 1
#define ENABLE_VALIDATION true

namespace vkx {
    class ExampleBase : public Context {
    protected:
        ExampleBase(bool enableValidation);
        ExampleBase() : ExampleBase(false) {};
        ~ExampleBase();

    public:
        void run();
        // Called if the window is resized and some resources have to be recreatesd
        void windowResize();

    private:
        // Set to true when example is created with enabled validation layers
        bool enableValidation = false;
        // Set to true when the debug marker extension is detected
        bool enableDebugMarkers = false;
        // fps timer (one second interval)
        float fpsTimer = 0.0f;
        // Get window title with example name, device, et.
        std::string getWindowTitle();
        // Destination dimensions for resizing the window
        uint32_t destWidth;
        uint32_t destHeight;

    protected:
        // Last frame time, measured using a high performance timer (if available)
        float frameTimer = 1.0f;
        // Frame counter to display fps
        uint32_t frameCounter = 0;
        uint32_t lastFPS = 0;

        // Color buffer format
        vk::Format colorformat = vk::Format::eB8G8R8A8Unorm;
        // Depth buffer format
        // Depth format is selected during Vulkan initialization
        vk::Format depthFormat;
        // Command buffer used for setup
        //vk::CommandBuffer setupCmdBuffer;
        // Command buffer for submitting a post present image barrier
        vk::CommandBuffer postPresentCmdBuffer;
        // Command buffer for submitting a pre present image barrier
        vk::CommandBuffer prePresentCmdBuffer;
        // vk::Pipeline stage flags for the submit info structure
        vk::PipelineStageFlags submitPipelineStages = vk::PipelineStageFlagBits::eBottomOfPipe;
        // Contains command buffers and semaphores to be presented to the queue
        vk::SubmitInfo submitInfo;
        // Command buffers used for rendering
        std::vector<vk::CommandBuffer> drawCmdBuffers;
        // Global render pass for frame buffer writes
        vk::RenderPass renderPass;
        // List of available frame buffers (same as number of swap chain images)
        std::vector<vk::Framebuffer>frameBuffers;
        // Active frame buffer index
        uint32_t currentBuffer = 0;
        // Descriptor set pool
        vk::DescriptorPool descriptorPool;
        // List of shader modules created (stored for cleanup)
        std::vector<vk::ShaderModule> shaderModules;
        // Wraps the swap chain to present images (framebuffers) to the windowing system
        SwapChain swapChain;
        // Synchronization semaphores
        struct {
            // Swap chain image presentation
            vk::Semaphore presentComplete;
            // Command buffer submission and execution
            vk::Semaphore renderComplete;
            // Text overlay submission and execution
            vk::Semaphore textOverlayComplete;
        } semaphores;
        // Simple texture loader
        TextureLoader *textureLoader{ nullptr };
        // Returns the base asset path (for shaders, models, textures) depending on the os
        const std::string getAssetPath();

    protected:

        bool prepared = false;
        uint32_t width = 1280;
        uint32_t height = 720;

        VK_CLEAR_COLOR_TYPE defaultClearColor = clearColor(glm::vec4({ 0.025f, 0.025f, 0.025f, 1.0f }));

        float zoom = 0;

        // Defines a frame rate independent timer value clamped from -1.0...1.0
        // For use in animations, rotations, etc.
        float timer = 0.0f;
        // Multiplier for speeding up (or slowing down) the global timer
        float timerSpeed = 0.25f;

        bool paused = false;

        bool enableTextOverlay = false;
        TextOverlay *textOverlay{ nullptr };

        // Use to adjust mouse rotation speed
        float rotationSpeed = 1.0f;
        // Use to adjust mouse zoom speed
        float zoomSpeed = 1.0f;

        glm::vec3 rotation = glm::vec3();
        glm::vec3 cameraPos = glm::vec3();
        glm::vec2 mousePos;

        std::string title = "Vulkan Example";
        std::string name = "vulkanExample";

        CreateImageResult depthStencil;

        // Gamepad state (only one pad supported)

        struct GamePadState {
            struct Axes {
                float x = 0.0f;
                float y = 0.0f;
                float z = 0.0f;
                float rz = 0.0f;
            } axes;
        } gamePadState;

        // OS specific 
#if defined(__ANDROID__)
        android_app* androidApp;
        // true if application has focused, false if moved to background
        bool focused = false;
#else 
        GLFWwindow* window;
#endif

        // Setup the vulkan instance, enable required extensions and connect to the physical device (GPU)
        void initVulkan(bool enableValidation);

#if defined(__ANDROID__)
        static int32_t handleAppInput(struct android_app* app, AInputEvent* event);
        static void handleAppCommand(android_app* app, int32_t cmd);
#else
        void setupWindow();
#endif

        // A default draw implementation
        void drawCommandBuffers(const std::vector<vk::CommandBuffer>& commandBuffers);
        // A default draw implementation
        virtual void draw();
        // Pure virtual render function (override in derived class)
        virtual void render() = 0;

        // Called when view change occurs
        // Can be overriden in derived class to e.g. update uniform buffers 
        // Containing view dependant matrices
        virtual void viewChanged();

        // Called if a key is pressed
        // Can be overriden in derived class to do custom key handling
        virtual void keyPressed(uint32_t keyCode);

        virtual void mouseMoved(double posx, double posy);

        // Called when the window has been resized
        // Can be overriden in derived class to recreate or rebuild resources attached to the frame buffer / swapchain
        virtual void windowResized();
        // Pure virtual function to be overriden by the dervice class
        // Called in case of an event where e.g. the framebuffer has to be rebuild and thus
        // all command buffers that may reference this
        virtual void buildCommandBuffers();

        // Setup default depth and stencil views
        void setupDepthStencil(const vk::CommandBuffer& setupCmdBuffer);
        // Create framebuffers for all requested swap chain images
        // Can be overriden in derived class to setup a custom framebuffer (e.g. for MSAA)
        virtual void setupFrameBuffer();
        // Setup a default render pass
        // Can be overriden in derived class to setup a custom render pass (e.g. for MSAA)
        virtual void setupRenderPass();

        // Connect and prepare the swap chain
        void initSwapchain();
        // Create swap chain images
        void setupSwapChain(const vk::CommandBuffer& setupCmdBuffer);

        // Check if command buffers are valid (!= VK_NULL_HANDLE)
        bool checkCommandBuffers();
        // Create command buffers for drawing commands
        void createCommandBuffers();
        // Destroy all command buffers and set their handles to VK_NULL_HANDLE
        // May be necessary during runtime if options are toggled 
        void destroyCommandBuffers();

        // Command buffer pool
        vk::CommandPool cmdPool;

        // Creates a new (graphics) command pool object storing command buffers
        void createCommandPool();
        //// Create command buffer for setup commands
        //void createSetupCommandBuffer();
        //// Finalize setup command bufferm submit it to the queue and remove it
        //void flushSetupCommandBuffer();

        // Prepare commonly used Vulkan functions
        virtual void prepare();

        // Load a SPIR-V shader
        vk::PipelineShaderStageCreateInfo loadShader(const std::string& fileName, vk::ShaderStageFlagBits stage);

        vk::PipelineShaderStageCreateInfo loadGlslShader(const std::string& fileName, vk::ShaderStageFlagBits stage);


        // Load a mesh (using ASSIMP) and create vulkan vertex and index buffers with given vertex layout
        vkx::MeshBuffer loadMesh(
            const std::string& filename,
            const vkx::MeshLayout& vertexLayout,
            float scale = 1.0f);

        // Start the main render loop
        void renderLoop();

        // Submit a pre present image barrier to the queue
        // Transforms the (framebuffer) image layout from color attachment to present(khr) for presenting to the swap chain
        void submitPrePresentBarrier(const vk::Image& image);

        // Submit a post present image barrier to the queue
        // Transforms the (framebuffer) image layout back from present(khr) to color attachment layout
        void submitPostPresentBarrier(const vk::Image& image);

        // Prepare a submit info structure containing
        // semaphores and submit buffer info for vkQueueSubmit
        vk::SubmitInfo prepareSubmitInfo(
            const std::vector<vk::CommandBuffer>& commandBuffers,
            vk::PipelineStageFlags *pipelineStages);

        void updateTextOverlay();

        // Called when the text overlay is updating
        // Can be overriden in derived class to add custom text to the overlay
        virtual void getOverlayText(vkx::TextOverlay * textOverlay);

        // Prepare the frame for workload submission
        // - Acquires the next image from the swap chain 
        // - Submits a post present barrier
        // - Sets the default wait and signal semaphores
        void prepareFrame();

        // Submit the frames' workload 
        // - Submits the text overlay (if enabled)
        // - 
        void submitFrame();

        static void KeyboardHandler(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void MouseHandler(GLFWwindow* window, int button, int action, int mods);
        static void MouseMoveHandler(GLFWwindow* window, double posx, double posy);
        static void MouseScrollHandler(GLFWwindow* window, double xoffset, double yoffset);
        static void SizeHandler(GLFWwindow* window, int width, int height);
        static void CloseHandler(GLFWwindow* window);
        static void FramebufferSizeHandler(GLFWwindow* window, int width, int height);
        static void JoystickHandler(int, int);
    };


}

// Boilerplate for running an example
#if defined(__ANDROID__)
#define ENTRY_POINT_START \
        void android_main(android_app* state) { \
            app_dummy(); 

#define ENTRY_POINT_END \
            return 0; \
        }
#else 
#define ENTRY_POINT_START \
        int main(const int argc, const char *argv[]) { 

#define ENTRY_POINT_END \
        }
#endif

#define RUN_EXAMPLE(ExampleType) \
    ENTRY_POINT_START \
        ExampleType* vulkanExample = new ExampleType(); \
        vulkanExample->run(); \
        delete(vulkanExample); \
    ENTRY_POINT_END

using namespace vkx;