set(
  LIBRETRO_INCLUDE_DIR
  ""
  CACHE PATH
  "Optional directory containing libretro.h and libretro_vulkan.h"
)

if(NOT LIBRETRO_INCLUDE_DIR)
  include(FetchContent)
  FetchContent_Declare(
    libretro_common
    URL
      https://github.com/libretro/libretro-common/archive/14bf5136e6c96d44e85f539d9e7b535f619fa7a6.tar.gz
    URL_HASH
      SHA256=22e22700c9baefd77dfe4177e60077f6ec7a9bbd7da02d47c7bff46d4f0a20a0
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
  )
  FetchContent_MakeAvailable(libretro_common)
  set(LIBRETRO_INCLUDE_DIR "${libretro_common_SOURCE_DIR}/include")
endif()

if(NOT IS_ABSOLUTE "${LIBRETRO_INCLUDE_DIR}")
  get_filename_component(
    LIBRETRO_INCLUDE_DIR
    "${LIBRETRO_INCLUDE_DIR}"
    ABSOLUTE
    BASE_DIR "${CMAKE_SOURCE_DIR}"
  )
endif()

foreach(header IN ITEMS libretro.h libretro_vulkan.h)
  if(NOT EXISTS "${LIBRETRO_INCLUDE_DIR}/${header}")
    message(
      FATAL_ERROR
      "${header} was not found in LIBRETRO_INCLUDE_DIR='${LIBRETRO_INCLUDE_DIR}'"
    )
  endif()
endforeach()

add_library(ares-n64-libretro SHARED libretro.cpp)

target_compile_definitions(ares-n64-libretro PRIVATE MIA_LIBRARY)
target_include_directories(
  ares-n64-libretro
  PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${LIBRETRO_INCLUDE_DIR}"
    "${CMAKE_SOURCE_DIR}/ares/n64/vulkan/parallel-rdp/parallel-rdp"
    "${CMAKE_SOURCE_DIR}/ares/n64/vulkan/parallel-rdp/volk"
    "${CMAKE_SOURCE_DIR}/ares/n64/vulkan/parallel-rdp/vulkan"
    "${CMAKE_SOURCE_DIR}/ares/n64/vulkan/parallel-rdp/vulkan-headers/include"
    "${CMAKE_SOURCE_DIR}/ares/n64/vulkan/parallel-rdp/util"
)
target_link_libraries(
  ares-n64-libretro
  PRIVATE
    ares::ares
    ares::mia
    ares::nall
    ares::nall::headers
)

set_target_properties(
  ares-n64-libretro
  PROPERTIES
    OUTPUT_NAME "ares_n64_libretro"
    PREFIX ""
)
