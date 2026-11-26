namespace Vulkan {
struct Context;
}

namespace ares {
struct VideoVulkanFrame;
}

namespace ares::Nintendo64 {

struct Vulkan {
  auto load(Node::Object) -> bool;
  auto unload() -> void;

  auto render() -> bool;
  auto frame() -> void;
  auto writeWord(u32 address, u32 data) -> void;
  auto scanoutAsync(bool field) -> bool;
  auto mapScanoutVulkan(VideoVulkanFrame& frame) -> bool;
  auto retainScanout(u32 syncIndex, u64 imageHandle) -> void;
  auto mapScanoutRead(const u8*& rgba, u32& width, u32& height) -> void;
  auto unmapScanoutRead() -> void;
  auto endScanout() -> void;
  auto crashed() -> const char*;

  struct Implementation;
  Implementation* implementation = nullptr;

  bool enable = true;
  bool disableVideoInterfaceProcessing = false;
  bool weaveDeinterlacing = false;
  u32  internalUpscale = 1;  //1, 2, 4, 8
  bool supersampleScanout = false;
  u32  outputUpscale = supersampleScanout ? 1 : internalUpscale;
};

extern Vulkan vulkan;

auto setVulkanExternalContext(
  ::Vulkan::Context* context,
  void (*lockQueue)(),
  void (*unlockQueue)()
) -> void;
auto clearVulkanExternalContext() -> void;

}
