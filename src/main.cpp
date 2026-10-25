#include "resource.h"
#include "vulkan_renderer.h"
#include "asset_loader.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include <glm/gtx/euler_angles.hpp>

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

struct FlyCamera {
    glm::vec3 pos;
    glm::vec3 rotation;
    glm::vec3 forward;
};

FlyCamera mCamera;
glm::vec2 mLastMouse;
bool mFirstMouse = true;

float deltaTime = 0.0f;

const float cameraSpeed = 0.5f;
const float mouseSensitivity = 0.1f;
void MouseCallback(GLFWwindow* window, double xpos, double ypos)
{
    
    glm::vec2 mousePos = glm::vec2((float)xpos, (float)ypos);
    if (mFirstMouse)
    {
        mLastMouse = mousePos;
        mFirstMouse = false;
        return;
    }
    glm::vec2 delta = mLastMouse - mousePos;

    mCamera.rotation.y -= delta.x * mouseSensitivity;
    mCamera.rotation.x += delta.y * mouseSensitivity;

    if(mCamera.rotation.x > 89.0f)
        mCamera.rotation.x = 89.0f;
    if(mCamera.rotation.x < -89.0f)
        mCamera.rotation.x = -89.0f;

    mCamera.forward.x = cos(glm::radians(mCamera.rotation.y)) * cos(glm::radians(mCamera.rotation.x));
    mCamera.forward.y = sin(glm::radians(mCamera.rotation.x));
    mCamera.forward.z = sin(glm::radians(mCamera.rotation.y)) * cos(glm::radians(mCamera.rotation.x));

    mLastMouse = mousePos;
}

void GenerateScene()
{
    const int monkeysPerAxis = 64;
    const float size = 0.02f;
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
}

int main()
{
    GfxCreateScene(&scene);
    GfxInit();

    MeshLoadInfo meshLoadInfo = {};
    TextureLoadInfo texLoadInfo =
    {
        .filename = "textures/bricks.ktx2"
    };
    LoadTexture(&texLoadInfo, mTestTexture);
    texLoadInfo.filename = "textures/bricks2.ktx2";
    LoadTexture(&texLoadInfo, mTestTexture2);
    texLoadInfo.filename = "textures/bricks3.ktx2";
    LoadTexture(&texLoadInfo, mTestTexture3);

    RenderTargetCreateInfo rtCreateInfo = 
    {
        .name = "GBuffer Color",
        .width = 1280,
        .height = 720,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .isDepth = false
    };
    CreateRenderTarget(&rtCreateInfo, mGBufferColor);
    rtCreateInfo.name = "GBuffer Depth";
    rtCreateInfo.isDepth = true;
    rtCreateInfo.format = VK_FORMAT_D32_SFLOAT;
    CreateRenderTarget(&rtCreateInfo, mGBufferDepth);

    meshLoadInfo.filename = "meshes/monkey_lod_1.obj";
    MeshID mMonkeyMeshLod1 = LoadMesh(&meshLoadInfo);
    meshLoadInfo.filename = "meshes/monkey_lod_2.obj";
    MeshID mMonkeyMeshLod2 = LoadMesh(&meshLoadInfo);
    meshLoadInfo.filename = "meshes/monkey_lod_3.obj";
    MeshID mMonkeyMeshLod3 = LoadMesh(&meshLoadInfo);
    meshLoadInfo.filename = "meshes/monkey_lod_4.obj";
    MeshID mMonkeyMeshLod4 = LoadMesh(&meshLoadInfo);
    MeshID mMonkeyLods[] = { mMonkeyMeshLod1, mMonkeyMeshLod2, mMonkeyMeshLod3, mMonkeyMeshLod4 };

    meshLoadInfo.filename = "meshes/monkey.obj";
    meshLoadInfo.numLods = 4;
    meshLoadInfo.pLods = mMonkeyLods;
    mMonkeyMesh = LoadMesh(&meshLoadInfo);
    meshLoadInfo.numLods = 0;

    meshLoadInfo.filename = "meshes/cube.obj";
    mCubeMesh = LoadMesh(&meshLoadInfo);


    meshLoadInfo.filename = "meshes/sphere_lod_1.obj";
    MeshID mSphereMeshLod1 = LoadMesh(&meshLoadInfo);
    meshLoadInfo.filename = "meshes/sphere_lod_2.obj";
    MeshID mSphereMeshLod2 = LoadMesh(&meshLoadInfo);
    meshLoadInfo.filename = "meshes/sphere_lod_3.obj";
    MeshID mSphereMeshLod3 = LoadMesh(&meshLoadInfo);
    meshLoadInfo.filename = "meshes/sphere_lod_4.obj";
    MeshID mSphereMeshLod4 = LoadMesh(&meshLoadInfo);
    
    MeshID mSphereLods[] = { mSphereMeshLod1, mSphereMeshLod2, mSphereMeshLod3, mSphereMeshLod4 };
    meshLoadInfo.filename = "meshes/sphere.obj";
    meshLoadInfo.numLods = 4;
    meshLoadInfo.pLods = mSphereLods;
    mSphereMesh = LoadMesh(&meshLoadInfo);


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
            .shaderFile = "shaders/matGeneric.spv",
            .descSetLayouts = layouts,
            .numLayouts = 3,
            .flags = PIPELINE_FLAG_GRAPHICS
        };
    mPipelineID = LoadPipeline(&pipelineInfo);
    GfxSetPipeline(GetPipeline(mPipelineID));

    GenerateScene();

    GfxUpdateScene(&scene);
    GfxSetRenderableData(&scene);
    

    mCamera = {
        .pos = glm::vec3(0.0f, 0.0f, 0.0f),
        .rotation = glm::vec3(0.0f, 0.0f, 0.0f),
        .forward = glm::vec3(1.0f, 0.0f, 0.0f)
    };

    glfwSetCursorPosCallback(GfxGetWindow(), MouseCallback);

    float lastTime = 0.0f;
    while (GfxUpdate())
    {
        deltaTime = glfwGetTime() - lastTime;
        lastTime = glfwGetTime();

        GLFWwindow * window = GfxGetWindow();
        glm::vec3 right   = glm::normalize(glm::cross(mCamera.forward, glm::vec3(0.0f, 1.0f, 0.0f))); 
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
            mCamera.pos += mCamera.forward * deltaTime * cameraSpeed;
        }else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
            mCamera.pos -= mCamera.forward * deltaTime * cameraSpeed;
        }

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        {
            mCamera.pos -= right * deltaTime * cameraSpeed;
        }else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        {
            mCamera.pos += right * deltaTime * cameraSpeed;
        }

        bool updateFrustum = !(glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);

        glm::mat4 cameraView = glm::lookAt(mCamera.pos, mCamera.pos + mCamera.forward, glm::vec3(0.0f, 1.0f, 0.0f));


        GfxWaitForFrame();
        GfxSetCameraData(cameraView, mCamera.pos, 80.0f, 0.002f, 64.0f, updateFrustum);
        GfxSortRenderables(scene);
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