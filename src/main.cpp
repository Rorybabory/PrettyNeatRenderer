#include "resource.h"
#include "vulkan_renderer.h"
#include "asset_loader.h"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include <GLFW/glfw3.h>

MeshID mMonkeyMesh;
MeshID mCubeMesh;
MeshID mPlaneMesh;
Texture mTestTexture;
PipelineID mPipelineID;

Scene scene;

int main() {
    GfxInit();
    MeshLoadInfo meshLoadInfo = {
        .filename = "meshes/monkey.obj"
    };
    TextureLoadInfo texLoadInfo =
    {
        .filename = "textures/brick.ktx2"
    };
    mMonkeyMesh = LoadMesh(&meshLoadInfo);
    meshLoadInfo.filename = "meshes/cube.obj";
    mCubeMesh = LoadMesh(&meshLoadInfo);
    meshLoadInfo.filename = "meshes/plane.obj";
    mPlaneMesh = LoadMesh(&meshLoadInfo);

    
    LoadTexture(&texLoadInfo, mTestTexture);

    GfxSetMesh(GetMesh(mMonkeyMesh));
    
    GfxUpdateTextures(&mTestTexture, 1, 0);
    
    VkDescriptorSetLayout layouts[] = 
    {
        GfxGetDescriptorSetLayoutTextures(),
        GfxGetDescriptorSetLayoutRenderables(),
    };

    PipelineLoadInfo pipelineInfo = 
    {
        .shaderFile = "shaders/test.spv",
        .descSetLayouts = layouts,
        .numLayouts = 2
    };
    mPipelineID = LoadPipeline(&pipelineInfo);
    GfxSetPipeline(GetPipeline(mPipelineID));

    //Adds a pretty cool monkey to the scene
    RenderableCreateInfo renderableMonkeyInfo =
    {
        .textureIDs = {
            0,
        },
        .transform = GfxGenerateModelMatrix(glm::vec3(-3.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f)),
        .tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
        .pipelineID = mPipelineID,
        .meshID = mMonkeyMesh,
    };
    GfxSceneAddRenderable(scene, &renderableMonkeyInfo);

    //Adds another pretty cool monkey to the scene
    renderableMonkeyInfo =
    {
        .textureIDs = {
            0,
        },
        .transform = GfxGenerateModelMatrix(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 35.0f, 0.0f)),
        .tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
        .pipelineID = mPipelineID,
        .meshID = mMonkeyMesh,
    };
    GfxSceneAddRenderable(scene, &renderableMonkeyInfo);

    //Wow more monkeyz
    renderableMonkeyInfo =
    {
        .textureIDs = {
            0,
        },
        .transform = GfxGenerateModelMatrix(glm::vec3(3.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, -30.0f, 0.0f)),
        .tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
        .pipelineID = mPipelineID,
        .meshID = mMonkeyMesh,
    };
    GfxSceneAddRenderable(scene, &renderableMonkeyInfo);

    GfxUpdateScene(&scene);


    float dt = 0.0f;
    float lastTime = 0.0f;
    while (GfxUpdate())
    { 
        dt = glfwGetTime() - lastTime;
        lastTime = glfwGetTime();
        glm::vec3 camPos = glm::vec3(cosf(glfwGetTime()*0.25f)*10.0f, 1.0f, sinf(glfwGetTime()*0.25f)*10.0f);
        GlobalData globalData;
        globalData.proj = glm::perspective(glm::radians(45.0f), GfxGetAspectRatio(), 0.1f, 32.0f);
        globalData.proj[1][1] *= -1;
        globalData.view = glm::lookAt(camPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        GfxSetGlobalData(globalData);

        GfxSetRenderableData(&scene);

        GfxRender();
    }
    GfxPreDestroy();
    DestroyTexture(mTestTexture);
    ClearPipelineRegistry();
    
    GfxDestroy();
    return 0;
}