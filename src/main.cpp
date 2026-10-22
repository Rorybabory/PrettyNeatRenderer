#include "resource.h"
#include "vulkan_renderer.h"
#include "asset_loader.h"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include <GLFW/glfw3.h>

MeshID mMonkeyMesh;
MeshID mCubeMesh;
MeshID mSphereMesh;
Texture mTestTexture;
Texture mTestTexture2;
Texture mTestTexture3;

Texture mGBufferColor;
Texture mGBufferDepth;

PipelineID mPipelineID;


Scene scene;

int main()
{
    GfxCreateScene(&scene);
    GfxInit();

    MeshLoadInfo meshLoadInfo = {
        .filename = "meshes/monkey.obj"};
    TextureLoadInfo texLoadInfo =
        {
            .filename = "textures/bricks.ktx2"};
    LoadTexture(&texLoadInfo, mTestTexture);
    texLoadInfo.filename = "textures/bricks2.ktx2";
    LoadTexture(&texLoadInfo, mTestTexture2);
    texLoadInfo.filename = "textures/bricks3.ktx2";
    LoadTexture(&texLoadInfo, mTestTexture3);

    RenderTargetCreateInfo rtCreateInfo = {
        .name = "GBuffer Color",
        .width = 1280,
        .height = 720,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .isDepth = false};
    CreateRenderTarget(&rtCreateInfo, mGBufferColor);
    rtCreateInfo.name = "GBuffer Depth";
    rtCreateInfo.isDepth = true;
    rtCreateInfo.format = VK_FORMAT_D32_SFLOAT;
    CreateRenderTarget(&rtCreateInfo, mGBufferDepth);

    mMonkeyMesh = LoadMesh(&meshLoadInfo);
    meshLoadInfo.filename = "meshes/cube.obj";
    mCubeMesh = LoadMesh(&meshLoadInfo);
    meshLoadInfo.filename = "meshes/sphere.obj";
    mSphereMesh = LoadMesh(&meshLoadInfo);

    GfxSetMesh(GetMesh(mMonkeyMesh));

    Texture textures[] = {mTestTexture, mTestTexture2, mTestTexture3};

    GfxUpdateTextures(textures, 3, 0);

    VkDescriptorSetLayout layouts[] =
        {
            GfxGetDescriptorSetLayoutTextures(),
            GfxGetDescriptorSetLayoutRenderables(),
            GfxGetDescriptorSetLayoutRenderableIndices(),
    };

    PipelineLoadInfo pipelineInfo =
        {
            .shaderFile = "shaders/test.spv",
            .descSetLayouts = layouts,
            .numLayouts = 3,
            .flags = PIPELINE_FLAG_GRAPHICS
        };
    mPipelineID = LoadPipeline(&pipelineInfo);


    GfxSetPipeline(GetPipeline(mPipelineID));

    int monkeysPerAxis = 4;
    float size = 0.1f;
    for (uint32_t i = 0; i < monkeysPerAxis; ++i)
    {
        for (uint32_t j = 0; j < monkeysPerAxis; ++j)
        {
            for (uint32_t k = 0; k < monkeysPerAxis; ++k)
            {
                float divisor = (float)monkeysPerAxis / 4.0f;
                MeshID mesh = mCubeMesh;
                uint16_t texture = 0;
                uint32_t checkerboard = (i + j + k) % 3;
                if (checkerboard == 0)
                {
                    mesh = mSphereMesh;
                    texture = 0;
                }
                else if (checkerboard == 1)
                {
                    mesh = mCubeMesh;
                    texture = 1;
                }
                else if (checkerboard == 2)
                {
                    mesh = mMonkeyMesh;
                    texture = 2;
                }
                // Adds a pretty cool monkey to the scene
                RenderableCreateInfo renderableMonkeyInfo =
                    {
                        .textureIDs = {
                            texture,
                        },
                        .transform = GfxGenerateModelMatrix(glm::vec3(-2.0f + ((float)i / divisor), -2.0f + ((float)j / divisor), -2.0f + ((float)k / divisor)), glm::vec3(size, size, size), glm::vec3(0.0f, 0.0f, 0.0f)),
                        .tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
                        .pipelineID = mPipelineID,
                        .meshID = mesh,
                };
                GfxSceneAddRenderable(scene, &renderableMonkeyInfo);
            }
        }
    }

    GfxUpdateScene(&scene);
    GfxSetRenderableData(&scene);
    GfxSortRenderables(scene);

    float dt = 0.0f;
    float lastTime = 0.0f;
    while (GfxUpdate())
    {
        dt = glfwGetTime() - lastTime;
        lastTime = glfwGetTime();
        glm::vec3 camPos = glm::vec3(cosf(glfwGetTime() * 0.25f) * 7.0f, 1.0f, sinf(glfwGetTime() * 0.25f) * 7.0f);
        GlobalData globalData;
        globalData.proj = glm::perspective(glm::radians(45.0f), GfxGetAspectRatio(), 0.1f, 32.0f);
        globalData.proj[1][1] *= -1;
        globalData.view = glm::lookAt(camPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        GfxSetGlobalData(globalData);

        GfxRender();
    }
    GfxPreDestroy();
    DestroyTexture(mTestTexture);
    DestroyTexture(mTestTexture2);
    DestroyTexture(mTestTexture3);
    DestroyTexture(mGBufferColor);
    DestroyTexture(mGBufferDepth);
    ClearPipelineRegistry();

    GfxDestroy();
    return 0;
}