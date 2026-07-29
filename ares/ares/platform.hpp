#pragma once

namespace ares {

enum class Event : u32 {
  None,
  Step,
  Frame,
  Power,
  Synchronize,
  FastForwardOn,
  FastForwardOff,
  Shutdown
};

struct VideoVulkanFrame {
  u64 image = 0;
  u64 imageView = 0;
  u64 semaphore = 0;
  u32 imageLayout = 0;
  u32 viewType = 0;
  u32 format = 0;
  u32 queueFamily = 0;
  u32 width = 0;
  u32 height = 0;
};

struct Platform {
  virtual auto attach(Node::Object) -> void {}
  virtual auto detach(Node::Object) -> void {}
  virtual auto pak(Node::Object) -> std::shared_ptr<vfs::directory> { return {}; }
  virtual auto event(Event) -> void {}
  virtual auto log(Node::Debugger::Tracer::Tracer, string_view message) -> void {}
  virtual auto status(string_view message) -> void {}
  virtual auto video(Node::Video::Screen, const u32* data, u32 pitch, u32 width, u32 height) -> void {}
  virtual auto videoRgba(Node::Video::Screen, const u8* data, u32 pitch, u32 width, u32 height) -> bool { return false; }
  virtual auto videoVulkan(Node::Video::Screen, const VideoVulkanFrame&) -> bool { return false; }
  virtual auto refreshRateHint(double refreshRate) -> void {}
  virtual auto audio(Node::Audio::Stream) -> void {}
  virtual auto input(Node::Input::Input) -> void {}
  virtual auto cheat(u32 addr) -> maybe<u32> { return nothing; }
};

extern Platform* platform;

}

namespace ares::Core {
  // <ares/node/node.hpp> forward declarations
  auto PlatformAttach(Node::Object node) -> void { if(platform && node->name()) platform->attach(node); }
  auto PlatformDetach(Node::Object node) -> void { if(platform && node->name()) platform->detach(node); }
  auto PlatformLog(Node::Debugger::Tracer::Tracer node, string_view text) -> void { if(platform) platform->log(node, text); }
}
