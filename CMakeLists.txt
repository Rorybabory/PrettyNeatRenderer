cmake_minimum_required(VERSION 3.20)
project(Renderer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Handle single-config generators (like Make/Ninja) vs multi-config (like Visual Studio)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 1. Find Dependencies
include(FetchContent)

find_package(Vulkan REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(ktx CONFIG REQUIRED)

# Fetch OpenXR
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_API_LAYERS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    OpenXR
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
    GIT_TAG        release-1.1.43 # Check GitHub for the latest release tag
)
FetchContent_MakeAvailable(OpenXR)
target_compile_definitions(Renderer PRIVATE XR_USE_GRAPHICS_API_VULKAN)
if(WIN32)
    target_compile_definitions(Renderer PRIVATE XR_USE_PLATFORM_WIN32)
elseif(UNIX AND NOT ANDROID)
    target_compile_definitions(Renderer PRIVATE XR_USE_PLATFORM_XLIB)
endif()

# Fetch Dear ImGui (newest release)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.93.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(imgui PUBLIC
    IMGUI_IMPL_VULKAN_USE_VOLK
)

target_link_libraries(imgui PUBLIC
    Vulkan::Vulkan
    glfw
)

# Find the tools required for custom commands
find_program(SLANGC_EXECUTABLE NAMES slangc REQUIRED)
find_program(KTX_CLI 
    NAMES ktx 
    HINTS "C:/Program Files/KTX-Software/bin"
    REQUIRED
)
# 2. Setup the Executable
add_executable(Renderer 
    src/main.cpp 
    src/vulkan_renderer.cpp 
    src/vma_impl.cpp 
    src/asset_loader.cpp
    src/render_graph.cpp
    src/vkutils.c 
    src/volk.c 
    src/spirv_reflect.c
)

target_compile_definitions(Renderer PRIVATE 
    VMA_STATIC_VULKAN_FUNCTIONS=0 
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
)

include_directories(include)

target_link_libraries(Renderer PRIVATE 
    Vulkan::Vulkan
    glfw
    GPUOpen::VulkanMemoryAllocator
    assimp::assimp
    KTX::ktx
    imgui
)

if(MSVC)
    target_compile_options(Renderer PRIVATE /utf-8)
endif()

# 3. Cross-Platform Shader Compilation (Replaces bash script)
file(GLOB SHADER_SOURCES "shaders/*.vert" "shaders/*.frag" "shaders/*.comp" "shaders/*.slang")
file(GLOB SHADER_MODULES "shaders/modules/*.slang")

set(SHADER_PRODUCTS "")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")

foreach(SHADER IN LISTS SHADER_SOURCES)
    get_filename_component(SHADER_NAME ${SHADER} NAME_WE) 
    
    set(SPV_OUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.spv")
    
    add_custom_command(
        OUTPUT ${SPV_OUT}
        COMMAND ${SLANGC_EXECUTABLE} ${SHADER} 
            -I "${CMAKE_CURRENT_SOURCE_DIR}/shaders/modules"
            -target spirv 
            -profile glsl_460
            -emit-spirv-directly 
            -fvk-use-entrypoint-name 
            -fvk-use-scalar-layout
            -o ${SPV_OUT}
        DEPENDS ${SHADER} ${SHADER_MODULES}
        COMMENT "Compiling ${SHADER_NAME} to SPIR-V with Slang..."
    )
    list(APPEND SHADER_PRODUCTS ${SPV_OUT})
endforeach()

add_custom_target(Shaders ALL
    DEPENDS ${SHADER_PRODUCTS}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        "$<TARGET_FILE_DIR:Renderer>/shaders"
)
add_dependencies(Renderer Shaders)

# 4. Texture Conversion
file(GLOB TEXTURES "textures/*.png" "textures/*.jpg")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/textures")
set(KTX_OUT "")

foreach(IMG IN LISTS TEXTURES)
    get_filename_component(NAME ${IMG} NAME_WE)
    set(OUT "${CMAKE_BINARY_DIR}/textures/${NAME}.ktx2")
    
    add_custom_command(
        OUTPUT ${OUT} 
        COMMAND ${KTX_CLI} create --format VK_FORMAT_R8G8B8A8_SRGB ${IMG} ${OUT} 
        DEPENDS ${IMG}
        COMMENT "Converting ${NAME} to KTX2..."
    )
    list(APPEND KTX_OUT ${OUT})
endforeach()

add_custom_target(Textures ALL DEPENDS ${KTX_OUT})
add_dependencies(Renderer Textures)

# 5. Copying All Assets to the Executable Directory
add_custom_command(TARGET Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/meshes"
        "$<TARGET_FILE_DIR:Renderer>/meshes"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        "$<TARGET_FILE_DIR:Renderer>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_BINARY_DIR}/textures"
        "$<TARGET_FILE_DIR:Renderer>/textures"
    COMMENT "Copying all assets (meshes, shaders, textures) to build directory..."
)
