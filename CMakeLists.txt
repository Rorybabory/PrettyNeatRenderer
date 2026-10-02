cmake_minimum_required(VERSION 3.20)
project(Renderer)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(assimp REQUIRED)
find_package(Ktx REQUIRED)


add_custom_target(Shaders ALL DEPENDS ${SHADER_PRODUCTS})

add_executable(Renderer src/main.cpp src/vulkan_renderer.cpp src/vma_impl.cpp src/asset_loader.cpp
    src/vkutils.c src/volk.c src/spirv_reflect.c)

add_custom_command(TARGET Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/meshes"
        "$<TARGET_FILE_DIR:Renderer>/meshes"
    COMMENT "Copying Meshes folder to build directory..."
)

add_custom_target(
    always_run_script ALL
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/buildshaders.sh"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Building shaders..."
    VERBATIM
)
target_compile_definitions(Renderer PRIVATE 
    VMA_STATIC_VULKAN_FUNCTIONS=0 
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
)

file(GLOB TEXTURES "textures/*.png")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/textures")

foreach(IMG IN LISTS TEXTURES)
    get_filename_component(NAME ${IMG} NAME_WE)
    set(OUT "${CMAKE_BINARY_DIR}/textures/${NAME}.ktx2")
    
    # Using ktx create and padding to the GPU-friendly 4-channel format
    add_custom_command(
        OUTPUT ${OUT} 
        COMMAND ktx create --format VK_FORMAT_R8G8B8A8_SRGB ${IMG} ${OUT} 
        DEPENDS ${IMG}
    )
    list(APPEND KTX_OUT ${OUT})
endforeach()

add_custom_target(Textures ALL DEPENDS ${KTX_OUT})

include_directories(include)
add_dependencies(Renderer Shaders)


target_link_libraries(Renderer PRIVATE Vulkan::Vulkan)
target_link_libraries(Renderer PRIVATE glfw)
target_link_libraries(Renderer PRIVATE GPUOpen::VulkanMemoryAllocator)
target_link_libraries(Renderer PRIVATE assimp)
target_link_libraries(Renderer PRIVATE ktx)
