// Copyright 2017 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>

#include "dawn/samples/SampleUtils.h"

#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/Timer.h"
#include "dawn/utils/WGPUHelpers.h"

const int MAX_FRAME_COUNT = 256;
const int MAX_FRAME_TIME = 1000; // ms

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

struct Uniform {
    alignas(16) glm::mat4 modelViewProjection;
    alignas(16) glm::mat4 normal;
    alignas(16) glm::vec4 materialDiffuse;
    alignas(16) glm::mat4 modelView;
};

// 函数用于读取顶点数据文件（文本格式，逗号分隔）
std::pair<std::vector<float>, size_t> loadVertexDataFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open vertex data file: " << filename << std::endl;
        return {{}, 0};
    }
    
    std::vector<float> vertexData;
    std::string line;
    
    // 读取文件的所有内容
    std::string content;
    while (std::getline(file, line)) {
        if (!content.empty()) {
            content += " ";
        }
        content += line;
    }
    file.close();
    
    // 解析逗号分隔的数值
    std::stringstream ss(content);
    std::string token;
    
    while (std::getline(ss, token, ',')) {
        // 移除前后空白字符
        token.erase(0, token.find_first_not_of(" \t\n\r"));
        token.erase(token.find_last_not_of(" \t\n\r") + 1);
        
        if (!token.empty()) {
            try {
                float value = std::stof(token);
                vertexData.push_back(value);
            } catch (const std::exception& e) {
                std::cerr << "Error parsing float value: " << token << " - " << e.what() << std::endl;
            }
        }
    }
    
    // 每个顶点有6个float（3个position + 3个normal）
    size_t vertexCount = vertexData.size() / 6;
    std::cout << "Loaded " << vertexCount << " vertices (" << vertexData.size() << " floats) from " << filename << std::endl;
    
    return {vertexData, vertexCount};
}

class ShadingScene : public SampleBase {
public:
    using SampleBase::SampleBase;
    
    // 添加FPS数据收集接口
    std::vector<float> GetFPSSamples() const { return fpsSamples; }
    void ClearFPSSamples() { fpsSamples.clear(); }
    bool HasEnoughSamples(int targetCount) const { return fpsSamples.size() >= targetCount; }
    
private:
    bool SetupImpl() override {
        startTime = std::chrono::steady_clock::now();

        wgpu::SurfaceTexture surfaceTexture;
        surface.GetCurrentTexture(&surfaceTexture);
        width = surfaceTexture.texture.GetWidth();
        height = surfaceTexture.texture.GetHeight();
        aspect = static_cast<float>(width) / static_cast<float>(height);

        glm::vec3 min_bound = {-2.23317, -1.34113, -1.28322};
        glm::vec3 max_bound = {2.25217, 1.35304, 1.24491};
        float diameter = glm::distance(min_bound, max_bound);
        radius = diameter / 2.0f;
        center = (max_bound + min_bound) / 2.0f;
        float fovy = 2.0f * atanf(radius / (2.0f + radius));
        projection = glm::perspective(fovy, aspect, 2.0f, 2.0f + diameter);

        wgpu::ShaderModule Module = dawn::utils::CreateShaderModule(device, R"(
            struct vsIn {
                @location(0) position : vec3f,
                @location(1) normal : vec3f,
            }

            struct Uniform {
                ModelViewProjectionMatrix : mat4x4f,
                NormalMatrix : mat4x4f,
                MaterialDiffuse : vec4f,
                ModelViewMatrix : mat4x4f,
            }

            struct vsOutput {
                @builtin(position) position : vec4f,
                @location(0) color : vec4f,
            }

            @group(0) @binding(0) var<uniform> uni : Uniform;

            @vertex fn vs(vert : vsIn) -> vsOutput
            {
                const LightSourcePosition : vec4f = vec4f(20.0, -20.0, 10.0, 1.0);
                var N : vec3f = normalize((uni.NormalMatrix * vec4f(vert.normal, 1.0)).xyz);
                var L : vec3f = normalize(LightSourcePosition.xyz);
                var diffuse : f32 = max(dot(N, L), 0.0);
                var position : vec4f = uni.ModelViewProjectionMatrix * vec4f(vert.position, 1.0);
                var vsOut : vsOutput;
                vsOut.position = position;
                vsOut.position.y *= -1.0;
                // vsOut.position.z *= 0.5;
                // vsOut.position.z = (vsOut.position.z + 1.0) / 2.0;
                // 移除Y坐标翻转，保持正常的3D坐标系
                vsOut.position.z = (vsOut.position.z + vsOut.position.w) * 0.5;
                vsOut.color = vec4f(diffuse * uni.MaterialDiffuse.rgb, uni.MaterialDiffuse.a);
                return vsOut;
            }

            @fragment fn fs(vsOut : vsOutput) -> @location(0) vec4f {
                return vsOut.color;
            }
        )");

        // 从文件加载顶点数据
        auto [vertexData, vertexCount] = loadVertexDataFromFile("D:\\Study\\PKU\\research_group_mayun\\dawn\\src\\dawn\\samples\\scenes\\shading.data");
        if (vertexData.empty()) {
            std::cerr << "Failed to load vertex data, using empty buffer" << std::endl;
            return false;
        }
        
        // 存储顶点数量供渲染时使用
        this->vertexCount = vertexCount;

        vertexBuffer = dawn::utils::CreateBufferFromData(
            device,
            vertexData.data(),
            vertexData.size() * sizeof(float),
            wgpu::BufferUsage::Vertex
        );
        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.size = sizeof(Uniform);
        bufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        uniformBuffer = device.CreateBuffer(&bufferDesc);

        wgpu::BindGroupLayout bgl = dawn::utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::Uniform}}
        );
        bindGroup = dawn::utils::MakeBindGroup(
            device, bgl,
            {
                {0, uniformBuffer, 0, sizeof(Uniform)},
            }
        );

        wgpu::TextureDescriptor depthTextureDesc;
        depthTextureDesc.size = {width, height, 1};
        depthTextureDesc.format = wgpu::TextureFormat::Depth24PlusStencil8;
        depthTextureDesc.usage = wgpu::TextureUsage::RenderAttachment;
        depthTexture = device.CreateTexture(&depthTextureDesc);

        dawn::utils::ComboRenderPipelineDescriptor descriptor;
        descriptor.layout = dawn::utils::MakeBasicPipelineLayout(device, &bgl);
        descriptor.vertex.module = Module;
        descriptor.vertex.entryPoint = "vs";
        descriptor.vertex.bufferCount = 1;

        descriptor.cBuffers[0].arrayStride = sizeof(float) * 6; // 3 for position, 3 for color, 3 for normal
        descriptor.cBuffers[0].attributeCount = 2;
        descriptor.cAttributes[0].format = wgpu::VertexFormat::Float32x3; // position
        descriptor.cAttributes[1].format = wgpu::VertexFormat::Float32x3; // color
        descriptor.cAttributes[0].offset = 0;
        descriptor.cAttributes[1].offset = sizeof(float) * 3; // color starts
        descriptor.cAttributes[0].shaderLocation = 0; // position
        descriptor.cAttributes[1].shaderLocation = 1; // color

        descriptor.cFragment.module = Module;
        descriptor.cFragment.entryPoint = "fs";
        descriptor.cTargets[0].format = GetPreferredSurfaceTextureFormat();

        wgpu::DepthStencilState depthStencilState = {};
        depthStencilState.format = wgpu::TextureFormat::Depth24PlusStencil8;
        depthStencilState.depthWriteEnabled = true;
        depthStencilState.depthCompare = wgpu::CompareFunction::Less;
        descriptor.depthStencil = &depthStencilState;

        pipeline = device.CreateRenderPipeline(&descriptor);

        return true;
    }

    void FrameImpl() override {
        wgpu::SurfaceTexture surfaceTexture;
        surface.GetCurrentTexture(&surfaceTexture);
        wgpu::RenderPassDescriptor renderPassDesc = {};
        wgpu::RenderPassColorAttachment colorAttachment = {};
        colorAttachment.view = surfaceTexture.texture.CreateView();
        colorAttachment.loadOp = wgpu::LoadOp::Clear;
        colorAttachment.storeOp = wgpu::StoreOp::Store;
        colorAttachment.clearValue = {0.0f, 0.0f, 0.0f, 1.0f}; 
        renderPassDesc.colorAttachmentCount = 1;
        renderPassDesc.colorAttachments = &colorAttachment;
    
        // 深度模板附件配置
        wgpu::RenderPassDepthStencilAttachment depthStencilAttachment = {};
        depthStencilAttachment.view = depthTexture.CreateView();
        depthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
        depthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
        depthStencilAttachment.depthClearValue = 1.0f; // WebGPU中深度范围是[0,1]，1.0是最远的
        depthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Clear;
        depthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Store;
        // depthStencilAttachment.stencilClearValue = 0;
        renderPassDesc.depthStencilAttachment = &depthStencilAttachment;

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
            pass.SetPipeline(pipeline);
            pass.SetBindGroup(0, bindGroup);
            pass.SetVertexBuffer(0, vertexBuffer);
            pass.SetViewport(0, 0, width, height, 0, 1);
            pass.Draw(vertexCount, 1, 0, 0); // 从文件加载的顶点数量
            pass.End();
        }

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        update();
    }

    void UpdateUniformBuffer() {
        Uniform ubo = {
            glm::mat4(1.0f), // modelViewProjection
            glm::mat4(1.0f), // normal
            glm::vec4(0.0f),     // materialDiffuse
            glm::mat4(1.0f), // modelView
        };
        glm::mat4 modelView = glm::mat4(1.0f);
        modelView = glm::translate(modelView, glm::vec3{-center.x, -center.y, -(center.z + 2.0 + radius)});
        modelView = glm::rotate(modelView, glm::radians(rotation), {0.0f, 1.0f, 0.0f});

        ubo.modelViewProjection = projection * modelView;
        ubo.normal = glm::transpose(glm::inverse(modelView));
        ubo.materialDiffuse = glm::vec4{0.0f, 0.0f, 0.7f, 1.0f};
        ubo.modelView = modelView;

        // auto const projection = glm::frustum(-2.8f, 2.8f, -2.8f * aspect, 2.8f * aspect, 6.0f, 10.0f);
        auto const projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

        ubo.modelViewProjection = projection * ubo.modelView;
        ubo.normal = glm::transpose(glm::inverse(ubo.modelView));

        // 更新uniform缓冲区
        queue.WriteBuffer(uniformBuffer, 0, &ubo, sizeof(ubo));

        auto now = std::chrono::steady_clock::now();
        float time = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() / 1000.0f;
        rotation = 36.0f * time; // 每秒旋转36度
    }

    void update() {
        // printf("rotation: [%f, %f, %f]\n", rotation[0], rotation[1], rotation[2]);
        updateFPS();
        // 更新uniform数据
        UpdateUniformBuffer();
    }

    void updateFPS() {
        lastFrameCount++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameTime).count();
        if (lastFrameCount >= MAX_FRAME_COUNT || elapsed >= MAX_FRAME_TIME) {
            float fps = lastFrameCount / (elapsed / 1000.0f);
            printf("FPS: %.3f\n", fps);
            
            // 收集FPS样本
            fpsSamples.push_back(fps);
            
            lastFrameTime = now;
            lastFrameCount = 0;
        }
    }

    // std::vector<ShaderData> shaderData;
    wgpu::RenderPipeline pipeline;
    wgpu::BindGroup bindGroup;
    wgpu::Buffer vertexBuffer;
    wgpu::Buffer uniformBuffer;
    uint32_t vertexCount = 0;  // 顶点数量
    wgpu::Texture depthTexture;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastFrameTime;
    int lastFrameCount = 0;
    uint32_t width, height;
    float aspect, radius, rotation{0};
    glm::vec3 center;
    glm::mat4 projection;
    std::vector<float> fpsSamples;  // FPS数据收集
};

// 创建场景函数，供GPUMark调用
std::unique_ptr<SampleBase> CreateShadingScene() {
    return std::make_unique<ShadingScene>();
}

#ifndef GPUMARK_BUILD
int main(int argc, const char* argv[]) {
    if (!InitSample(argc, argv)) {
        return 1;
    }

    ShadingScene* sample = new ShadingScene();
    sample->Run(0);
}
#endif
