#include <libretro.h>
#include <vulkan_headers.hpp>
#include <libretro_vulkan.h>
// Upstream ares no longer exposes the paraLLEl-RDP headers through
// n64/vulkan/vulkan.hpp, so pull in the Vulkan context declarations directly.
#include <context.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <mia/mia.hpp>
#include <n64/vulkan/vulkan.hpp>

#include "core_options.hpp"

#if defined(dllexport)
#undef dllexport
#endif

namespace nall {
auto main(Arguments) -> void {}
}

namespace ares::Nintendo64 {
auto load(Node::System& node, nall::string name) -> bool;
auto option(nall::string name, nall::string value) -> bool;
}

namespace {

constexpr unsigned MaxPorts = 4;
constexpr double AudioFrequency = 48000.0;

retro_environment_t environmentCallback = nullptr;
retro_video_refresh_t videoCallback = nullptr;
retro_audio_sample_t audioSampleCallback = nullptr;
retro_audio_sample_batch_t audioBatchCallback = nullptr;
retro_input_poll_t inputPollCallback = nullptr;
retro_input_state_t inputStateCallback = nullptr;
retro_log_printf_t logCallback = nullptr;
retro_rumble_interface rumbleInterface = {};
const retro_hw_render_interface_vulkan* vulkanInterface = nullptr;
std::unique_ptr<::Vulkan::Context> vulkanContext;

struct LibretroInstanceFactory final : ::Vulkan::InstanceFactory {
  retro_vulkan_create_instance_wrapper_t callback = nullptr;
  void* opaque = nullptr;

  auto create_instance(const VkInstanceCreateInfo* info) -> VkInstance override {
    return callback ? callback(opaque, info) : VK_NULL_HANDLE;
  }
} vulkanInstanceFactory;

struct LibretroDeviceFactory final : ::Vulkan::DeviceFactory {
  retro_vulkan_create_device_wrapper_t callback = nullptr;
  void* opaque = nullptr;

  auto create_device(
    VkPhysicalDevice gpu,
    const VkDeviceCreateInfo* info
  ) -> VkDevice override {
    return callback ? callback(gpu, opaque, info) : VK_NULL_HANDLE;
  }
} vulkanDeviceFactory;

std::array<unsigned, MaxPorts> portDevices = {
  RETRO_DEVICE_JOYPAD,
  RETRO_DEVICE_JOYPAD,
  RETRO_DEVICE_JOYPAD,
  RETRO_DEVICE_JOYPAD,
};

auto logMessage(enum retro_log_level level, const char* message) -> void {
  if(logCallback) logCallback(level, "%s\n", message);
}

template<typename T>
auto vulkanHandle(u64 value) -> T {
  if constexpr(std::is_pointer_v<T>) {
    return reinterpret_cast<T>(static_cast<uintptr_t>(value));
  } else {
    return static_cast<T>(value);
  }
}

auto lockVulkanQueue() -> void {
  if(vulkanInterface && vulkanInterface->lock_queue) {
    vulkanInterface->lock_queue(vulkanInterface->handle);
  }
}

auto unlockVulkanQueue() -> void {
  if(vulkanInterface && vulkanInterface->unlock_queue) {
    vulkanInterface->unlock_queue(vulkanInterface->handle);
  }
}

auto RETRO_CALLCONV vulkanApplicationInfo() -> const VkApplicationInfo* {
  static const VkApplicationInfo application = {
    VK_STRUCTURE_TYPE_APPLICATION_INFO,
    nullptr,
    "ares N64 libretro",
    1,
    "ares",
    1,
    VK_API_VERSION_1_1,
  };
  return &application;
}

auto RETRO_CALLCONV createVulkanInstance(
  PFN_vkGetInstanceProcAddr getInstanceProcAddress,
  const VkApplicationInfo* application,
  retro_vulkan_create_instance_wrapper_t createInstance,
  void* opaque
) -> VkInstance {
  if(
    !getInstanceProcAddress ||
    !createInstance ||
    !::Vulkan::Context::init_loader(getInstanceProcAddress, true)
  ) return VK_NULL_HANDLE;

  vulkanContext = std::make_unique<::Vulkan::Context>();
  vulkanInstanceFactory.callback = createInstance;
  vulkanInstanceFactory.opaque = opaque;
  vulkanContext->set_instance_factory(&vulkanInstanceFactory);
  vulkanContext->set_application_info(
    application ? application : vulkanApplicationInfo()
  );

  if(!vulkanContext->init_instance(nullptr, 0, 0)) {
    vulkanContext.reset();
    return VK_NULL_HANDLE;
  }

  auto instance = vulkanContext->get_instance();
  vulkanContext->release_instance();
  return instance;
}

auto RETRO_CALLCONV createVulkanDeviceLegacy(
  retro_vulkan_context* output,
  VkInstance instance,
  VkPhysicalDevice gpu,
  VkSurfaceKHR surface,
  PFN_vkGetInstanceProcAddr getInstanceProcAddress,
  const char** requiredDeviceExtensions,
  unsigned requiredDeviceExtensionCount,
  const char** requiredDeviceLayers,
  unsigned requiredDeviceLayerCount,
  const VkPhysicalDeviceFeatures* requiredFeatures
) -> bool {
  (void)requiredDeviceLayers;
  (void)requiredDeviceLayerCount;

  if(
    !output ||
    !instance ||
    !getInstanceProcAddress ||
    !::Vulkan::Context::init_loader(getInstanceProcAddress, true)
  ) return false;

  // Retain the v1 path for frontends that require it during v2 negotiation.
  vulkanContext = std::make_unique<::Vulkan::Context>();
  vulkanContext->set_application_info(vulkanApplicationInfo());
  if(!vulkanContext->init_device_from_instance(
    instance,
    gpu,
    surface,
    requiredDeviceExtensions,
    requiredDeviceExtensionCount,
    requiredFeatures
  )) {
    vulkanContext.reset();
    return false;
  }

  auto& queues = vulkanContext->get_queue_info();
  auto graphicsQueue = queues.queues[::Vulkan::QUEUE_INDEX_GRAPHICS];
  auto graphicsFamily = queues.family_indices[::Vulkan::QUEUE_INDEX_GRAPHICS];
  if(!graphicsQueue || graphicsFamily == VK_QUEUE_FAMILY_IGNORED) {
    vulkanContext.reset();
    return false;
  }

  *output = {};
  output->gpu = vulkanContext->get_gpu();
  output->device = vulkanContext->get_device();
  output->queue = graphicsQueue;
  output->queue_family_index = graphicsFamily;
  output->presentation_queue = graphicsQueue;
  output->presentation_queue_family_index = graphicsFamily;
  vulkanContext->release_device();
  logMessage(RETRO_LOG_INFO, "Vulkan negotiation v1 device created");
  return true;
}

auto RETRO_CALLCONV createVulkanDevice(
  retro_vulkan_context* output,
  VkInstance instance,
  VkPhysicalDevice gpu,
  VkSurfaceKHR surface,
  PFN_vkGetInstanceProcAddr getInstanceProcAddress,
  retro_vulkan_create_device_wrapper_t createDevice,
  void* opaque
) -> bool {
  if(
    !output ||
    !vulkanContext ||
    vulkanContext->get_instance() != instance ||
    !getInstanceProcAddress ||
    !createDevice ||
    !::Vulkan::Context::init_loader(getInstanceProcAddress, true)
  ) return false;

  vulkanDeviceFactory.callback = createDevice;
  vulkanDeviceFactory.opaque = opaque;
  vulkanContext->set_device_factory(&vulkanDeviceFactory);

  if(!vulkanContext->init_device(gpu, surface, nullptr, 0, 0)) return false;

  auto& queues = vulkanContext->get_queue_info();
  auto graphicsQueue = queues.queues[::Vulkan::QUEUE_INDEX_GRAPHICS];
  auto graphicsFamily = queues.family_indices[::Vulkan::QUEUE_INDEX_GRAPHICS];
  if(!graphicsQueue || graphicsFamily == VK_QUEUE_FAMILY_IGNORED) return false;

  *output = {};
  output->gpu = vulkanContext->get_gpu();
  output->device = vulkanContext->get_device();
  output->queue = graphicsQueue;
  output->queue_family_index = graphicsFamily;
  output->presentation_queue = graphicsQueue;
  output->presentation_queue_family_index = graphicsFamily;
  vulkanContext->release_device();
  logMessage(RETRO_LOG_INFO, "Vulkan negotiation v2 device created");
  return true;
}

auto RETRO_CALLCONV destroyVulkanDevice() -> void {
  ares::Nintendo64::clearVulkanExternalContext();
  vulkanInterface = nullptr;
  vulkanContext.reset();
  vulkanInstanceFactory = {};
  vulkanDeviceFactory = {};
}

auto RETRO_CALLCONV resetVulkanContext() -> void {
  if(!environmentCallback || !vulkanContext) return;

  const retro_hw_render_interface* baseInterface = nullptr;
  if(
    !environmentCallback(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE, &baseInterface) ||
    !baseInterface ||
    baseInterface->interface_type != RETRO_HW_RENDER_INTERFACE_VULKAN
  ) {
    logMessage(RETRO_LOG_ERROR, "frontend did not expose the Vulkan render interface");
    return;
  }

  auto candidate =
    reinterpret_cast<const retro_hw_render_interface_vulkan*>(baseInterface);
  bool incompatible = false;
  if(candidate->interface_version < 1) {
    logMessage(RETRO_LOG_ERROR, "Vulkan interface version is below 1");
    incompatible = true;
  }
  if(candidate->device != vulkanContext->get_device()) {
    logMessage(RETRO_LOG_ERROR, "Vulkan interface exposed a different device");
    incompatible = true;
  }
  if(!candidate->set_image) {
    logMessage(RETRO_LOG_ERROR, "Vulkan interface does not provide set_image");
    incompatible = true;
  }
  if(!candidate->get_sync_index) {
    logMessage(RETRO_LOG_ERROR, "Vulkan interface does not provide get_sync_index");
    incompatible = true;
  }
  if(incompatible) {
    logMessage(RETRO_LOG_ERROR, "frontend exposed an incompatible Vulkan interface");
    return;
  }

  vulkanInterface = candidate;
  ares::Nintendo64::setVulkanExternalContext(
    vulkanContext.get(),
    lockVulkanQueue,
    unlockVulkanQueue
  );
  logMessage(RETRO_LOG_INFO, "Vulkan zero-copy video path enabled");
}

auto requestVulkanContext() -> bool {
  if(!environmentCallback) return false;

  retro_hw_render_context_negotiation_interface support = {};
  support.interface_type =
    RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN;
  support.interface_version =
    RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN_VERSION;
  if(
    !environmentCallback(
      RETRO_ENVIRONMENT_GET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_SUPPORT,
      &support
    ) ||
    support.interface_version < 2
  ) return false;

  static const retro_hw_render_context_negotiation_interface_vulkan negotiation = {
    RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN,
    RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN_VERSION,
    vulkanApplicationInfo,
    createVulkanDeviceLegacy,
    // Auxiliary resources are released explicitly during core deinitialization.
    nullptr,
    createVulkanInstance,
    createVulkanDevice,
  };
  if(
    !environmentCallback(
      RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE,
      const_cast<retro_hw_render_context_negotiation_interface_vulkan*>(
        &negotiation
      )
    )
  ) return false;

  retro_hw_render_callback hardware = {};
  hardware.context_type = RETRO_HW_CONTEXT_VULKAN;
  hardware.context_reset = resetVulkanContext;
  hardware.depth = false;
  hardware.stencil = false;
  hardware.bottom_left_origin = false;
  hardware.version_major = 1;
  hardware.version_minor = 1;
  // Use the device returned by the negotiated Vulkan creation callback.
  hardware.cache_context = false;
  hardware.debug_context = false;
  return environmentCallback(RETRO_ENVIRONMENT_SET_HW_RENDER, &hardware);
}

auto optionValue(const char* key, const char* fallback) -> const char* {
  if(!environmentCallback) return fallback;
  retro_variable variable = {key, nullptr};
  if(environmentCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &variable) && variable.value) {
    return variable.value;
  }
  return fallback;
}

auto directoryFromEnvironment(unsigned command) -> std::string {
  if(!environmentCallback) return {};
  const char* path = nullptr;
  if(!environmentCallback(command, &path) || !path || !*path) return {};
  std::string result = path;
  if(result.back() != '/' && result.back() != '\\') result.push_back('/');
  return result;
}

auto nodePort(ares::Node::Object node) -> unsigned {
  auto current = node;
  while(current) {
    auto name = current->name();
    if(name == "Controller Port 1") return 0;
    if(name == "Controller Port 2") return 1;
    if(name == "Controller Port 3") return 2;
    if(name == "Controller Port 4") return 3;
    current = current->parent().lock();
  }
  return MaxPorts;
}

auto pollJoypad(unsigned port, unsigned id) -> bool {
  if(!inputStateCallback || port >= MaxPorts) return false;
  if((portDevices[port] & RETRO_DEVICE_MASK) == RETRO_DEVICE_NONE) return false;
  return inputStateCallback(port, RETRO_DEVICE_JOYPAD, 0, id) != 0;
}

auto pollAnalog(unsigned port, unsigned index, unsigned id) -> int16_t {
  if(!inputStateCallback || port >= MaxPorts) return 0;
  if((portDevices[port] & RETRO_DEVICE_MASK) == RETRO_DEVICE_NONE) return 0;
  return inputStateCallback(port, RETRO_DEVICE_ANALOG, index, id);
}

class LibretroPlatform final : public ares::Platform {
public:
  auto attach(ares::Node::Object node) -> void override {
    if(auto stream = node->cast<ares::Node::Audio::Stream>()) {
      stream->setResamplerFrequency(AudioFrequency);
    }
  }

  auto detach(ares::Node::Object) -> void override {
  }

  auto pak(ares::Node::Object node) -> std::shared_ptr<vfs::directory> override {
    if(node->name() == "Nintendo 64" && system) return system->pak;
    if(node->name() == "Nintendo 64 Cartridge" && game) return game->pak;
    if(node->name() == "Gamepad") {
      auto port = nodePort(node);
      if(port < controllerPaks.size() && controllerPaks[port]) {
        return controllerPaks[port]->pak;
      }
    }
    return {};
  }

  auto log(ares::Node::Debugger::Tracer::Tracer, string_view message) -> void override {
    nall::string text{message};
    logMessage(RETRO_LOG_DEBUG, text.data());
  }

  auto status(string_view message) -> void override {
    nall::string text{message};
    logMessage(RETRO_LOG_INFO, text.data());
  }

  auto video(
    ares::Node::Video::Screen,
    const u32* data,
    u32 pitch,
    u32 width,
    u32 height
  ) -> void override {
    std::lock_guard<std::mutex> lock(frameMutex);
    pendingFrame.resize(static_cast<size_t>(width) * height);
    auto sourcePitch = pitch / sizeof(uint32_t);
    for(uint32_t y = 0; y < height; y++) {
      std::memcpy(
        pendingFrame.data() + static_cast<size_t>(y) * width,
        data + static_cast<size_t>(y) * sourcePitch,
        static_cast<size_t>(width) * sizeof(uint32_t)
      );
    }

    pendingWidth = width;
    pendingHeight = height;
    frameGeneration++;
  }

  auto videoRgba(
    ares::Node::Video::Screen,
    const u8* data,
    u32 pitch,
    u32 width,
    u32 height
  ) -> bool override {
    std::lock_guard<std::mutex> lock(frameMutex);
    pendingFrame.resize(static_cast<size_t>(width) * height);

    for(uint32_t y = 0; y < height; y++) {
      auto source = data + static_cast<size_t>(y) * pitch;
      auto target = pendingFrame.data() + static_cast<size_t>(y) * width;
      for(uint32_t x = 0; x < width; x++) {
        target[x] =
          static_cast<uint32_t>(source[x * 4 + 0]) << 16 |
          static_cast<uint32_t>(source[x * 4 + 1]) << 8 |
          static_cast<uint32_t>(source[x * 4 + 2]);
      }
    }

    pendingWidth = width;
    pendingHeight = height;
    frameGeneration++;
    return true;
  }

  auto videoVulkan(
    ares::Node::Video::Screen,
    const ares::VideoVulkanFrame& frame
  ) -> bool override {
    if(!vulkanInterface) return false;
    std::lock_guard<std::mutex> lock(frameMutex);
    pendingVulkanFrame = frame;
    vulkanFrameGeneration++;
    return true;
  }

  auto refreshRateHint(double rate) -> void override {
    if(rate > 1.0) refreshRate.store(rate);
  }

  auto audio(ares::Node::Audio::Stream stream) -> void override {
    while(stream->pending()) {
      f64 samples[2] = {};
      auto channels = stream->read(samples);
      double left = samples[0];
      double right = channels > 1 ? samples[1] : samples[0];
      left = std::clamp(left, -1.0, 1.0);
      right = std::clamp(right, -1.0, 1.0);
      audioFrames.push_back(static_cast<int16_t>(std::lround(left * 32767.0)));
      audioFrames.push_back(static_cast<int16_t>(std::lround(right * 32767.0)));
    }
  }

  auto input(ares::Node::Input::Input node) -> void override {
    auto port = nodePort(node);
    if(port >= MaxPorts) return;

    if(auto rumble = node->cast<ares::Node::Input::Rumble>()) {
      if(rumbleInterface.set_rumble_state) {
        rumbleInterface.set_rumble_state(
          port,
          RETRO_RUMBLE_STRONG,
          rumble->strongValue()
        );
        rumbleInterface.set_rumble_state(
          port,
          RETRO_RUMBLE_WEAK,
          rumble->weakValue()
        );
      }
      return;
    }

    auto name = node->name();
    if(auto axis = node->cast<ares::Node::Input::Axis>()) {
      if(name == "X-Axis") {
        axis->setValue(pollAnalog(
          port,
          RETRO_DEVICE_INDEX_ANALOG_LEFT,
          RETRO_DEVICE_ID_ANALOG_X
        ));
      } else if(name == "Y-Axis") {
        axis->setValue(pollAnalog(
          port,
          RETRO_DEVICE_INDEX_ANALOG_LEFT,
          RETRO_DEVICE_ID_ANALOG_Y
        ));
      } else {
        axis->setValue(0);
      }
      return;
    }

    auto button = node->cast<ares::Node::Input::Button>();
    if(!button) return;

    bool value = false;
    if(name == "Up") value = pollJoypad(port, RETRO_DEVICE_ID_JOYPAD_UP);
    else if(name == "Down") value = pollJoypad(port, RETRO_DEVICE_ID_JOYPAD_DOWN);
    else if(name == "Left") value = pollJoypad(port, RETRO_DEVICE_ID_JOYPAD_LEFT);
    else if(name == "Right") value = pollJoypad(port, RETRO_DEVICE_ID_JOYPAD_RIGHT);
    else if(name == "A") value = pollJoypad(port, RETRO_DEVICE_ID_JOYPAD_B);
    else if(name == "B") value = pollJoypad(port, RETRO_DEVICE_ID_JOYPAD_Y);
    else if(name == "L") value = pollJoypad(port, RETRO_DEVICE_ID_JOYPAD_L);
    else if(name == "R") value = pollJoypad(port, RETRO_DEVICE_ID_JOYPAD_R);
    else if(name == "Z") value = pollJoypad(port, RETRO_DEVICE_ID_JOYPAD_L2);
    else if(name == "Start") value = pollJoypad(port, RETRO_DEVICE_ID_JOYPAD_START);
    else if(name == "C-Up") {
      value = pollAnalog(
        port,
        RETRO_DEVICE_INDEX_ANALOG_RIGHT,
        RETRO_DEVICE_ID_ANALOG_Y
      ) < -16384;
    } else if(name == "C-Down") {
      value = pollAnalog(
        port,
        RETRO_DEVICE_INDEX_ANALOG_RIGHT,
        RETRO_DEVICE_ID_ANALOG_Y
      ) > 16384;
    } else if(name == "C-Left") {
      value = pollAnalog(
        port,
        RETRO_DEVICE_INDEX_ANALOG_RIGHT,
        RETRO_DEVICE_ID_ANALOG_X
      ) < -16384;
    } else if(name == "C-Right") {
      value = pollAnalog(
        port,
        RETRO_DEVICE_INDEX_ANALOG_RIGHT,
        RETRO_DEVICE_ID_ANALOG_X
      ) > 16384;
    }
    button->setValue(value);
  }

  auto beginRun() -> void {
    audioFrames.clear();
  }

  auto updateFrame() -> void {
    std::lock_guard<std::mutex> lock(frameMutex);
    if(frameGeneration != consumedGeneration && !pendingFrame.empty()) {
      consumedGeneration = frameGeneration;
      frontFrame.swap(pendingFrame);
      frontWidth = pendingWidth;
      frontHeight = pendingHeight;
    }
    if(vulkanFrameGeneration != consumedVulkanFrameGeneration) {
      consumedVulkanFrameGeneration = vulkanFrameGeneration;
      frontVulkanFrame = pendingVulkanFrame;
    }
  }

  auto present() -> void {
    if(videoCallback) {
      bool presentedVulkan = false;
      if(
        vulkanInterface &&
        frontVulkanFrame.image &&
        frontVulkanFrame.imageView
      ) {
        if(consumedVulkanFrameGeneration != presentedVulkanFrameGeneration) {
          auto syncIndex = vulkanInterface->get_sync_index(
            vulkanInterface->handle
          );
          ares::Nintendo64::vulkan.retainScanout(
            syncIndex,
            frontVulkanFrame.image
          );

          retro_vulkan_image image = {};
          image.image_view =
            vulkanHandle<VkImageView>(frontVulkanFrame.imageView);
          image.image_layout =
            static_cast<VkImageLayout>(frontVulkanFrame.imageLayout);
          image.create_info.sType =
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
          image.create_info.image =
            vulkanHandle<VkImage>(frontVulkanFrame.image);
          image.create_info.viewType =
            static_cast<VkImageViewType>(frontVulkanFrame.viewType);
          image.create_info.format =
            static_cast<VkFormat>(frontVulkanFrame.format);
          image.create_info.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
          };
          image.create_info.subresourceRange = {
            VK_IMAGE_ASPECT_COLOR_BIT,
            0,
            1,
            0,
            1,
          };

          vulkanInterface->set_image(
            vulkanInterface->handle,
            &image,
            0,
            nullptr,
            frontVulkanFrame.queueFamily
          );
          videoCallback(
            RETRO_HW_FRAME_BUFFER_VALID,
            frontVulkanFrame.width,
            frontVulkanFrame.height,
            0
          );
          presentedVulkanFrameGeneration = consumedVulkanFrameGeneration;
          presentedVulkan = true;
        } else {
          videoCallback(
            nullptr,
            frontVulkanFrame.width,
            frontVulkanFrame.height,
            0
          );
          presentedVulkan = true;
        }
      }

      if(!presentedVulkan && vulkanInterface) {
        videoCallback(nullptr, 640, pal ? 576 : 480, 0);
      } else if(!presentedVulkan && frontFrame.empty()) {
        videoCallback(nullptr, 640, pal ? 576 : 480, 0);
      } else if(!presentedVulkan) {
        videoCallback(
          frontFrame.data(),
          frontWidth,
          frontHeight,
          static_cast<size_t>(frontWidth) * sizeof(uint32_t)
        );
      }
    }

    if(audioFrames.empty()) return;
    auto frames = audioFrames.size() / 2;
    if(audioBatchCallback) {
      audioBatchCallback(audioFrames.data(), frames);
    } else if(audioSampleCallback) {
      for(size_t index = 0; index < frames; index++) {
        audioSampleCallback(audioFrames[index * 2], audioFrames[index * 2 + 1]);
      }
    }
  }

  auto clearFrames() -> void {
    std::lock_guard<std::mutex> lock(frameMutex);
    pendingFrame.clear();
    frontFrame.clear();
    pendingWidth = pendingHeight = 0;
    frontWidth = frontHeight = 0;
    frameGeneration = 0;
    consumedGeneration = 0;
    pendingVulkanFrame = {};
    frontVulkanFrame = {};
    vulkanFrameGeneration = 0;
    consumedVulkanFrameGeneration = 0;
    presentedVulkanFrameGeneration = 0;
    audioFrames.clear();
  }

  std::shared_ptr<mia::Pak> system;
  std::shared_ptr<mia::Pak> game;
  std::array<std::shared_ptr<mia::Pak>, MaxPorts> controllerPaks;
  std::atomic<double> refreshRate = 60.0;
  uint32_t outputScale = 1;
  bool pal = false;

private:
  std::mutex frameMutex;
  std::vector<uint32_t> pendingFrame;
  std::vector<uint32_t> frontFrame;
  std::vector<int16_t> audioFrames;
  uint32_t pendingWidth = 0;
  uint32_t pendingHeight = 0;
  uint32_t frontWidth = 0;
  uint32_t frontHeight = 0;
  uint64_t frameGeneration = 0;
  uint64_t consumedGeneration = 0;
  ares::VideoVulkanFrame pendingVulkanFrame;
  ares::VideoVulkanFrame frontVulkanFrame;
  uint64_t vulkanFrameGeneration = 0;
  uint64_t consumedVulkanFrameGeneration = 0;
  uint64_t presentedVulkanFrameGeneration = 0;
};

LibretroPlatform platform;
ares::Node::System root;
bool initialized = false;
bool loaded = false;
uint64_t frameCounter = 0;
size_t serializeSize = 0;  // the N64 state size is fixed once content is loaded

auto saveContent() -> void {
  if(!loaded || !root || !platform.game) return;
  root->save();
  platform.game->save(platform.game->location);
  for(auto& controllerPak : platform.controllerPaks) {
    if(controllerPak) {
      controllerPak->save("save.pak", ".pak", platform.game->location);
    }
  }
}

auto unloadContent() -> void {
  if(!loaded) return;
  saveContent();
  if(root) root->unload();
  root.reset();
  platform.system.reset();
  platform.game.reset();
  for(auto& controllerPak : platform.controllerPaks) controllerPak.reset();
  platform.clearFrames();
  loaded = false;
  frameCounter = 0;
  serializeSize = 0;
}

auto configureMiaDirectories() -> void {
  auto systemDirectory = directoryFromEnvironment(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY);
  auto saveDirectory = directoryFromEnvironment(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY);
  if(systemDirectory.empty()) systemDirectory = saveDirectory;
  if(systemDirectory.empty()) systemDirectory = "./";

  mia::setHomeLocation([systemDirectory] {
    return nall::string{systemDirectory.c_str()};
  });
  mia::setSaveLocation([saveDirectory] {
    return nall::string{saveDirectory.c_str()};
  });
}

auto loadResultName(LoadResult result) -> const char* {
  switch(result.result) {
  case successful: return "success";
  case noFileSelected: return "no file selected";
  case databaseNotFound: return "database not found";
  case romNotFoundInDatabase: return "ROM not found in database";
  case romNotFound: return "ROM not found";
  case invalidROM: return "invalid ROM";
  case couldNotParseManifest: return "could not parse manifest";
  case noFirmware: return "firmware not found";
  case otherError: return "other error";
  }
  return "unknown error";
}

auto connectController(unsigned portIndex) -> void {
  auto port = root->find<ares::Node::Port>(
    nall::string{"Controller Port ", portIndex + 1}
  );
  if(!port) return;

  auto peripheral = port->allocate("Gamepad");
  port->connect();
  if(!peripheral) return;

  auto pakPort = peripheral->find<ares::Node::Port>("Pak");
  if(!pakPort || !platform.game || !platform.game->pak) return;

  auto cpakKey = nall::string{"port", portIndex + 1, "/cpak"};
  auto rpakKey = nall::string{"port", portIndex + 1, "/rpak"};
  bool wantsControllerPak = platform.game->pak->attribute(cpakKey).boolean();
  bool wantsRumblePak = platform.game->pak->attribute(rpakKey).boolean();

  if(wantsControllerPak) {
    auto controllerPak = mia::Pak::create("Nintendo 64");
    controllerPak->pak->append("save.pak", 32_KiB);
    controllerPak->load("save.pak", ".pak", platform.game->location);
    platform.controllerPaks[portIndex] = controllerPak;
    pakPort->allocate("Controller Pak");
    pakPort->connect();
  } else if(wantsRumblePak) {
    pakPort->allocate("Rumble Pak");
    pakPort->connect();
  }
}

// NTSC and PAL cartridges share IPL3 code for most CIC pairs, so mia derives the
// CIC variant from the header region code. Keep both in sync when forcing a region.
auto overrideRegion(bool pal) -> void {
  auto pak = platform.game->pak;
  pak->setAttribute("region", pal ? "PAL" : "NTSC");
  static const char* pairs[][2] = {
    {"CIC-NUS-6101", "CIC-NUS-7102"},
    {"CIC-NUS-6102", "CIC-NUS-7101"},
    {"CIC-NUS-6103", "CIC-NUS-7103"},
    {"CIC-NUS-6105", "CIC-NUS-7105"},
    {"CIC-NUS-6106", "CIC-NUS-7106"},
  };
  auto cic = pak->attribute("cic");
  for(auto& pair : pairs) {
    if(cic == pair[pal ? 0 : 1]) {
      pak->setAttribute("cic", pair[pal ? 1 : 0]);
      break;
    }
  }
}

auto updateGeometry() -> void {
  if(!environmentCallback) return;
  retro_game_geometry geometry = {};
  geometry.base_width = 640;
  geometry.base_height = platform.pal ? 576 : 480;
  geometry.max_width = 640 * platform.outputScale;
  geometry.max_height = (platform.pal ? 576 : 480) * platform.outputScale;
  geometry.aspect_ratio = 4.0f / 3.0f;
  environmentCallback(RETRO_ENVIRONMENT_SET_GEOMETRY, &geometry);
}

} // namespace

extern "C" {

RETRO_API void retro_set_environment(retro_environment_t callback) {
  environmentCallback = callback;

  ares_libretro_core_options::registerOptions(callback);

  static const retro_controller_description controllerTypes[] = {
    {"Nintendo 64 Controller", RETRO_DEVICE_JOYPAD},
  };
  static const retro_controller_info controllerInfo[] = {
    {controllerTypes, 1},
    {controllerTypes, 1},
    {controllerTypes, 1},
    {controllerTypes, 1},
    {nullptr, 0},
  };
  callback(
    RETRO_ENVIRONMENT_SET_CONTROLLER_INFO,
    const_cast<retro_controller_info*>(controllerInfo)
  );
}

RETRO_API void retro_set_video_refresh(retro_video_refresh_t callback) {
  videoCallback = callback;
}

RETRO_API void retro_set_audio_sample(retro_audio_sample_t callback) {
  audioSampleCallback = callback;
}

RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t callback) {
  audioBatchCallback = callback;
}

RETRO_API void retro_set_input_poll(retro_input_poll_t callback) {
  inputPollCallback = callback;
}

RETRO_API void retro_set_input_state(retro_input_state_t callback) {
  inputStateCallback = callback;
}

RETRO_API void retro_init(void) {
  if(initialized) return;
  if(environmentCallback) {
    retro_log_callback logging = {};
    if(environmentCallback(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging)) {
      logCallback = logging.log;
    }
    environmentCallback(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumbleInterface);
  }
  mia::construct();
  ares::platform = &platform;
  initialized = true;
  logMessage(RETRO_LOG_INFO, "ares N64 libretro initialized");
}

RETRO_API void retro_deinit(void) {
  unloadContent();
  destroyVulkanDevice();
  logMessage(RETRO_LOG_INFO, "Vulkan core resources released");
  ares::platform = nullptr;
  initialized = false;
  logCallback = nullptr;
  rumbleInterface = {};
}

RETRO_API unsigned retro_api_version(void) {
  return RETRO_API_VERSION;
}

RETRO_API void retro_get_system_info(struct retro_system_info* info) {
  if(!info) return;
  std::memset(info, 0, sizeof(*info));
  info->library_name = "ares N64";
  info->library_version = ares::Version.data();
  info->valid_extensions = "z64|n64|v64|zip";
  info->need_fullpath = true;
  info->block_extract = false;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info* info) {
  if(!info) return;
  std::memset(info, 0, sizeof(*info));
  info->geometry.base_width = 640;
  info->geometry.base_height = platform.pal ? 576 : 480;
  info->geometry.max_width = 640 * platform.outputScale;
  info->geometry.max_height =
    (platform.pal ? 576 : 480) * platform.outputScale;
  info->geometry.aspect_ratio = 4.0f / 3.0f;
  info->timing.fps = platform.refreshRate.load();
  info->timing.sample_rate = AudioFrequency;
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device) {
  if(port < portDevices.size()) portDevices[port] = device;
}

RETRO_API void retro_reset(void) {
  if(root) root->power(true);
}

RETRO_API void retro_run(void) {
  if(!loaded || !root) return;
  if(inputPollCallback) inputPollCallback();

  platform.beginRun();
  root->run();
  platform.updateFrame();
  platform.present();

  frameCounter++;
  if(frameCounter % 1800 == 0) saveContent();
}

RETRO_API size_t retro_serialize_size(void) {
  if(!loaded || !root) return 0;
  if(!serializeSize) serializeSize = root->serialize(true).size();
  return serializeSize;
}

RETRO_API bool retro_serialize(void* data, size_t size) {
  if(!loaded || !root || !data) return false;
  auto state = root->serialize(true);
  if(size < state.size()) return false;
  std::memcpy(data, state.data(), state.size());
  if(size > state.size()) {
    std::memset(static_cast<uint8_t*>(data) + state.size(), 0, size - state.size());
  }
  return true;
}

RETRO_API bool retro_unserialize(const void* data, size_t size) {
  if(!loaded || !root || !data || size > UINT32_MAX) return false;
  serializer state{static_cast<const u8*>(data), static_cast<u32>(size)};
  return root->unserialize(state);
}

RETRO_API void retro_cheat_reset(void) {
}

RETRO_API void retro_cheat_set(unsigned, bool, const char*) {
}

RETRO_API bool retro_load_game(const struct retro_game_info* info) {
  if(!initialized) retro_init();
  unloadContent();
  if(!info || !info->path || !*info->path) {
    logMessage(RETRO_LOG_ERROR, "ares N64 requires a full ROM path");
    return false;
  }

  enum retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
  if(!environmentCallback ||
     !environmentCallback(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format)) {
    logMessage(RETRO_LOG_ERROR, "frontend rejected XRGB8888 video");
    return false;
  }

  configureMiaDirectories();
  platform.clearFrames();
  platform.game = mia::Medium::create("Nintendo 64");
  if(!platform.game) return false;

  auto result = platform.game->load(nall::string{info->path});
  if(result != successful) {
    std::string message = "failed to load N64 ROM: ";
    message += loadResultName(result);
    logMessage(RETRO_LOG_ERROR, message.c_str());
    platform.game.reset();
    return false;
  }

  if(platform.game->pak->attribute("dd").boolean()) {
    logMessage(
      RETRO_LOG_ERROR,
      "64DD cartridge detected; this core currently supports cartridge-only N64"
    );
    platform.game.reset();
    return false;
  }

  platform.system = mia::System::create("Nintendo 64");
  if(!platform.system) return false;
  result = platform.system->load();
  if(result != successful) {
    std::string message = "failed to load N64 system data: ";
    message += loadResultName(result);
    logMessage(RETRO_LOG_ERROR, message.c_str());
    platform.system.reset();
    platform.game.reset();
    return false;
  }

  auto regionOption = optionValue("ares_n64_region", "auto");
  if(regionOption[0] == 'N' || regionOption[0] == 'P') {
    overrideRegion(regionOption[0] == 'P');
    std::string message = "region forced to ";
    message += regionOption;
    logMessage(RETRO_LOG_INFO, message.c_str());
  }
  auto region = platform.game->pak->attribute("region");
  platform.pal = region.beginsWith("PAL");
  platform.refreshRate.store(platform.pal ? 50.0 : 60.0);

  auto renderer = optionValue("ares_n64_renderer", "paraLLEl-RDP (Vulkan)");
  auto quality = optionValue("ares_n64_quality", "SD");
  auto recompiler = optionValue("ares_n64_recompiler", "enabled");
  auto expansionPak = optionValue("ares_n64_expansion_pak", "enabled");
  platform.outputScale = quality[0] == 'U' ? 4 : quality[0] == 'H' ? 2 : 1;

  if(renderer[0] != 'S' && !requestVulkanContext()) {
    logMessage(
      RETRO_LOG_WARN,
      "frontend Vulkan negotiation unavailable; using the compatible readback path"
    );
  }

  ares::Nintendo64::option("Enable GPU acceleration", renderer[0] != 'S');
  ares::Nintendo64::option("Quality", quality);
  ares::Nintendo64::option("Supersampling", false);
  ares::Nintendo64::option("Disable Video Interface Processing", false);
  ares::Nintendo64::option("Weave Deinterlacing", true);
  ares::Nintendo64::option("Homebrew Mode", false);
  ares::Nintendo64::option("Deterministic Entropy", false);
  ares::Nintendo64::option("Recompiler", recompiler[0] == 'e');
  ares::Nintendo64::option("Expansion Pak", expansionPak[0] == 'e');
  ares::Nintendo64::option("Controller Pak Banks", "32KiB (Default)");

  auto configuration = nall::string{
    "[Nintendo] Nintendo 64 (",
    platform.pal ? "PAL" : "NTSC",
    ")"
  };
  if(!ares::Nintendo64::load(root, configuration)) {
    logMessage(RETRO_LOG_ERROR, "ares rejected the N64 system configuration");
    platform.system.reset();
    platform.game.reset();
    return false;
  }

  if(auto cartridge = root->find<ares::Node::Port>("Cartridge Slot")) {
    cartridge->allocate();
    cartridge->connect();
  } else {
    root->unload();
    root.reset();
    platform.system.reset();
    platform.game.reset();
    return false;
  }

  for(unsigned port = 0; port < MaxPorts; port++) connectController(port);

  root->power(false);
  loaded = true;
  frameCounter = 0;
  updateGeometry();
  logMessage(RETRO_LOG_INFO, "N64 cartridge loaded successfully");
  return true;
}

RETRO_API bool retro_load_game_special(
  unsigned,
  const struct retro_game_info*,
  size_t
) {
  return false;
}

RETRO_API void retro_unload_game(void) {
  unloadContent();
}

RETRO_API unsigned retro_get_region(void) {
  return platform.pal ? RETRO_REGION_PAL : RETRO_REGION_NTSC;
}

RETRO_API void* retro_get_memory_data(unsigned) {
  return nullptr;
}

RETRO_API size_t retro_get_memory_size(unsigned) {
  return 0;
}

} // extern "C"
