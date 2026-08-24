#include "vulkan_renderer.h"
#include "asset_loader.h"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include <GLFW/glfw3.h>

Mesh mTestMesh;
Texture mTestTexture;
Pipeline mPipeline;

int main() {
    GfxInit();
    MeshLoadInfo meshLoadInfo = {
        .filename = "meshes/monkey.obj"
    };
    TextureLoadInfo texLoadInfo =
    {
        .filename = "textures/brick.ktx2"
    };
    LoadMesh(&meshLoadInfo, mTestMesh);
    LoadTexture(&texLoadInfo, mTestTexture);
    
    GfxCreateBindlessTextureDesc();
    GfxUpdateTextures(&mTestTexture, 1, 0);
    
    PipelineLoadInfo pipelineInfo = 
    {
        .shaderFile = "shaders/test.spv",
        .descSetLayout = GfxGetDescriptorSetLayout(),
    };
    LoadPipeline(&pipelineInfo, mPipeline);

    GfxSetPipeline(&mPipeline);
    GfxSetMesh(&mTestMesh);

    float dt = 0.0f;
    float lastTime = 0.0f;
    while (GfxUpdate())
    { 
        dt = glfwGetTime() - lastTime;
        lastTime = glfwGetTime();
        glm::vec3 camPos = glm::vec3(cosf(glfwGetTime()*0.25f)*10.0f, 1.0f, sinf(glfwGetTime()*0.25f)*10.0f);
        ShaderData shaderData;
        shaderData.proj = glm::perspective(glm::radians(45.0f), GfxGetAspectRatio(), 0.1f, 32.0f);
        shaderData.proj[1][1] *= -1;
        shaderData.view = glm::lookAt(camPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        for (auto i = 0; i < 3; i++) {
            glm::vec3 instancePos = glm::vec3((float)(i - 1) * 3.0f, 0.0f, 0.0f);
            shaderData.model[i] = glm::translate(glm::mat4(1.0f), instancePos);
        }
        GfxSetShaderData(shaderData);

        GfxRender();
    }
    GfxPreDestroy();
    DestroyTexture(mTestTexture);
    DestroyPipeline(mPipeline);
    
    GfxDestroy();
    return 0;
}