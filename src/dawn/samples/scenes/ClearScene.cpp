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

class ClearScene : public SampleBase {
public:
    using SampleBase::SampleBase;
    
    // 添加FPS数据收集接口
    std::vector<float> GetFPSSamples() const { return fpsSamples; }
    void ClearFPSSamples() { fpsSamples.clear(); }
    bool HasEnoughSamples(int targetCount) const { return fpsSamples.size() >= targetCount; }
    
private:
    bool SetupImpl() override {
        startTime = std::chrono::steady_clock::now();
        return true;
    }

    void FrameImpl() override {
        // lastFrameCount++;
        wgpu::SurfaceTexture surfaceTexture;
        surface.GetCurrentTexture(&surfaceTexture);

        dawn::utils::ComboRenderPassDescriptor renderPass({surfaceTexture.texture.CreateView()});
        renderPass.cColorAttachments[0].clearValue = {clearColor[0], clearColor[1], clearColor[2], clearColor[3]};
        
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
            pass.End();
        }

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        update();
    }

    void update() {
        updateFPS();

        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count() / 1000000.0f;
        // printf("Elapsed time: %lf seconds\n", elapsed);
        // if (frameCount % 60 == 0) {
        //     printf("FPS: %lf\n", 60.0 / timerForFPS->GetElapsedTime());
        //     timerForFPS->Start();
        // }
        float period = 5.0;
        float c = 1.0f;
        float h = fmod(elapsed / period * 6.0f, 6.0f);
        float x = c * (1 - fabs(fmod(h, 2.0f) - 1));
        float r = 0.0f, g = 0.0f, b = 0.0f;
        switch (int(h)) {
            case 0: r = c; g = x; b = 0; break;
            case 1: r = x; g = c; b = 0; break;
            case 2: r = 0; g = c; b = x; break;
            case 3: r = 0; g = x; b = c; break;
            case 4: r = x; g = 0; b = c; break;
            case 5: r = c; g = 0; b = x; break;
            default: r = 0; g = 0; b = 0; break;
        }
        clearColor[0] = r;
        clearColor[1] = g;
        clearColor[2] = b;
        clearColor[3] = 1.0f;  // Alpha channel
        // printf("Clear color: R=%.2f, G=%.2f, B=%.2f, A=%.2f\n", clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
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

    float clearColor[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    // float lastFrameTime = 0.0f;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastFrameTime;
    int lastFrameCount = 0;
    std::vector<float> fpsSamples;  // FPS数据收集
};

// 创建场景函数，供GPUMark调用
std::unique_ptr<SampleBase> CreateClearScene() {
    return std::make_unique<ClearScene>();
}

#ifndef GPUMARK_BUILD
int main(int argc, const char* argv[]) {
    if (!InitSample(argc, argv)) {
        return 1;
    }

    ClearScene* sample = new ClearScene();
    sample->Run(0);
}
#endif
