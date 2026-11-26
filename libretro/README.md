# ares Nintendo 64 libretro core

This target is a focused libretro frontend for the ares Nintendo 64 core.
It supports cartridge ROMs (`.z64`, `.n64`, `.v64`, and `.zip`), four
controllers, analog sticks, Rumble Pak, Controller Pak, cartridge saves, and
save states. Nintendo 64DD media is intentionally out of scope.

Core options:

| Key | Values | Notes |
| --- | --- | --- |
| `ares_n64_renderer` | `paraLLEl-RDP (Vulkan)`, `Software` | Vulkan uses the frontend's device (zero-copy) when it supports context negotiation v2. |
| `ares_n64_quality` | `SD`, `HD`, `UHD` | paraLLEl-RDP internal resolution. |
| `ares_n64_recompiler` | `enabled`, `disabled` | CPU and RSP dynamic recompilers. |
| `ares_n64_expansion_pak` | `enabled`, `disabled` | 8 MiB Expansion Pak. |
| `ares_n64_region` | `auto`, `NTSC`, `PAL` | Overrides the ROM header region (and the matching CIC). Useful for translations whose header was retagged, e.g. a Japanese game marked as PAL running at 50 Hz. |

Performance notes for this port:

- Frames are refreshed synchronously at the end of `retro_run()` (no separate
  screen thread), so the frontend always receives the frame produced during
  that call instead of racing an asynchronous refresh.
- The CPU JIT budget treats COUNT/COMPARE as wrapping counters. Upstream
  clamped a negative distance to zero, which forced a full hardware
  synchronization after every recompiled block in games where COUNT had
  already passed COMPARE.
- `retro_serialize_size()` is cached per loaded game.

The build downloads a pinned revision of
[`libretro-common`](https://github.com/libretro/libretro-common) for the public
libretro headers. An existing checkout can be used instead by setting
`LIBRETRO_INCLUDE_DIR` to its `include` directory.

Configure and build with CMake and Ninja:

```sh
cmake -S . -B build/libretro-n64 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DARES_BUILD_LIBRETRO_N64=ON \
  -DARES_CORES=n64 \
  -DARES_ENABLE_CHD=OFF \
  -DARES_UNITY_CORES=ON
cmake --build build/libretro-n64 --target ares-n64-libretro
```

The resulting core is named `ares_n64_libretro.dll` on Windows,
`ares_n64_libretro.so` on Linux, and `ares_n64_libretro.dylib` on macOS.

The `Build Nintendo 64 libretro core` GitHub Actions workflow produces portable
Windows and Linux x64 Release builds with LTO and publishes the DLL and shared
object as workflow artifacts.
