# ares Nintendo 64 libretro core

This target is a focused libretro frontend for the ares Nintendo 64 core.
It supports cartridge ROMs (`.z64`, `.n64`, `.v64`, and `.zip`), four
controllers, analog sticks, Rumble Pak, Controller Pak, cartridge saves, and
save states. Nintendo 64DD media is intentionally out of scope.

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

The `Build Nintendo 64 libretro core` GitHub Actions workflow produces a
portable Windows x64 Release build with LTO and publishes the DLL as a workflow
artifact.
