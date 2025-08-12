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
    alignas(16) glm::mat4 modelView;
    alignas(16) glm::mat4 modelViewProjection;
    alignas(16) glm::mat4 normal;
};

class CubeScene : public SampleBase {
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

        wgpu::ShaderModule vsModule = dawn::utils::CreateShaderModule(device, R"(
            struct vsIn {
                @location(0) position : vec3f,
                @location(1) color : vec3f,
                @location(2) normal : vec3f,
            }

            struct Uniform {
                modelview : mat4x4f,
                modelviewprojection : mat4x4f,
                normal : mat4x4f,
            }

            struct vsOutput {
                @builtin(position) position : vec4f,
                @location(0) color : vec4f,
            }

            @group(0) @binding(0) var<uniform> uni : Uniform;
            //@group(0) @binding(1) var<storage, read_write> data : array<f32>;

            @vertex fn vs(vert : vsIn) -> vsOutput
            {
                const lightSource : vec4f = vec4f(2.0, 2.0, 20.0, 0.0);
                var position : vec4f = uni.modelviewprojection * vec4f(vert.position, 1.0);
                var eyeNormal : vec3f = (uni.normal * vec4f(vert.normal, 1.0)).xyz;
                var position4 : vec4f = uni.modelview * vec4f(vert.position, 1.0);
                var position3 : vec3f = position4.xyz / position4.w;
                var lightDir : vec3f = normalize(lightSource.xyz - position3);
                var diff : f32 = max(0.0, dot(eyeNormal, lightDir));
                var vsOut : vsOutput;
                //vsOut.position = vec4f(position.xyz / 2, position.w);
                vsOut.position = vec4f(position.xyz / position.w, 1.0);
                vsOut.position.z = (vsOut.position.z + 1.0) / 2.0;
                // vsOut.position = position;
                vsOut.color = vec4f(diff * vert.color, 1.0);
                return vsOut;
            }
        )");

        wgpu::ShaderModule fsModule = dawn::utils::CreateShaderModule(device, R"(
            struct vsOutput {
                @builtin(position) position : vec4f,
                @location(0) color : vec4f,
            }
            @fragment fn fs(vsOut : vsOutput) -> @location(0) vec4f {
                return vsOut.color;
            }
        )");

        float vertexData[] = {
            -1, -1, 1, 0, 0, 1, 0, -1, 0, 
            -1, 1, 1, 0, 1, 1, 0, 1, 0, 
            1, 1, 1, 1, 1, 1, 0, 1, 0, 
            -1, -1, 1, 0, 0, 1, 0, -1, 0, 
            1, 1, 1, 1, 1, 1, 0, 1, 0, 
            1, -1, 1, 1, 0, 1, 0, -1, 0, 
            1, -1, -1, 1, 0, 0, 0, -1, 0, 
            1, 1, -1, 1, 1, 0, 0, 1, 0, 
            -1, 1, -1, 0, 1, 0, 0, 1, 0, 
            1, -1, -1, 1, 0, 0, 0, -1, 0, 
            -1, 1, -1, 0, 1, 0, 0, 1, 0, 
            -1, -1, -1, 0, 0, 0, 0, -1, 0, 
            1, -1, 1, 1, 0, 1, 0, -1, 0, 
            1, 1, 1, 1, 1, 1, 0, 1, 0, 
            1, 1, -1, 1, 1, 0, 0, 1, 0, 
            1, -1, 1, 1, 0, 1, 0, -1, 0, 
            1, 1, -1, 1, 1, 0, 0, 1, 0, 
            1, -1, -1, 1, 0, 0, 0, -1, 0, 
            -1, -1, -1, 0, 0, 0, 0, -1, 0, 
            -1, 1, -1, 0, 1, 0, 0, 1, 0, 
            -1, 1, 1, 0, 1, 1, 0, 1, 0, 
            -1, -1, -1, 0, 0, 0, 0, -1, 0, 
            -1, 1, 1, 0, 1, 1, 0, 1, 0, 
            -1, -1, 1, 0, 0, 1, 0, -1, 0, 
            1, -1, 1, 1, 0, 1, 0, -1, 0, 
            -1, -1, -1, 0, 0, 0, 0, -1, 0, 
            -1, -1, 1, 0, 0, 1, 0, -1, 0, 
            1, -1, 1, 1, 0, 1, 0, -1, 0, 
            1, -1, -1, 1, 0, 0, 0, -1, 0, 
            -1, -1, -1, 0, 0, 0, 0, -1, 0, 
            -1, 1, 1, 0, 1, 1, 0, 1, 0, 
            -1, 1, -1, 0, 1, 0, 0, 1, 0, 
            1, 1, -1, 1, 1, 0, 0, 1, 0, 
            -1, 1, 1, 0, 1, 1, 0, 1, 0, 
            1, 1, -1, 1, 1, 0, 0, 1, 0, 
            1, 1, 1, 1, 1, 1, 0, 1, 0
        };

        vertexBuffer = dawn::utils::CreateBufferFromData(
            device,
            vertexData,
            sizeof(vertexData),
            wgpu::BufferUsage::Vertex
        );
        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.size = sizeof(Uniform);
        bufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        uniformBuffer = device.CreateBuffer(&bufferDesc);

        wgpu::BindGroupLayout bgl = dawn::utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Vertex, wgpu::BufferBindingType::Uniform}}
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
        descriptor.vertex.module = vsModule;
        // descriptor.vertex.entryPoint = "vs";
        descriptor.vertex.bufferCount = 1;

        descriptor.cBuffers[0].arrayStride = sizeof(float) * 9; // 3 for position, 3 for color, 3 for normal
        descriptor.cBuffers[0].attributeCount = 3;
        descriptor.cAttributes[0].format = wgpu::VertexFormat::Float32x3; // position
        descriptor.cAttributes[1].format = wgpu::VertexFormat::Float32x3; // color
        descriptor.cAttributes[2].format = wgpu::VertexFormat::Float32x3; // normal
        descriptor.cAttributes[0].offset = 0;
        descriptor.cAttributes[1].offset = sizeof(float) * 3; // color starts
        descriptor.cAttributes[2].offset = sizeof(float) * 6; // normal starts
        descriptor.cAttributes[0].shaderLocation = 0; // position
        descriptor.cAttributes[1].shaderLocation = 1; // color
        descriptor.cAttributes[2].shaderLocation = 2; // normal

        descriptor.cFragment.module = fsModule;
        // descriptor.cFragment.entryPoint = "fs";
        descriptor.cTargets[0].format = GetPreferredSurfaceTextureFormat();

        wgpu::DepthStencilState depthStencilState = {};
        depthStencilState.format = wgpu::TextureFormat::Depth24PlusStencil8;
        depthStencilState.depthWriteEnabled = true;
        depthStencilState.depthCompare = wgpu::CompareFunction::Less;
        descriptor.depthStencil = &depthStencilState;

        // descriptor.cDepthStencil.depthWriteEnabled = true;
        // descriptor.cDepthStencil.depthCompare = wgpu::CompareFunction::Less;
        // descriptor.cDepthStencil.format = wgpu::TextureFormat::Depth24PlusStencil8;
        // wgpu::DepthStencilState depthStencilState = {};
        // depthStencilState.format = wgpu::TextureFormat::Depth24PlusStencil8;
        // depthStencilState.depthWriteEnabled = true;
        // depthStencilState.depthCompare = wgpu::CompareFunction::Less;
        // descriptor.depthStencil = &depthStencilState;

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
        colorAttachment.clearValue = {0.2f, 0.2f, 0.2f, 1.0f}; 
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

        // renderPass.cColorAttachments[0].view = surfaceTexture.texture.CreateView();
        // renderPass.cColorAttachments[0].clearValue = {0.2f, 0.2f, 0.2f, 1.0f};
        // renderPass.cColorAttachments[0].loadOp = wgpu::LoadOp::Clear;
        // renderPass.cColorAttachments[0].storeOp = wgpu::StoreOp::Store;
        // renderPass.cDepthStencilAttachmentInfo.depthClearValue = 1.0f;
        // renderPass.cDepthStencilAttachmentInfo.depthLoadOp = wgpu::LoadOp::Clear;
        // renderPass.cDepthStencilAttachmentInfo.depthStoreOp = wgpu::StoreOp::Store;
        // renderPass.cDepthStencilAttachmentInfo.stencilLoadOp = wgpu::LoadOp::Clear;
        // renderPass.cDepthStencilAttachmentInfo.stencilStoreOp = wgpu::StoreOp::Store;
        // renderPass.cDepthStencilAttachmentInfo.view = depthTexture.CreateView();

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
            pass.SetPipeline(pipeline);
            pass.SetBindGroup(0, bindGroup);
            pass.SetVertexBuffer(0, vertexBuffer);
            pass.SetViewport(0, 0, width, height, 0, 1);
            pass.Draw(36, 1, 0, 0); // 36 vertices
            pass.End();
        }

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        update();
    }

    void UpdateUniformBuffer() {
        Uniform ubo = {
            glm::mat4(1.0f), // modelView
            glm::mat4(1.0f), // modelViewProjection
            glm::mat4(1.0f)  // normal
        };
        ubo.modelView = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -8.0f));
        ubo.modelView = glm::rotate(ubo.modelView, glm::radians(rotation[0]), {1.0f, 0.0f, 0.0f});
        ubo.modelView = glm::rotate(ubo.modelView, glm::radians(rotation[1]), {0.0f, 1.0f, 0.0f});
        ubo.modelView = glm::rotate(ubo.modelView, glm::radians(rotation[2]), {0.0f, 0.0f, 1.0f});

        // auto const projection = glm::frustum(-2.8f, 2.8f, -2.8f * aspect, 2.8f * aspect, 6.0f, 10.0f);
        auto const projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

        ubo.modelViewProjection = projection * ubo.modelView;
        ubo.normal = glm::transpose(glm::inverse(ubo.modelView));

        // 更新uniform缓冲区
        queue.WriteBuffer(uniformBuffer, 0, &ubo, sizeof(ubo));

        auto now = std::chrono::steady_clock::now();
        float time = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() / 5;
        rotation[0] = 45 + 0.25f * time;
        rotation[1] = 45 + 0.5f * time;
        rotation[2] = 10 + 0.1f * time; 
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
    wgpu::Texture depthTexture;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastFrameTime;
    int lastFrameCount = 0;
    uint32_t width, height;
    float aspect;
    float rotation[3] = {45, 45, 10};
    std::vector<float> fpsSamples;  // FPS数据收集
};

// 创建场景函数，供GPUMark调用
std::unique_ptr<SampleBase> CreateCubeScene() {
    return std::make_unique<CubeScene>();
}

#ifndef GPUMARK_BUILD
int main(int argc, const char* argv[]) {
    if (!InitSample(argc, argv)) {
        return 1;
    }

    CubeScene* sample = new CubeScene();
    sample->Run(0);
}
#endif
