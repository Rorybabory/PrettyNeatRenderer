#include "asset_loader.h"
#include "resource.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <cstddef>
#include <cstring>
#include <ktx.h>
#include <vk_mem_alloc.h>
#include "volk.h"

#include <spdlog/spdlog.h>
#include "volk.h"
#include "vkutils.h"

#include <ktxvulkan.h>
#include <vulkan/vulkan_core.h>

#include "vulkan_renderer.h"

const uint32_t SCRATCH_BUFFER_SIZE = 1 << 24;
const uint32_t MESH_BUFFER_SIZE = 1 << 26;

extern VmaAllocator allocator;
extern VkDevice device;
extern VkCommandPool commandPool;
extern VkQueue graphicsQueue;

VkBuffer gpuScratchBuffer = NULL;
VmaAllocation gpuScratchBufferAlloc = NULL;
VmaAllocationInfo gpuScratchBufferAllocInfo;
void *gpuScratchBufferMapped = NULL;

VkBuffer meshVertexBuffer;
VmaAllocation meshVertexBufferAlloc;
VmaAllocationInfo meshVertexBufferAllocInfo{};
VkDeviceSize meshVertexBufferSize = 0;

VkBuffer meshIndexBuffer;
VmaAllocation meshIndexBufferAlloc;
VmaAllocationInfo meshIndexBufferAllocInfo{};
VkDeviceSize meshIndexBufferSize = 0;

Pipeline pipelineRegistry[MAX_PIPELINES];
uint32_t pipelineRegistryCount = 0;
Mesh meshRegistry[MAX_MESHES];
uint32_t meshRegistryCount = 0;

const uint32_t MAX_VERTICES = 1 << 18;
const uint32_t MAX_INDICES = 1 << 18;
const uint64_t MAX_SHADER_SIZE = 1 << 20;

Vertex *cpuScratchVertices = NULL;
uint32_t *cpuScratchIndices = NULL;

char *scratchShaderSrc = NULL;

VkBuffer *GetMeshVertexBuffer()
{
    return &meshVertexBuffer;
}

VkBuffer *GetMeshIndexBuffer()
{
    return &meshIndexBuffer;
}

uint32_t GetNumMeshes()
{
    return meshRegistryCount;
}

void AssetLoaderInit()
{
    VkBufferCreateInfo scratchBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = SCRATCH_BUFFER_SIZE,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };
    VmaAllocationCreateInfo scratchBufferAllocInfo = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO};
    VK_CHECK(vmaCreateBuffer(
        allocator,
        &scratchBufferInfo,
        &scratchBufferAllocInfo,
        &gpuScratchBuffer,
        &gpuScratchBufferAlloc,
        &gpuScratchBufferAllocInfo));

    vmaSetAllocationName(allocator, gpuScratchBufferAlloc, "GPU Scratch Buffer");

    cpuScratchVertices = (Vertex *)malloc(sizeof(Vertex) * MAX_VERTICES);
    cpuScratchIndices = (uint32_t *)malloc(sizeof(uint32_t) * MAX_INDICES);
    scratchShaderSrc = (char *)malloc(sizeof(char) * MAX_SHADER_SIZE);

    VkBufferCreateInfo meshBufferInfo =
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = MESH_BUFFER_SIZE,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};

    VmaAllocationCreateInfo meshBufferAllocCreateInfo{
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY};
    VK_CHECK(vmaCreateBuffer(allocator, &meshBufferInfo, &meshBufferAllocCreateInfo, &meshVertexBuffer, &meshVertexBufferAlloc, &meshVertexBufferAllocInfo));
    vmaSetAllocationName(allocator, meshVertexBufferAlloc, "Mesh Vertex Buffer");

    VkBufferCreateInfo indexBufferInfo =
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = MESH_BUFFER_SIZE,
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};

    VmaAllocationCreateInfo indexBufferAllocCreateInfo{
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY};
    VK_CHECK(vmaCreateBuffer(allocator, &indexBufferInfo, &indexBufferAllocCreateInfo, &meshIndexBuffer, &meshIndexBufferAlloc, &meshIndexBufferAllocInfo));
    vmaSetAllocationName(allocator, meshIndexBufferAlloc, "Mesh Index Buffer");
}

MeshID LoadMesh(MeshLoadInfo *loadInfo)
{
    Mesh mesh = {};
    const aiScene *scene = aiImportFile(loadInfo->filename,
                                        aiProcess_CalcTangentSpace |
                                            aiProcess_Triangulate |
                                            aiProcess_SortByPType);
    if (!scene)
    {
        spdlog::error("AssetLoader: {}", aiGetErrorString());
        return -1;
    }
    if (!scene->HasMeshes())
    {
        spdlog::error("AssetLoader: {} does not contain any meshes!", loadInfo->filename);
        return -1;
    }

    mesh.bbMin = glm::vec3(10000000.0f, 10000000.0f, 100000000.0f);
    mesh.bbMax = glm::vec3(-10000000.0f, -10000000.0f, -100000000.0f);

    aiMesh *assimpMesh = scene->mMeshes[0];

    for (uint32_t i = 0; i < assimpMesh->mNumVertices; ++i)
    {
        cpuScratchVertices[i].mPos = {assimpMesh->mVertices[i].x, assimpMesh->mVertices[i].y, assimpMesh->mVertices[i].z};
        cpuScratchVertices[i].mNormal = {assimpMesh->mNormals[i].x, assimpMesh->mNormals[i].y, assimpMesh->mNormals[i].z};
        cpuScratchVertices[i].mUv = {assimpMesh->mTextureCoords[0][i].x, assimpMesh->mTextureCoords[0][i].y};

        mesh.bbMin = glm::min(cpuScratchVertices[i].mPos, mesh.bbMin);
        mesh.bbMax = glm::max(cpuScratchVertices[i].mPos, mesh.bbMax);
    }
    for (uint32_t i = 0; i < assimpMesh->mNumFaces; i++)
    {
        cpuScratchIndices[i * 3 + 0] = assimpMesh->mFaces[i].mIndices[0];
        cpuScratchIndices[i * 3 + 1] = assimpMesh->mFaces[i].mIndices[1];
        cpuScratchIndices[i * 3 + 2] = assimpMesh->mFaces[i].mIndices[2];
    }
    mesh.indexCount = assimpMesh->mNumFaces * 3;

    VkDeviceSize vertexSize = assimpMesh->mNumVertices * sizeof(Vertex);
    VkDeviceSize indexSize = mesh.indexCount * sizeof(uint32_t);
    aiReleaseImport(scene);

    // Copy CPU Scratch Vertex and Index buffers to Mapped GPU Memory
    memcpy(((char *)gpuScratchBufferAllocInfo.pMappedData), cpuScratchVertices, vertexSize);
    memcpy(((char *)gpuScratchBufferAllocInfo.pMappedData) + vertexSize, cpuScratchIndices, indexSize);
    mesh.vertexOffset = meshVertexBufferSize / sizeof(Vertex);
    mesh.firstIndex = meshIndexBufferSize / sizeof(uint32_t);

    // Allocate a transfer command buffer
    VkCommandBuffer copyCmdBuffer = GfxCreateOneTimeCmdBuffer();

    // Record Buffer Transfer Command
    VkCommandBufferBeginInfo beginInfo =
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VK_CHECK(vkBeginCommandBuffer(copyCmdBuffer, &beginInfo));

    VkBufferCopy vertexBufferCopy = {
        .srcOffset = 0,
        .dstOffset = meshVertexBufferSize,
        .size = vertexSize};
    VkBufferCopy indexBufferCopy = {
        .srcOffset = vertexSize,
        .dstOffset = meshIndexBufferSize,
        .size = indexSize};
    vkCmdCopyBuffer(copyCmdBuffer, gpuScratchBuffer, meshVertexBuffer, 1, &vertexBufferCopy);
    vkCmdCopyBuffer(copyCmdBuffer, gpuScratchBuffer, meshIndexBuffer, 1, &indexBufferCopy);
    vkEndCommandBuffer(copyCmdBuffer);

    // Submit to our graphics queue
    VkQueue queue = GfxGetQueue();
    VkSubmitInfo submitInfo =
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &copyCmdBuffer};

    vkQueueSubmit(queue, 1, &submitInfo, nullptr);

    meshVertexBufferSize += vertexSize;
    meshIndexBufferSize += indexSize;
    meshRegistry[meshRegistryCount++] = mesh;
    return (MeshID)(meshRegistryCount - 1);
}

Mesh *GetMesh(MeshID id)
{
    return &meshRegistry[id];
}

bool CreateRenderTarget(RenderTargetCreateInfo *createInfo, Texture &texture)
{
    // Create Image
    VkImageCreateInfo texImgInfo =
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = (VkFormat)createInfo->format,
            .extent = {.width = createInfo->width, .height = createInfo->height, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL};
    VmaAllocationCreateInfo allocCreateInfo =
        {
            .usage = VMA_MEMORY_USAGE_AUTO};
    VK_CHECK(vmaCreateImage(allocator, &texImgInfo, &allocCreateInfo, &texture.image, &texture.allocation, nullptr));
    vmaSetAllocationName(allocator, texture.allocation, createInfo->name);

    VkSamplerCreateInfo samplerCreateInfo =
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = 8.0f,
            .maxLod = 1.0f};
    VK_CHECK(vkCreateSampler(device, &samplerCreateInfo, nullptr, &texture.sampler));

    VkImageViewCreateInfo viewInfo =
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = texture.image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = texImgInfo.format,
            .subresourceRange =
                {
                    .aspectMask = static_cast<VkImageAspectFlags>(createInfo->isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
                    .levelCount = 1,
                    .layerCount = 1}};
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &texture.imageView));

    texture.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    texture.format = texImgInfo.format;
    texture.layerCount = 1;
}

bool LoadTexture(TextureLoadInfo *loadInfo, Texture &texture)
{

    // Load Texture
    ktxTexture *ktxTexture{nullptr};
    ktxTexture_CreateFromNamedFile(loadInfo->filename, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);

    if (ktxTexture == NULL)
    {
        spdlog::error("AssetLoader: Failed to load: {}", loadInfo->filename);
        return false;
    }

    // Create Image
    VkImageCreateInfo texImgInfo =
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = ktxTexture_GetVkFormat(ktxTexture),
            .extent = {.width = ktxTexture->baseWidth, .height = ktxTexture->baseHeight, .depth = 1},
            .mipLevels = ktxTexture->numLevels,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    VmaAllocationCreateInfo allocCreateInfo =
        {
            .usage = VMA_MEMORY_USAGE_AUTO};
    VK_CHECK(vmaCreateImage(allocator, &texImgInfo, &allocCreateInfo, &texture.image, &texture.allocation, nullptr));
    vmaSetAllocationName(allocator, texture.allocation, loadInfo->filename);

    // Load Image Data to Buffer
    memcpy(gpuScratchBufferAllocInfo.pMappedData, ktxTexture->pData, ktxTexture->dataSize);

    // Create Fence and Command Buffer for copying data from the buffer to the image
    VkFenceCreateInfo fenceOneTimeInfo = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fenceOneTime = NULL;
    VK_CHECK(vkCreateFence(device, &fenceOneTimeInfo, nullptr, &fenceOneTime));
    VkCommandBuffer cbOneTime = {};
    VkCommandBufferAllocateInfo cbOneTimeAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .commandBufferCount = 1};
    VK_CHECK(vkAllocateCommandBuffers(device, &cbOneTimeAllocInfo, &cbOneTime));

    // Copy buffer to image with proper barriers
    VkCommandBufferBeginInfo cbOneTimeBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VK_CHECK(vkBeginCommandBuffer(cbOneTime, &cbOneTimeBeginInfo));
    VkImageMemoryBarrier2 barrierTexImage =
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = texture.image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = ktxTexture->numLevels,
                .layerCount = 1}};
    VkDependencyInfo barrierTexInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrierTexImage};
    vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
    VkBufferImageCopy *copyRegions = (VkBufferImageCopy *)malloc(sizeof(VkBufferImageCopy) * ktxTexture->numLevels);

    for (uint32_t i = 0; i < ktxTexture->numLevels; ++i)
    {
        ktx_size_t mipOffset = 0;
        ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &mipOffset);
        copyRegions[i] = {
            .bufferOffset = mipOffset,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = i,
                .layerCount = 1},
            .imageExtent = {.width = ktxTexture->baseWidth >> i, .height = ktxTexture->baseHeight >> i, .depth = 1}};
    }
    vkCmdCopyBufferToImage(
        cbOneTime,
        gpuScratchBuffer,
        texture.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<uint32_t>(ktxTexture->numLevels),
        copyRegions);

    VkImageMemoryBarrier2 barrierTexRead =
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            .image = texture.image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = ktxTexture->numLevels,
                .layerCount = 1}};
    barrierTexInfo.pImageMemoryBarriers = &barrierTexRead;
    vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
    VK_CHECK(vkEndCommandBuffer(cbOneTime));

    VkSubmitInfo oneTimeSubmitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cbOneTime};
    vkQueueSubmit(graphicsQueue, 1, &oneTimeSubmitInfo, fenceOneTime);
    vkWaitForFences(device, 1, &fenceOneTime, VK_TRUE, UINT64_MAX);

    VkSamplerCreateInfo samplerCreateInfo =
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = 8.0f,
            .maxLod = (float)ktxTexture->numLevels};
    VK_CHECK(vkCreateSampler(device, &samplerCreateInfo, nullptr, &texture.sampler));

    VkImageViewCreateInfo viewInfo =
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = texture.image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = texImgInfo.format,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .levelCount = 1,
                    .layerCount = 1}};
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &texture.imageView));

    texture.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    texture.format = texImgInfo.format;
    texture.layerCount = ktxTexture->numLayers;
    ktxTexture_Destroy(ktxTexture);
    vkDestroyFence(device, fenceOneTime, nullptr);
    return true;
}
void DestroyTexture(Texture &texture)
{
    vmaDestroyImage(allocator, texture.image, texture.allocation);
    vkDestroySampler(device, texture.sampler, nullptr);
    vkDestroyImageView(device, texture.imageView, nullptr);
}
Pipeline LoadComputePipeline(PipelineLoadInfo *pipelineInfo, VkShaderModule shaderModule)
{
    Pipeline pipeline;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo =
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = pipelineInfo->numLayouts,
            .pSetLayouts = pipelineInfo->descSetLayouts};
    VkPushConstantRange range = {};
    if (pipelineInfo->pushConstantSize > 0)
    {
        range = {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, 
            .offset = 0,
            .size = pipelineInfo->pushConstantSize,
        };
        pipelineLayoutInfo.pPushConstantRanges = &range;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
    }
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipeline.layout));

    VkComputePipelineCreateInfo pipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shaderModule,
            .pName = "main",
        },
        .layout = pipeline.layout 
    };
    vkCreateComputePipelines(device, nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline.pipeline);
    return pipeline;
}
Pipeline LoadGraphicsPipeline(PipelineLoadInfo *pipelineInfo, VkShaderModule shaderModule)
{
    Pipeline pipeline;
    VkPushConstantRange pushConstantRange =
        {
            .stageFlags = VK_SHADER_STAGE_ALL,
            .size = sizeof(VkDeviceAddress)};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo =
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = pipelineInfo->numLayouts,
            .pSetLayouts = pipelineInfo->descSetLayouts,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange

    };
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipeline.layout));

    VkVertexInputBindingDescription vertexBinding =
        {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};

    VkVertexInputAttributeDescription vertexAttributes[3] =
        {
            {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT},
            {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, mNormal)},
            {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, mUv)},
    };

    VkPipelineVertexInputStateCreateInfo vertexInputState =
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &vertexBinding,
            .vertexAttributeDescriptionCount = 3,
            .pVertexAttributeDescriptions = vertexAttributes};
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
    VkDescriptorSetAndBindingMappingEXT *bindingMappings = NULL;
    uint32_t numBindings = 0;
    GfxGetBindingMappings(&bindingMappings, &numBindings);

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo =
        {
            .sType = VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT,
            .mappingCount = numBindings,
            .pMappings = bindingMappings,
    };
#endif

    VkPipelineShaderStageCreateInfo shaderStages[] =
        {
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
             .pNext = &mappingInfo,
#endif
             .stage = VK_SHADER_STAGE_VERTEX_BIT,
             .module = shaderModule,
             .pName = "main"},
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
             .pNext = &mappingInfo,
#endif
             .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
             .module = shaderModule,
             .pName = "main"}};

    VkPipelineViewportStateCreateInfo viewportState =
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1};
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState =
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2,
            .pDynamicStates = dynamicStates};
    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL};
    VkFormat colorAttachmentFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkPipelineRenderingCreateInfo renderingCreateInfo =
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &colorAttachmentFormat,
            .depthAttachmentFormat = GfxGetDepthFormat()};
    VkPipelineColorBlendAttachmentState blendAttachment{
        .colorWriteMask = 0xF};
    VkPipelineColorBlendStateCreateInfo colorBlendState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment};
    VkPipelineRasterizationStateCreateInfo rasterizationState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .lineWidth = 1.0f,
    };
    VkPipelineMultisampleStateCreateInfo multisampleState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineCreateFlags2CreateInfo pipelineCreateFlagsInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO,
        .pNext = &renderingCreateInfo,
#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
        .flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT
#endif
    };
    VkGraphicsPipelineCreateInfo pipelineCreateInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
        .pNext = &pipelineCreateFlagsInfo,
#else
        .pNext = &renderingCreateInfo,
#endif
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputState,
        .pInputAssemblyState = &inputAssemblyState,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizationState,
        .pMultisampleState = &multisampleState,
        .pDepthStencilState = &depthStencilState,
        .pColorBlendState = &colorBlendState,
        .pDynamicState = &dynamicState,
#ifndef DESCRIPTOR_HEAP_EXPERIMENTS
        .layout = pipeline.layout
#endif
    };
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline.pipeline));
    return pipeline;
}

PipelineID LoadPipeline(PipelineLoadInfo *pipelineInfo)
{
    Pipeline pipeline;

    FILE *spvFile = fopen(pipelineInfo->shaderFile, "rb");
    if (!spvFile)
    {
        spdlog::error("Failed to load shader: {}", pipelineInfo->shaderFile);
        return -1;
    }
    fseek(spvFile, 0, SEEK_END);
    size_t fileLength = ftell(spvFile);
    rewind(spvFile);
    if (fileLength > MAX_SHADER_SIZE)
    {
        spdlog::error("Shader is to large, increase max shader size: {}", pipelineInfo->shaderFile);
        return -1;
    }
    fread(scratchShaderSrc, 1, fileLength, spvFile);
    fclose(spvFile);

    VkShaderModuleCreateInfo smInfo =
        {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = fileLength,
            .pCode = (const uint32_t *)scratchShaderSrc};
    VkShaderModule shaderModule = {};
    VK_CHECK(vkCreateShaderModule(device, &smInfo, nullptr, &shaderModule));

    if (pipelineInfo->flags & PIPELINE_FLAG_GRAPHICS)
    {
        pipeline = LoadGraphicsPipeline(pipelineInfo, shaderModule);
    }
    else if (pipelineInfo->flags & PIPELINE_FLAG_COMPUTE)
    {
        pipeline = LoadComputePipeline(pipelineInfo, shaderModule);
    }else {
        
    }

    vkDestroyShaderModule(device, shaderModule, nullptr);

    pipelineRegistry[pipelineRegistryCount++] = pipeline;
    return (PipelineID)(pipelineRegistryCount - 1);
}

Pipeline *GetPipeline(PipelineID id)
{
    return &pipelineRegistry[id];
}

void ClearPipelineRegistry()
{
    for (uint32_t i = 0; i < pipelineRegistryCount; ++i)
    {
        vkDestroyPipeline(device, pipelineRegistry[i].pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineRegistry[i].layout, nullptr);
    }
}

void AssetLoaderDestroy()
{
    free(scratchShaderSrc);
    free(cpuScratchVertices);
    free(cpuScratchIndices);
    vmaDestroyBuffer(allocator, gpuScratchBuffer, gpuScratchBufferAlloc);
    vmaDestroyBuffer(allocator, meshVertexBuffer, meshVertexBufferAlloc);
    vmaDestroyBuffer(allocator, meshIndexBuffer, meshIndexBufferAlloc);
}