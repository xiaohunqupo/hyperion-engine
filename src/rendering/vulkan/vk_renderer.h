//
// Created by ethan on 2/5/22.
//

#ifndef HYPERION_VK_RENDERER_H
#define HYPERION_VK_RENDERER_H

#include <vector>
#include <set>
#include <optional>
#include <array>

#include <vulkan/vulkan.h>

#include <util/non_owning_ptr.h>
#include "../../system/sdl_system.h"

#include "renderer_device.h"
#include "renderer_swapchain.h"
#include "renderer_shader.h"
#include "renderer_buffer.h"
#include "renderer_pipeline.h"
#include "renderer_descriptor_pool.h"
#include "renderer_frame.h"

#define VK_RENDERER_API_VERSION VK_API_VERSION_1_2

namespace hyperion {

#define DEFAULT_PENDING_FRAMES_COUNT 2


class RendererQueue {
public:
    RendererQueue();
    void GetQueueFromDevice(RendererDevice device, uint32_t queue_family_index, uint32_t queue_index=0);
private:
    VkQueue queue;
};

class VkRenderer {
    static RendererResult CheckValidationLayerSupport(const std::vector<const char *> &requested_layers);

    std::vector<VkPhysicalDevice> EnumeratePhysicalDevices();
    VkPhysicalDevice PickPhysicalDevice(std::vector<VkPhysicalDevice> _devices);

    /* Setup debug mode */
    RendererResult SetupDebug();
    RendererResult SetupDebugMessenger();

    RendererResult AllocatePendingFrames();
    RendererResult CleanupPendingFrames();

    RendererResult CreateCommandPool();
    RendererResult CreateCommandBuffers();
public:
    VkRenderer(SystemSDL &_system, const char *app_name, const char *engine_name);
    RendererResult Initialize(bool load_debug_layers=false);
    void CreateSurface();

    RendererFrame *GetNextFrame();
    void WaitImageReady(RendererFrame *frame);
    void WaitDeviceIdle();

    HYP_FORCE_INLINE RendererFrame *GetCurrentFrame() { return this->current_frame; }
    HYP_FORCE_INLINE const RendererFrame *GetCurrentFrame() const { return this->current_frame; }

    VkResult AcquireNextImage(RendererFrame *frame);
    void     BeginFrame      (RendererFrame *frame);
    void     EndFrame        (RendererFrame *frame);
    void     PresentFrame    (RendererFrame *frame);

    void SetValidationLayers(std::vector<const char *> _layers);
    RendererDevice *GetRendererDevice();
    RendererResult InitializeRendererDevice(VkPhysicalDevice _physical_device = nullptr);
    RendererResult InitializeSwapchain();

    RendererResult AddPipeline(RendererPipeline::Builder &&builder, RendererPipeline **out = nullptr);

    void SetQueueFamilies(std::set<uint32_t> queue_families);
    void SetCurrentWindow(SystemWindow *window);

    SystemWindow *GetCurrentWindow();
    RendererResult Destroy();

    helpers::SingleTimeCommands GetSingleTimeCommands();

    std::vector<const char *> requested_device_extensions;

    uint16_t frames_to_allocate = DEFAULT_PENDING_FRAMES_COUNT;

    const char *app_name;
    const char *engine_name;

    std::vector<std::unique_ptr<RendererPipeline>> pipelines;
    RendererDescriptorPool descriptor_pool;

    uint32_t acquired_frames_index = 0;
    RendererSwapchain *swapchain = nullptr;

    /* Per frame data */
    VkCommandPool command_pool;
    std::vector<VkCommandBuffer> command_buffers;

private:
    SystemWindow *window = nullptr;
    SystemSDL    system;

    VkInstance instance = nullptr;
    VkSurfaceKHR surface = nullptr;

    std::vector<std::unique_ptr<RendererFrame>> pending_frames;
    RendererFrame *current_frame = nullptr;
    int frames_index = 0;

    VkQueue queue_graphics;
    VkQueue queue_present;

    RendererDevice    *device = nullptr;

    std::set<uint32_t> queue_families;
    std::vector<const char *> validation_layers;

#ifndef HYPERION_BUILD_RELEASE
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
};

} // namespace hyperion


#endif //HYPERION_VK_RENDERER_H
