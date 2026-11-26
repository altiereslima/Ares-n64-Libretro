#include <n64/n64.hpp>
#include <array>

//included here rather than in vulkan.hpp so that parallel-rdp headers are only
//parsed in this translation unit
#include "rdp_device.hpp"

namespace ares::Nintendo64 {

Vulkan vulkan;

namespace {

::Vulkan::Context* externalContext = nullptr;
void (*externalLockQueue)() = nullptr;
void (*externalUnlockQueue)() = nullptr;

template<typename T>
auto vulkanHandleValue(T handle) -> u64 {
  if constexpr(std::is_pointer_v<T>) {
    return (u64)reinterpret_cast<uintptr_t>(handle);
  } else {
    return (u64)handle;
  }
}

}

struct LoggingInterface : Util::LoggingInterface {
  auto log(const char* tag, const char* fmt, va_list va) -> bool {
    char buffer[8192];
    vsnprintf(buffer, sizeof(buffer), fmt, va);
  //print(terminal::color::yellow(tag), buffer);
    return true;
  }
} loggingInterface;

struct Vulkan::Implementation {
  Implementation(u8* data, u32 size);
  ~Implementation();

  ::Vulkan::Context ownedContext;
  ::Vulkan::Context* context = nullptr;
  ::Vulkan::Device device;
  ::RDP::CommandProcessor* processor = nullptr;
  atomic<const char*> crash_error = nullptr;
  bool directOutput = false;

  struct Validation : public ::RDP::ValidationInterface {
    Implementation& self;
    Validation(Implementation& i) : self(i) {}
    void report_rdp_crash(::RDP::ValidationError err, const char *msg) override {
      self.crash_error = msg;
    }
  } validator{*this};

  //commands are u64 words, but the backend uses u32 swapped words.
  //size and offset are in u64 words.
  u32 buffer[0x10000] = {};
  u32 queueSize = 0;
  u32 queueOffset = 0;

  ::RDP::VIScanoutBuffer scanout;
  ::Vulkan::ImageHandle scanoutImage;
  std::array<::Vulkan::ImageHandle, 8> recentScanouts;
  u32 recentScanoutIndex = 0;
  std::vector<::Vulkan::ImageHandle> retainedScanouts;
  std::mutex lock;
  std::condition_variable condition;
  u32 scanoutCount = 0;
  u32 endCount = 0;
};

auto Vulkan::load(Node::Object) -> bool {
  if (vulkan.enable) {
    Util::set_thread_logging_interface(&loggingInterface);
    delete implementation;
    implementation = new Vulkan::Implementation(rdram.ram.data, rdram.ram.size);
    if(!implementation->processor) {
      delete implementation;
      implementation = nullptr;
    }

    if (!implementation) {
      platform->status("Vulkan init failed: No RDP rendering support");
      vulkan.enable = false;
      rdram.hidden.data = nullptr;
    } else {
      platform->status("Vulkan Enabled: using paraLLEl-RDP");
      rdram.hidden.data = (u8*)implementation->processor->begin_read_hidden_rdram();
    }
  } else {
    platform->status("Vulkan Disabled: No RDP rendering support");
    rdram.hidden.data = nullptr;
  }

  return true;
}

auto Vulkan::unload() -> void {
  rdram.hidden.data = nullptr;
  if (implementation) delete implementation;
  implementation = nullptr;
}

auto Vulkan::render() -> bool {
  if(!implementation) return false;

  static constexpr u32 commandLength[64] = {
    1, 1, 1, 1, 1, 1, 1, 1, 4, 6,12,14,12,14,20,22,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  };

  auto& command = rdp.command;

  u32 current = command.current & ~7;
  u32 end = command.end & ~7;
  u32 length = (end - current) / 8;
  if(current >= end) return true;

  u32* buffer = implementation->buffer;
  u32& queueSize = implementation->queueSize;
  u32& queueOffset = implementation->queueOffset;
  if(queueSize + length >= 0x8000) return true;

  if(!command.source) {
    do {
      buffer[queueSize * 2 + 0] = rdram.ram.read<Word>(current, RBusDevice::DP_DMA); current += 4;
      buffer[queueSize * 2 + 1] = rdram.ram.read<Word>(current, RBusDevice::DP_DMA); current += 4;
      queueSize++;
    } while(--length);
  } else {
    do {
      buffer[queueSize * 2 + 0] = rsp.dmem.read<Word>(current); current += 4;
      buffer[queueSize * 2 + 1] = rsp.dmem.read<Word>(current); current += 4;
      if(system.homebrewMode) {
        rsp.debugger.dmemReadWord(current - 8, 8, "RDP XBUS");
      }
      queueSize++;
    } while(--length);
  }

  while(queueOffset < queueSize) {
    u32 op = buffer[queueOffset * 2];
    u32 code = op >> 24 & 63;
    u32 length = commandLength[code];

    if(queueOffset + length > queueSize) {
      //partial command, keep data around for next processing call
      command.start = command.current = command.end;
      return true;
    }

    if(code >= 8) {
      implementation->processor->enqueue_command(length * 2, buffer + queueOffset * 2);
    }

    if(::RDP::Op(code) == ::RDP::Op::SyncFull) {
      implementation->processor->wait_for_timeline(implementation->processor->signal_timeline());
      rdp.syncFull();
    }

    queueOffset += length;
  }

  queueOffset = 0;
  queueSize = 0;
  command.current = command.end;
  return true;
}

auto Vulkan::frame() -> void {
  if(!implementation) return;
  implementation->processor->begin_frame_context();
}

auto Vulkan::writeWord(u32 address, u32 data) -> void {
  if(!implementation) return;
  implementation->processor->set_vi_register(::RDP::VIRegister(address), data);
}

auto Vulkan::scanoutAsync(bool field) -> bool {
  if(!implementation) return false;

  { //wait until we're done reading in thread before we clobber the readback buffer
    std::unique_lock<std::mutex> lock{implementation->lock};
    implementation->condition.wait(lock, [this]() {
      return implementation->scanoutCount == implementation->endCount;
    });
  }

  implementation->processor->set_vi_register(::RDP::VIRegister::VCurrentLine, field);

  //0 steps if scanning out at upscaled resolution.
  //each downscale step reduces output resolution to [width, height] * max(1, upscale >> downscale_steps)
  ::RDP::ScanoutOptions options;
  options.downscale_steps = supersampleScanout ? 16 : 0;
  options.persist_frame_on_invalid_input = true;  //this is a compatibility hack, but I'm not sure what for ...
  if(disableVideoInterfaceProcessing) {
    options.vi = {false, false, true, false, false, false};
  }
  if(!supersampleScanout){
    options.blend_previous_frame = weaveDeinterlacing;
    options.upscale_deinterlacing = !weaveDeinterlacing;
  }
  else {
    options.blend_previous_frame = false;
    options.upscale_deinterlacing = true;
  }

  if(implementation->directOutput) {
    implementation->scanoutImage = implementation->processor->scanout(options);
    if(implementation->scanoutImage) {
      implementation->recentScanouts[
        implementation->recentScanoutIndex++ %
        implementation->recentScanouts.size()
      ] = implementation->scanoutImage;
      implementation->device.flush_frame();
    }
    implementation->scanoutCount++;
    return true;
  }

  if(implementation->scanout.fence) {
    implementation->scanout.fence->wait();
  }
  implementation->processor->scanout_async_buffer(implementation->scanout, options);
  implementation->scanoutCount++;
  return true;
}

auto Vulkan::mapScanoutVulkan(VideoVulkanFrame& frame) -> bool {
  frame = {};
  if(
    !implementation ||
    !implementation->directOutput ||
    !implementation->scanoutImage
  ) return false;

  auto& image = *implementation->scanoutImage;
  frame.image = vulkanHandleValue(image.get_image());
  frame.imageView = vulkanHandleValue(image.get_view().get_view());
  frame.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  frame.viewType = VK_IMAGE_VIEW_TYPE_2D;
  frame.format = image.get_format();
  frame.queueFamily =
    implementation->context->get_queue_info().family_indices[::Vulkan::QUEUE_INDEX_GRAPHICS];
  frame.width = image.get_width();
  frame.height = image.get_height();
  return true;
}

auto Vulkan::retainScanout(u32 syncIndex, u64 imageHandle) -> void {
  if(
    !implementation ||
    !implementation->directOutput ||
    !implementation->scanoutImage
  ) return;

  if(implementation->retainedScanouts.size() <= syncIndex) {
    implementation->retainedScanouts.resize(syncIndex + 1);
  }

  for(auto& scanout : implementation->recentScanouts) {
    if(scanout && vulkanHandleValue(scanout->get_image()) == imageHandle) {
      implementation->retainedScanouts[syncIndex] = scanout;
      return;
    }
  }

  implementation->retainedScanouts[syncIndex] = implementation->scanoutImage;
}

auto Vulkan::mapScanoutRead(const u8*& rgba, u32& width, u32& height) -> void {
  if(!implementation || !implementation->scanout.fence || !implementation->scanout.width || !implementation->scanout.height) {
    rgba = nullptr;
    width = 0;
    height = 0;
  } else {
    implementation->scanout.fence->wait();
    rgba = (const u8*)implementation->device.map_host_buffer(*implementation->scanout.buffer, ::Vulkan::MEMORY_ACCESS_READ_BIT);
    width = implementation->scanout.width;
    height = implementation->scanout.height;
  }
}

auto Vulkan::unmapScanoutRead() -> void {
  if(implementation && implementation->scanout.buffer) {
    implementation->device.unmap_host_buffer(*implementation->scanout.buffer, ::Vulkan::MEMORY_ACCESS_READ_BIT);
  }
}

auto Vulkan::endScanout() -> void {
  if(implementation) {
    //notify main thread that we're done reading
    std::lock_guard<std::mutex> lock{implementation->lock};
    implementation->endCount++;
    implementation->condition.notify_one();
  }
}

auto Vulkan::crashed() -> const char* {
  if(implementation) return implementation->crash_error;
  return nullptr;
}

Vulkan::Implementation::Implementation(u8* data, u32 size) {
  if(!::Vulkan::Context::init_loader(nullptr)) return;
  if(externalContext) {
    context = externalContext;
    directOutput = true;
  } else {
    context = &ownedContext;
    if(!context->init_instance_and_device(nullptr, 0, nullptr, 0, 0)) return;
  }
  device.set_context(*context);
  if(directOutput && externalLockQueue && externalUnlockQueue) {
    device.set_queue_lock(externalLockQueue, externalUnlockQueue);
  }
  device.init_frame_contexts(3);

  ::RDP::CommandProcessorFlags flags = ::RDP::COMMAND_PROCESSOR_FLAG_HOST_VISIBLE_HIDDEN_RDRAM_BIT;
  switch(vulkan.internalUpscale) {
  case 2: flags |= ::RDP::COMMAND_PROCESSOR_FLAG_UPSCALING_2X_BIT; break;
  case 4: flags |= ::RDP::COMMAND_PROCESSOR_FLAG_UPSCALING_4X_BIT; break;
  case 8: flags |= ::RDP::COMMAND_PROCESSOR_FLAG_UPSCALING_8X_BIT; break;
  }

  if(vulkan.internalUpscale > 1) {
    flags |= ::RDP::COMMAND_PROCESSOR_FLAG_SUPER_SAMPLED_DITHER_BIT;
    //rasky: this is explicitly disabled because we want to make sure we don't
    // read back the super sampled version, as it can cause artifacts. We want
    // parallelRDP to also produce a 1x render to use for readbacks.
    //flags |= ::RDP::COMMAND_PROCESSOR_FLAG_SUPER_SAMPLED_READ_BACK_BIT;
  }

  processor = new ::RDP::CommandProcessor(device, data, 0, size, size / 2, flags);
  if(!processor->device_is_supported()) {
    delete processor;
    processor = nullptr;
    return;
  }

  processor->set_validation_interface(&validator);
}

Vulkan::Implementation::~Implementation() {
  if(processor) delete processor;
}

auto setVulkanExternalContext(
  ::Vulkan::Context* context,
  void (*lockQueue)(),
  void (*unlockQueue)()
) -> void {
  externalContext = context;
  externalLockQueue = lockQueue;
  externalUnlockQueue = unlockQueue;
}

auto clearVulkanExternalContext() -> void {
  vulkan.unload();
  externalContext = nullptr;
  externalLockQueue = nullptr;
  externalUnlockQueue = nullptr;
}

}
