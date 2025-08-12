/*
 * JavaScript 版本的 Animometer，与 Animometer.cpp 一一对应
 * 
 * Animometer 是 MotionMark 基准测试的 WebGPU 实现
 * 渲染大量动画三角形来测试渲染性能，并输出 FPS
 * 
 * C++ vs JavaScript API 对应关系：
 * ==========================================================================================================
 * constexpr size_t kNumTriangles = 10000;  →  const kNumTriangles = 10000;
 * struct alignas(256) ShaderData           →  class ShaderData (手动对齐)
 * std::vector<ShaderData> shaderData       →  Array<ShaderData>
 * wgpu::BindGroupLayout bgl                →  GPUBindGroupLayout
 * wgpu::BindGroup bindGroup                →  GPUBindGroup
 * wgpu::Buffer ubo                         →  GPUBuffer (Uniform Buffer Object)
 * dawn::utils::Timer* timer                →  Performance.now() 计时
 * RandomFloat()                            →  Math.random() 实现
 */

const kNumTriangles = 10000;  // 对应 C++: constexpr size_t kNumTriangles = 10000;

// 对应 C++: float RandomFloat(float min, float max)
function randomFloat(min, max) {
    return Math.random() * (max - min) + min;
}

// 对应 C++: struct alignas(256) ShaderData
// WebGPU 要求 uniform buffer 按 256 字节对齐
class ShaderData {
    constructor() {
        this.scale = 0.0;        // 对应 C++: float scale;
        this.time = 0.0;         // 对应 C++: float time;
        this.offsetX = 0.0;      // 对应 C++: float offsetX;
        this.offsetY = 0.0;      // 对应 C++: float offsetY;
        this.scalar = 0.0;       // 对应 C++: float scalar;
        this.scalarOffset = 0.0; // 对应 C++: float scalarOffset;
        // 填充到 256 字节对齐 (6 * 4 = 24 字节，需要填充 232 字节)
        this._padding = new Float32Array(58); // 58 * 4 = 232 字节
    }

    // 将数据写入到 Float32Array 中，用于传递给 GPU
    toFloat32Array() {
        const array = new Float32Array(64); // 256 / 4 = 64 个 float
        array[0] = this.scale;
        array[1] = this.time;
        array[2] = this.offsetX;
        array[3] = this.offsetY;
        array[4] = this.scalar;
        array[5] = this.scalarOffset;
        // 其余位置保持为 0 (padding)
        return array;
    }
}

class AnimometerSample {
    constructor() {
        // 对应 C++ 成员变量
        this.device = null;
        this.context = null;
        this.canvas = null;
        this.preferredFormat = null;
        
        // 对应 C++: wgpu::RenderPipeline pipeline;
        this.pipeline = null;
        
        // 对应 C++: wgpu::BindGroup bindGroup;
        this.bindGroup = null;
        
        // 对应 C++: wgpu::Buffer ubo;
        this.ubo = null;
        
        // 对应 C++: int frameCount = 0;
        this.frameCount = 0;
        
        // 对应 C++: dawn::utils::Timer* timer
        this.lastTime = 0;
        this.frameCountForFPS = 0;
        
        // 对应 C++: std::vector<ShaderData> shaderData;
        this.shaderData = [];
    }

    // 对应 C++ 的 bool SetupImpl() override 方法
    async setupImpl() {
        /*
         * 顶点着色器对应关系（完全相同的 WGSL 代码）：
         * C++: wgpu::ShaderModule vsModule = dawn::utils::CreateShaderModule(device, R"(shader_code)");
         * JS:  const vsModule = this.device.createShaderModule({ code: `shader_code` });
         */
        const vsModule = this.device.createShaderModule({
            code: `
                struct Constants {
                    scale : f32,
                    time : f32,
                    offsetX : f32,
                    offsetY : f32,
                    scalar : f32,
                    scalarOffset : f32,
                };
                @group(0) @binding(0) var<uniform> c : Constants;

                struct VertexOut {
                    @location(0) v_color : vec4f,
                    @builtin(position) Position : vec4f,
                };

                @vertex fn main(@builtin(vertex_index) VertexIndex : u32) -> VertexOut {
                    var positions : array<vec4f, 3> = array(
                        vec4f( 0.0,  0.1, 0.0, 1.0),
                        vec4f(-0.1, -0.1, 0.0, 1.0),
                        vec4f( 0.1, -0.1, 0.0, 1.0)
                    );

                    var colors : array<vec4f, 3> = array(
                        vec4f(1.0, 0.0, 0.0, 1.0),
                        vec4f(0.0, 1.0, 0.0, 1.0),
                        vec4f(0.0, 0.0, 1.0, 1.0)
                    );

                    var position : vec4f = positions[VertexIndex];
                    var color : vec4f = colors[VertexIndex];

                    // TODO(dawn:572): Revisit once modf has been reworked in WGSL.
                    var fade : f32 = c.scalarOffset + c.time * c.scalar / 10.0;
                    fade = fade - floor(fade);
                    if (fade < 0.5) {
                        fade = fade * 2.0;
                    } else {
                        fade = (1.0 - fade) * 2.0;
                    }

                    var xpos : f32 = position.x * c.scale;
                    var ypos : f32 = position.y * c.scale;
                    let angle : f32 = 3.14159 * 2.0 * fade;
                    let xrot : f32 = xpos * cos(angle) - ypos * sin(angle);
                    let yrot : f32 = xpos * sin(angle) + ypos * cos(angle);
                    xpos = xrot + c.offsetX;
                    ypos = yrot + c.offsetY;

                    var output : VertexOut;
                    output.v_color = vec4f(fade, 1.0 - fade, 0.0, 1.0) + color;
                    output.Position = vec4f(xpos, ypos, 0.0, 1.0);
                    return output;
                }
            `
        });

        /*
         * 片段着色器对应关系：
         * C++: wgpu::ShaderModule fsModule = dawn::utils::CreateShaderModule(device, R"(shader_code)");
         */
        const fsModule = this.device.createShaderModule({
            code: `
                @fragment fn main(@location(0) v_color : vec4f) -> @location(0) vec4f {
                    return v_color;
                }
            `
        });

        /*
         * 绑定组布局创建对应关系：
         * C++: wgpu::BindGroupLayout bgl = dawn::utils::MakeBindGroupLayout(
         *          device, {{0, wgpu::ShaderStage::Vertex, wgpu::BufferBindingType::Uniform, true}});
         */
        const bindGroupLayout = this.device.createBindGroupLayout({
            entries: [{
                binding: 0,                           // 对应 C++: binding = 0
                visibility: GPUShaderStage.VERTEX,    // 对应 C++: wgpu::ShaderStage::Vertex
                buffer: {                             // 对应 C++: wgpu::BufferBindingType::Uniform
                    type: 'uniform',
                    hasDynamicOffset: true            // 对应 C++: 最后一个参数 true
                }
            }]
        });

        /*
         * 渲染管线创建对应关系：
         * C++: dawn::utils::ComboRenderPipelineDescriptor descriptor;
         *      descriptor.layout = dawn::utils::MakeBasicPipelineLayout(device, &bgl);
         *      descriptor.vertex.module = vsModule;
         *      descriptor.cFragment.module = fsModule;
         *      descriptor.cTargets[0].format = GetPreferredSurfaceTextureFormat();
         */
        const pipelineLayout = this.device.createPipelineLayout({
            bindGroupLayouts: [bindGroupLayout]  // 对应 C++: MakeBasicPipelineLayout
        });

        const pipelineDescriptor = {
            layout: pipelineLayout,              // 对应 C++: descriptor.layout = ...
            vertex: {
                module: vsModule,                // 对应 C++: descriptor.vertex.module = vsModule
                entryPoint: 'main'
            },
            fragment: {                          // 对应 C++: descriptor.cFragment.module = fsModule
                module: fsModule,
                entryPoint: 'main',
                targets: [{                      // 对应 C++: descriptor.cTargets[0].format = ...
                    format: this.preferredFormat
                }]
            },
            primitive: {
                topology: 'triangle-list'        // 默认值
            }
        };

        // 对应 C++: pipeline = device.CreateRenderPipeline(&descriptor);
        this.pipeline = this.device.createRenderPipeline(pipelineDescriptor);

        /*
         * Shader 数据初始化对应关系：
         * C++: shaderData.resize(kNumTriangles);
         *      for (auto& data : shaderData) { ... }
         */
        this.shaderData = [];
        for (let i = 0; i < kNumTriangles; i++) {
            const data = new ShaderData();
            data.scale = randomFloat(0.2, 0.4);         // 对应 C++: data.scale = RandomFloat(0.2f, 0.4f);
            data.time = 0.0;                            // 对应 C++: data.time = 0.0;
            data.offsetX = randomFloat(-0.9, 0.9);      // 对应 C++: data.offsetX = RandomFloat(-0.9f, 0.9f);
            data.offsetY = randomFloat(-0.9, 0.9);      // 对应 C++: data.offsetY = RandomFloat(-0.9f, 0.9f);
            data.scalar = randomFloat(0.5, 2.0);        // 对应 C++: data.scalar = RandomFloat(0.5f, 2.0f);
            data.scalarOffset = randomFloat(0.0, 10.0); // 对应 C++: data.scalarOffset = RandomFloat(0.0f, 10.0f);
            this.shaderData.push(data);
        }

        /*
         * Uniform Buffer 创建对应关系：
         * C++: wgpu::BufferDescriptor bufferDesc;
         *      bufferDesc.size = kNumTriangles * sizeof(ShaderData);
         *      bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
         *      ubo = device.CreateBuffer(&bufferDesc);
         */
        this.ubo = this.device.createBuffer({
            size: kNumTriangles * 256,                  // 对应 C++: kNumTriangles * sizeof(ShaderData)
            usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.UNIFORM  // 对应 C++: CopyDst | Uniform
        });

        /*
         * 绑定组创建对应关系：
         * C++: bindGroup = dawn::utils::MakeBindGroup(device, bgl, {{0, ubo, 0, sizeof(ShaderData)}});
         */
        this.bindGroup = this.device.createBindGroup({
            layout: bindGroupLayout,            // 对应 C++: bgl
            entries: [{
                binding: 0,                     // 对应 C++: binding = 0
                resource: {
                    buffer: this.ubo,           // 对应 C++: ubo
                    offset: 0,                  // 对应 C++: offset = 0
                    size: 256                   // 对应 C++: sizeof(ShaderData)
                }
            }]
        });

        // 对应 C++: timer->Start();
        this.lastTime = performance.now();
        this.frameCountForFPS = 0;
        
        return true; // 对应 C++: return true;
    }

    // 对应 C++ 的 void FrameImpl() override 方法
    frameImpl() {
        /*
         * 帧计数和时间更新对应关系：
         * C++: frameCount++;
         *      for (auto& data : shaderData) { data.time = frameCount / 60.0f; }
         */
        this.frameCount++;
        this.frameCountForFPS++;
        
        for (const data of this.shaderData) {
            data.time = this.frameCount / 60.0;  // 对应 C++: data.time = frameCount / 60.0f;
        }

        /*
         * 数据上传对应关系：
         * C++: queue.WriteBuffer(ubo, 0, shaderData.data(), kNumTriangles * sizeof(ShaderData));
         */
        // 将所有 shader 数据写入 buffer
        for (let i = 0; i < kNumTriangles; i++) {
            const dataArray = this.shaderData[i].toFloat32Array();
            this.device.queue.writeBuffer(
                this.ubo,
                i * 256,    // 每个 ShaderData 256 字节对齐
                dataArray
            );
        }

        /*
         * 渲染过程对应关系（与 C++ 完全相同）：
         * C++: wgpu::SurfaceTexture surfaceTexture;
         *      surface.GetCurrentTexture(&surfaceTexture);
         *      dawn::utils::ComboRenderPassDescriptor renderPass({surfaceTexture.texture.CreateView()});
         */
        const currentTexture = this.context.getCurrentTexture();
        const renderPassDescriptor = {
            colorAttachments: [{
                view: currentTexture.createView(),
                clearValue: { r: 0.0, g: 0.0, b: 0.0, a: 0.0 },
                loadOp: 'clear',
                storeOp: 'store'
            }]
        };

        /*
         * 命令编码和渲染对应关系：
         * C++: wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
         *      wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
         *      pass.SetPipeline(pipeline);
         *      for (size_t i = 0; i < kNumTriangles; i++) {
         *          uint32_t offset = i * sizeof(ShaderData);
         *          pass.SetBindGroup(0, bindGroup, 1, &offset);
         *          pass.Draw(3);
         *      }
         */
        const encoder = this.device.createCommandEncoder();
        const pass = encoder.beginRenderPass(renderPassDescriptor);
        pass.setPipeline(this.pipeline);

        // 渲染所有三角形
        for (let i = 0; i < kNumTriangles; i++) {
            const offset = i * 256;  // 对应 C++: uint32_t offset = i * sizeof(ShaderData);
            pass.setBindGroup(0, this.bindGroup, [offset]);  // 对应 C++: pass.SetBindGroup(0, bindGroup, 1, &offset);
            pass.draw(3);            // 对应 C++: pass.Draw(3);
        }

        pass.end();
        const commands = encoder.finish();
        this.device.queue.submit([commands]);

        /*
         * FPS 计算和输出对应关系：
         * C++: if (frameCount % 60 == 0) {
         *          printf("FPS: %lf\n", 60.0 / timer->GetElapsedTime());
         *          timer->Start();
         *      }
         */
        if (this.frameCountForFPS % 60 === 0) {
            const currentTime = performance.now();
            const elapsedTime = (currentTime - this.lastTime) / 1000.0; // 转换为秒
            const fps = 60.0 / elapsedTime;
            console.log(`FPS: ${fps.toFixed(2)}`);  // 对应 C++: printf("FPS: %lf\n", ...)
            this.lastTime = currentTime;             // 对应 C++: timer->Start();
            this.frameCountForFPS = 0;
        }
    }

    // 对应 C++ SampleBase 的 Run() 方法
    async run() {
        if (!navigator.gpu) {
            throw new Error('WebGPU not supported');
        }

        const adapter = await navigator.gpu.requestAdapter();
        if (!adapter) {
            throw new Error('No WebGPU adapter found');
        }

        this.device = await adapter.requestDevice();

        this.canvas = document.createElement('canvas');
        this.canvas.width = 640;
        this.canvas.height = 480;
        document.body.appendChild(this.canvas);

        this.context = this.canvas.getContext('webgpu');
        this.preferredFormat = navigator.gpu.getPreferredCanvasFormat();
        
        this.context.configure({
            device: this.device,
            format: this.preferredFormat,
            alphaMode: 'opaque'
        });

        await this.setupImpl();

        // 对应 C++: sample->Run(0); (无限循环)
        const renderLoop = () => {
            this.frameImpl();
            requestAnimationFrame(renderLoop);
        };
        renderLoop();
    }
}

/*
 * main() 函数对应关系：
 * C++: int main(int argc, const char* argv[]) {
 *          if (!InitSample(argc, argv)) return 1;
 *          AnimometerSample* sample = new AnimometerSample();
 *          sample->Run(0);
 *      }
 */
async function main() {
    if (!navigator.gpu) {
        console.error('WebGPU not supported');
        return 1;
    }

    try {
        const sample = new AnimometerSample();  // 对应 C++: new AnimometerSample()
        await sample.run();                     // 对应 C++: sample->Run(0)
        console.log('Animometer sample started successfully');
        return 0;
    } catch (error) {
        console.error('Failed to run Animometer sample:', error);
        return 1;
    }
}

// 程序入口点
if (typeof window !== 'undefined') {
    window.addEventListener('load', main);
} else {
    main();
}

// 模块导出
if (typeof module !== 'undefined' && module.exports) {
    module.exports = AnimometerSample;
}
