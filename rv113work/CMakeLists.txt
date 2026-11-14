cmake_minimum_required(VERSION 3.24)
project(RealVisionRender VERSION 1.14.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(RV_ENABLE_VULKAN "Enable Vulkan capability probing" ON)
add_executable(realvision_render
 engine/src/main.cpp engine/src/scene.cpp engine/src/transport.cpp
 engine/src/rvlink.cpp engine/src/vulkan_probe.cpp
 engine/src/vulkan_rt_caps.cpp engine/src/vulkan_rt_device.cpp engine/src/vulkan_as.cpp)
target_include_directories(realvision_render PRIVATE engine/include)
target_compile_definitions(realvision_render PRIVATE RV_VERSION="1.14.0")
find_program(GLSLC glslc)
if(GLSLC)
  set(RV_SHADER_SOURCES
    engine/shaders/raygen.rgen
    engine/shaders/miss.rmiss
    engine/shaders/closesthit.rchit)
  set(RV_SPV_OUTPUTS)
  foreach(SHADER ${RV_SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPV "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.spv")
    add_custom_command(OUTPUT "${SPV}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
      COMMAND ${GLSLC} --target-env=vulkan1.2 "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}" -o "${SPV}"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}" VERBATIM)
    list(APPEND RV_SPV_OUTPUTS "${SPV}")
  endforeach()
  add_custom_target(rv_shaders DEPENDS ${RV_SPV_OUTPUTS})
  add_dependencies(realvision_render rv_shaders)
endif()
if(WIN32)
 target_compile_definitions(realvision_render PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
 target_link_libraries(realvision_render PRIVATE ws2_32)
endif()
if(RV_ENABLE_VULKAN)
 find_package(Vulkan QUIET)
 if(Vulkan_FOUND)
  target_compile_definitions(realvision_render PRIVATE RV_HAS_VULKAN=1)
  target_link_libraries(realvision_render PRIVATE Vulkan::Vulkan)
 else()
  message(WARNING "Vulkan SDK not found. GPU backend unavailable in this build.")
 endif()
endif()

# Optional image codecs: formats are enabled when the corresponding development
# package is available at build time. BMP and PPM remain dependency-free.
find_package(PNG QUIET)
if(PNG_FOUND)
 target_compile_definitions(realvision_render PRIVATE RV_HAS_LIBPNG=1)
 target_link_libraries(realvision_render PRIVATE PNG::PNG)
endif()
find_package(JPEG QUIET)
if(JPEG_FOUND)
 target_compile_definitions(realvision_render PRIVATE RV_HAS_LIBJPEG=1)
 target_link_libraries(realvision_render PRIVATE JPEG::JPEG)
endif()
