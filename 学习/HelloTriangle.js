/*
 * JavaScript 版本的 HelloTriangle，与 HelloTriangle.cpp 一一对应
 * 
 * C++ vs JavaScript API 对应关系：
 * 
 * C++ (Dawn/WebGPU Native)          →  JavaScript (WebGPU Web API)
 * ==========================================================================================================
 * class HelloTriangleSample         →  class HelloTriangleSample
 * : public SampleBase               →  (继承关系通过组合实现)
 * 
 * wgpu::Device device               →  this.device (GPUDevice)
 * wgpu::Buffer vertexBuffer         →  this.vertexBuffer (GPUBuffer)  
 * wgpu::RenderPipeline pipeline     →  this.pipeline (GPURenderPipeline)
 * wgpu::Surface surface             →  this.context (GPUCanvasContext)
 * wgpu::Queue queue                 →  this.device.queue (GPUQueue)
 * 
 * device.CreateBuffer()             →  device.createBuffer()
 * device.CreateShaderModule()       →  device.createShaderModule()
 * device.CreateRenderPipeline()     →  device.createRenderPipeline()
 * device.CreateCommandEncoder()     →  device.createCommandEncoder()
 * 
 * dawn::utils::CreateBufferFromData →  手动创建buffer + mappedAtCreation
 * dawn::utils::CreateShaderModule   →  device.createShaderModule()
 * ComboRenderPipelineDescriptor     →  手动构造 pipelineDescriptor 对象
 * 
 * 内存管理：
 * C++: RAII + 智能指针自动管理      →  JavaScript: GC 自动管理
 * 
 * 错误处理：
 * C++: 异常 + 返回值检查           →  JavaScript: Promise + try/catch
 */

class HelloTriangleSample {
    constructor() {
        // C++: 成员变量在 private: 部分声明，通过 SampleBase 继承 device 等
        // JS:  直接在构造函数中声明所有需要的成员变量
        this.device = null;           // 对应 C++: wgpu::Device device (继承自 SampleBase)
        this.context = null;          // 对应 C++: wgpu::Surface surface (继承自 SampleBase)  
        this.canvas = null;           // 对应 C++: GLFW 窗口 (在 SampleBase 中管理)
        this.vertexBuffer = null;     // 对应 C++: wgpu::Buffer vertexBuffer
        this.pipeline = null;         // 对应 C++: wgpu::RenderPipeline pipeline
        this.preferredFormat = null;  // 对应 C++: GetPreferredSurfaceTextureFormat() 返回值
    }

    // 对应 C++ 的 bool SetupImpl() override 方法
    // C++: 返回 bool 表示成功/失败，JS: 返回 Promise<boolean> 
    async setupImpl() {
        /*
         * 顶点数据对应关系：
         * C++: static const float vertexData[12] = { ... };
         * JS:  const vertexData = new Float32Array([ ... ]);
         * 
         * 数据布局完全相同：每个顶点 4 个 float (x, y, z, w)
         */
        const vertexData = new Float32Array([
            0.0, 0.5, 0.0, 1.0,    // 第一个顶点 (x, y, z, w) - 顶部
            -0.5, -0.5, 0.0, 1.0,  // 第二个顶点 - 左下
            0.5, -0.5, 0.0, 1.0    // 第三个顶点 - 右下
        ]);

        /*
         * Buffer 创建对应关系（完全对应 C++ 的实现方式）：
         * C++: vertexBuffer = dawn::utils::CreateBufferFromData(device, vertexData, sizeof(vertexData), wgpu::BufferUsage::Vertex);
         * 
         * dawn::utils::CreateBufferFromData 的实际实现：
         * 1. descriptor.usage = usage | wgpu::BufferUsage::CopyDst;  // 自动添加 CopyDst
         * 2. wgpu::Buffer buffer = device.CreateBuffer(&descriptor);
         * 3. device.GetQueue().WriteBuffer(buffer, 0, data, size);
         */
        this.vertexBuffer = this.device.createBuffer({
            size: vertexData.byteLength,                    // 对应 C++: sizeof(vertexData)
            usage: GPUBufferUsage.VERTEX | GPUBufferUsage.COPY_DST  // 对应 C++: wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst
            // 不使用 mappedAtCreation，保持与 C++ 一致的创建方式
        });
        // 对应 C++: device.GetQueue().WriteBuffer(buffer, 0, data, size)
        this.device.queue.writeBuffer(this.vertexBuffer, 0, vertexData);

        /*
         * Shader 模块创建对应关系：
         * C++: wgpu::ShaderModule module = dawn::utils::CreateShaderModule(device, R"(shader_code)");
         * JS:  const shaderModule = this.device.createShaderModule({ code: `shader_code` });
         * 
         * WGSL 代码完全相同，只是字符串格式不同 (C++ 原始字符串 vs JS 模板字符串)
         */
        const shaderModule = this.device.createShaderModule({
            code: `
                @vertex fn vs(@location(0) pos : vec4f) -> @builtin(position) vec4f {
                    return pos;
                }

                @fragment fn fs(@builtin(position) FragCoord : vec4f) -> @location(0) vec4f {
                    return vec4f(1, 0, 0, 1);  // 红色 (R=1, G=0, B=0, A=1)
                }
            `
        });

        /*
         * 渲染管线描述符对应关系（完全按照 C++ 的设置顺序和方式）：
         * C++: dawn::utils::ComboRenderPipelineDescriptor descriptor;
         *      descriptor.layout = nullptr;                    // layout: 'auto'  
         *      descriptor.vertex.module = module;
         *      descriptor.vertex.bufferCount = 1;
         *      descriptor.cBuffers[0].arrayStride = 4 * sizeof(float);
         *      descriptor.cBuffers[0].attributeCount = 1;
         *      descriptor.cAttributes[0].format = wgpu::VertexFormat::Float32x4;
         *      descriptor.cFragment.module = module;
         *      descriptor.cTargets[0].format = GetPreferredSurfaceTextureFormat();
         */
        const pipelineDescriptor = {
            layout: 'auto',              // 对应 C++: descriptor.layout = nullptr

            vertex: {
                module: shaderModule,    // 对应 C++: descriptor.vertex.module = module
                entryPoint: 'vs',        // C++ 中自动推断，JS 中需要显式指定
                buffers: [{              // 对应 C++: descriptor.vertex.bufferCount = 1
                    arrayStride: 4 * 4,  // 对应 C++: descriptor.cBuffers[0].arrayStride = 4 * sizeof(float)
                    attributes: [{       // 对应 C++: descriptor.cBuffers[0].attributeCount = 1
                        format: 'float32x4',      // 对应 C++: descriptor.cAttributes[0].format = wgpu::VertexFormat::Float32x4
                        offset: 0,                // ComboRenderPipelineDescriptor 默认值
                        shaderLocation: 0         // 对应 @location(0)
                    }]
                }]
            },

            fragment: {                  // 对应 C++: descriptor.cFragment.module = module
                module: shaderModule,
                entryPoint: 'fs',        // C++ 中自动推断，JS 中需要显式指定  
                targets: [{              // 对应 C++: descriptor.cTargets[0].format = GetPreferredSurfaceTextureFormat()
                    format: this.preferredFormat
                }]
            }
            // primitive.topology 使用默认值 'triangle-list'，与 C++ ComboRenderPipelineDescriptor 一致
        };

        // 对应 C++: pipeline = device.CreateRenderPipeline(&descriptor);
        this.pipeline = this.device.createRenderPipeline(pipelineDescriptor);
        return true;  // 对应 C++: return true;
    }

    // 对应 C++ 的 void FrameImpl() override 方法（完全按照 C++ 的执行顺序）
    frameImpl() {
        /*
         * 获取当前帧纹理（与 C++ 完全对应）：
         * C++: wgpu::SurfaceTexture surfaceTexture;
         *      surface.GetCurrentTexture(&surfaceTexture);
         * JS:  const currentTexture = this.context.getCurrentTexture();
         */
        const currentTexture = this.context.getCurrentTexture();
        
        /*
         * 渲染通道描述符（与 C++ 完全对应）：
         * C++: dawn::utils::ComboRenderPassDescriptor renderPass({surfaceTexture.texture.CreateView()});
         * JS:  手动构造等效的 renderPassDescriptor
         * 
         * ComboRenderPassDescriptor 默认设置：
         * - loadOp: Clear, storeOp: Store
         * - clearValue: {0.0f, 0.0f, 0.0f, 0.0f}
         */
        const renderPassDescriptor = {
            colorAttachments: [{
                view: currentTexture.createView(),        // 对应 C++: surfaceTexture.texture.CreateView()
                clearValue: { r: 0.0, g: 0.0, b: 0.0, a: 0.0 },  // ComboRenderPassDescriptor 默认值
                loadOp: 'clear',                          // ComboRenderPassDescriptor 默认值
                storeOp: 'store'                          // ComboRenderPassDescriptor 默认值
            }]
        };

        /*
         * 命令编码器创建（与 C++ 完全对应）：
         * C++: wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
         */
        const encoder = this.device.createCommandEncoder();
        
        {   // 保持 C++ 的作用域块风格
            /*
             * 渲染通道编码器（与 C++ 完全对应）：
             * C++: wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
             */
            const pass = encoder.beginRenderPass(renderPassDescriptor);
            
            /*
             * 渲染命令（与 C++ 完全对应）：
             * C++: pass.SetPipeline(pipeline);
             *      pass.SetVertexBuffer(0, vertexBuffer);
             *      pass.Draw(3);
             *      pass.End();
             */
            pass.setPipeline(this.pipeline);              // 对应 C++: pass.SetPipeline(pipeline)
            pass.setVertexBuffer(0, this.vertexBuffer);   // 对应 C++: pass.SetVertexBuffer(0, vertexBuffer)
            pass.draw(3);                                 // 对应 C++: pass.Draw(3)
            pass.end();                                   // 对应 C++: pass.End()
        }   // C++ 作用域结束，pass 自动析构

        /*
         * 命令提交（与 C++ 完全对应）：
         * C++: wgpu::CommandBuffer commands = encoder.Finish();
         *      queue.Submit(1, &commands);
         */
        const commands = encoder.finish();               // 对应 C++: encoder.Finish()
        this.device.queue.submit([commands]);            // 对应 C++: queue.Submit(1, &commands)
    }

    // 对应 C++ SampleBase 的 Run() 方法和初始化过程
    async run() {
        /*
         * WebGPU 初始化对应关系：
         * C++ 中通过 SampleBase 自动处理适配器和设备创建
         * JS 中需要手动调用 WebGPU API
         */
        
        // 检查 WebGPU 支持 (C++ 中编译时确定)
        if (!navigator.gpu) {
            throw new Error('WebGPU not supported');
        }

        /*
         * 适配器和设备创建对应关系：
         * C++ (在 SampleBase 中): 
         *   - wgpu::Instance instance = wgpu::CreateInstance();
         *   - instance.RequestAdapter() 
         *   - adapter.RequestDevice()
         * 
         * JS:
         *   - navigator.gpu.requestAdapter()
         *   - adapter.requestDevice()
         */
        const adapter = await navigator.gpu.requestAdapter();
        if (!adapter) {
            throw new Error('No WebGPU adapter found');
        }

        this.device = await adapter.requestDevice();

        /*
         * 窗口/画布创建对应关系：
         * C++ (在 SampleBase 中):
         *   - 使用 GLFW 创建窗口
         *   - 创建 wgpu::Surface 
         * 
         * JS:
         *   - 创建 HTML Canvas 元素
         *   - 获取 WebGPU 上下文
         */
        this.canvas = document.createElement('canvas');
        this.canvas.width = 640;   // 对应 C++ 中的窗口宽度
        this.canvas.height = 480;  // 对应 C++ 中的窗口高度
        document.body.appendChild(this.canvas);

        /*
         * Surface 配置对应关系：
         * C++ (在 SampleBase 中):
         *   - GetPreferredSurfaceTextureFormat() 
         *   - surface 自动配置
         * 
         * JS:
         *   - navigator.gpu.getPreferredCanvasFormat()
         *   - context.configure()
         */
        this.context = this.canvas.getContext('webgpu');
        this.preferredFormat = navigator.gpu.getPreferredCanvasFormat();
        
        this.context.configure({
            device: this.device,
            format: this.preferredFormat,
            alphaMode: 'opaque'  // 对应 C++ 中的默认 alpha 模式
        });

        // 对应 C++: SetupImpl() 调用
        await this.setupImpl();

        /*
         * 渲染循环对应关系：
         * C++ (在 SampleBase::Run() 中):
         *   - while (!ShouldQuit()) { FrameImpl(); }
         *   - 使用 GLFW 的事件循环
         * 
         * JS:
         *   - requestAnimationFrame() 递归调用
         *   - 浏览器的渲染循环
         */
        const renderLoop = () => {
            this.frameImpl();                    // 对应 C++: FrameImpl() 调用
            requestAnimationFrame(renderLoop);  // 对应 C++ 的循环继续
        };
        renderLoop();
    }
}

/*
 * main() 函数对应关系（尽可能保持 C++ 的结构和逻辑）：
 * C++: int main(int argc, const char* argv[]) {
 *          if (!InitSample(argc, argv)) {
 *              return 1;
 *          }
 *          HelloTriangleSample* sample = new HelloTriangleSample();
 *          sample->Run(16000);
 *      }
 * 
 * 对应的 JS 实现：
 * - InitSample() 对应浏览器的 WebGPU 初始化检查
 * - new HelloTriangleSample() 对应创建实例
 * - sample->Run(16000) 对应 await sample.run()
 * - 错误处理：C++ 返回 1，JS 抛出异常
 */
async function main() {
    // 对应 C++: if (!InitSample(argc, argv)) return 1;
    if (!navigator.gpu) {
        console.error('WebGPU not supported');
        return 1;  // 保持 C++ 的错误返回方式
    }

    try {
        // 对应 C++: HelloTriangleSample* sample = new HelloTriangleSample();
        const sample = new HelloTriangleSample();
        
        // 对应 C++: sample->Run(16000);
        await sample.run();
        
        console.log('HelloTriangle sample started successfully');
        return 0;  // 对应 C++ 的成功返回
    } catch (error) {
        console.error('Failed to run HelloTriangle sample:', error);
        return 1;  // 对应 C++ 的错误返回
    }
}

/*
 * 程序入口点对应关系：
 * C++: main() 函数直接作为程序入口点
 * JS:  需要等待 DOM 加载完成，或检测运行环境
 */
if (typeof window !== 'undefined') {
    // 浏览器环境：等待页面加载后运行 (对应 C++ 的直接运行)
    window.addEventListener('load', main);
} else {
    // Node.js 环境：直接运行 (如果未来支持 Node.js WebGPU)
    main();
}

/*
 * 模块导出对应关系：
 * C++: 通过头文件和链接器解析符号
 * JS:  使用 CommonJS 或 ES6 模块系统
 */
if (typeof module !== 'undefined' && module.exports) {
    module.exports = HelloTriangleSample;  // CommonJS 导出
}
