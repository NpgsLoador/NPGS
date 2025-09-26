#pragma once

#define _SILENCE_CXX23_DENORM_DEPRECATION_WARNING // for std::float_denorm_style in Imath/half.h

#include "xstdafx.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <boost/multiprecision/cpp_int.hpp>
#include <fast-cpp-csv-parser/csv.h>

#define GLFW_INCLUDE_VULKAN
#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#include <GLFW/glfw3.h>

#define IMATH_DLL
#include <Imath/ImathBox.h>

#include <ktx.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfRgba.h>
#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfTiledRgbaFile.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <spirv_cross/spirv_reflect.hpp>
#include <stb_image.h>
#include <vma/vk_mem_alloc.h>

#define VULKAN_HPP_HANDLES_MOVE_EXCHANGE
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include <Windows.h>

#ifndef _RELEASE
#define NPGS_ENABLE_CONSOLE_LOGGER
#else
#define GLM_FORCE_INLINE
#define NPGS_ENABLE_FILE_LOGGER
#endif // _RELEASE
#include "Engine/Utils/Logger.hpp"
