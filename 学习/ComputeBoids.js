/*
 * JavaScript 版本的 ComputeBoids，与 ComputeBoids.cpp 一一对应
 * 
 * ComputeBoids 实现了基于计算着色器的鸟群聚集（Boids）模拟
 * 使用 Craig Reynolds 的三条规则：分离、对齐、聚合
 * 
 * C++ vs JavaScript API 对应关系：
 * ==========================================================================================================
 * constexpr uint32_t kNumParticles = 1500;   →  const kNumParticles = 1500;
 * struct Particle                             →  class Particle / Float32Array 结构
 * struct SimParams                            →  class SimParams / Float32Array 结构
 * std::array<wgpu::Buffer, 2> particleBuffers →  Array<GPUBuffer> (length 2)
 * wgpu::ComputePipeline updatePipeline        →  GPUComputePipeline
 * wgpu::RenderPipeline renderPipeline         →  GPURenderPipeline
 * wgpu::BindGroup updateBGs                   →  Array<GPUBindGroup>
 * size_t pingpong = 0                         →  let pingpong = 0
 */

const kNumParticles = 1024;  // 对应 C++: constexpr uint32_t kNumParticles = 1500;

// 对应 C++: struct Particle
class Particle {
    constructor(pos = [0, 0], vel = [0, 0]) {
        this.pos = new Float32Array(pos);  // vec2f pos 对应 C++: vec2f pos;
        this.vel = new Float32Array(vel);  // vec2f vel 对应 C++: vec2f vel;
    }

    // 返回 Float32Array 用于写入 GPU buffer
    toFloat32Array() {
        const array = new Float32Array(4);  // 2 * vec2f = 4 floats
        array[0] = this.pos[0];
        array[1] = this.pos[1];
        array[2] = this.vel[0];
        array[3] = this.vel[1];
        return array;
    }
}

// 对应 C++: struct SimParams
class SimParams {
    constructor() {
        this.deltaT = 0.04;         // 对应 C++: float deltaT = 0.04f;
        this.rule1Distance = 0.1;   // 对应 C++: float rule1Distance = 0.1f;
        this.rule2Distance = 0.025; // 对应 C++: float rule2Distance = 0.025f;
        this.rule3Distance = 0.025; // 对应 C++: float rule3Distance = 0.025f;
        this.rule1Scale = 0.02;     // 对应 C++: float rule1Scale = 0.02f;
        this.rule2Scale = 0.05;     // 对应 C++: float rule2Scale = 0.05f;
        this.rule3Scale = 0.005;    // 对应 C++: float rule3Scale = 0.005f;
        this.particleCount = kNumParticles; // 对应 C++: uint32_t particleCount = kNumParticles;
    }

    // 返回 Float32Array 用于写入 uniform buffer
    toFloat32Array() {
        const array = new Float32Array(8);  // 7 floats + 1 uint32 = 8 elements
        array[0] = this.deltaT;
        array[1] = this.rule1Distance;
        array[2] = this.rule2Distance;
        array[3] = this.rule3Distance;
        array[4] = this.rule1Scale;
        array[5] = this.rule2Scale;
        array[6] = this.rule3Scale;
        array[7] = this.particleCount;
        return array;
    }
}

class ComputeBoidsSample {
    constructor() {
        // 对应 C++ 成员变量
        this.device = null;
        this.context = null;
        this.canvas = null;
        this.preferredFormat = null;

        // 对应 C++: wgpu::Buffer modelBuffer;
        this.modelBuffer = null;
        
        // 对应 C++: std::array<wgpu::Buffer, 2> particleBuffers;
        this.particleBuffers = [];
        
        // 对应 C++: wgpu::RenderPipeline renderPipeline;
        this.renderPipeline = null;
        
        // 对应 C++: wgpu::Buffer updateParams;
        this.updateParams = null;
        
        // 对应 C++: wgpu::ComputePipeline updatePipeline;
        this.updatePipeline = null;
        
        // 对应 C++: std::array<wgpu::BindGroup, 2> updateBGs;
        this.updateBGs = [];
        
        // 对应 C++: size_t pingpong = 0;
        this.pingpong = 0;
    }

    // 对应 C++ 的 void initBuffers() 方法
    initBuffers() {
        /*
         * 三角形模型顶点数据对应关系：
         * C++: float model[6] = {-0.01, -0.02, 0.01, -0.02, 0.00, 0.02};
         */
        const model = new Float32Array([
            -0.01, -0.02,  // 对应 C++: -0.01f, -0.02f
             0.01, -0.02,  // 对应 C++:  0.01f, -0.02f
             0.00,  0.02   // 对应 C++:  0.00f,  0.02f
        ]);

        /*
         * 模型 buffer 创建对应关系：
         * C++: modelBuffer = dawn::utils::CreateBufferFromData(device, model, sizeof(model),
         *                                                      wgpu::BufferUsage::Vertex);
         */
        this.modelBuffer = this.device.createBuffer({
            size: model.byteLength,                               // 对应 C++: sizeof(model)
            usage: GPUBufferUsage.VERTEX | GPUBufferUsage.COPY_DST, // 对应 C++: wgpu::BufferUsage::Vertex
            mappedAtCreation: true
        });
        new Float32Array(this.modelBuffer.getMappedRange()).set(model);
        this.modelBuffer.unmap();

        /*
         * 初始粒子数据生成对应关系：
         * C++: std::vector<Particle> initialParticles(kNumParticles);
         *      for (auto& p : initialParticles) {
         *          p.pos = glm::vec2(RandomFloat(-1.f, 1.f), RandomFloat(-1.f, 1.f));
         *          p.vel = glm::vec2(RandomFloat(-1.f, 1.f), RandomFloat(-1.f, 1.f)) * 0.1f;
         *      }
         */
        const initialParticles = [];
        for (let i = 0; i < kNumParticles; i++) {
            const particle = new Particle(
                [Math.random() * 2 - 1, Math.random() * 2 - 1],      // 对应 C++: RandomFloat(-1.f, 1.f)
                [(Math.random() * 2 - 1) * 0.1, (Math.random() * 2 - 1) * 0.1]  // 对应 C++: * 0.1f
            );
            initialParticles.push(particle);
        }

        // 将粒子数据转换为 Float32Array
        const particleData = new Float32Array(kNumParticles * 4); // 每个粒子 4 个 float
        for (let i = 0; i < kNumParticles; i++) {
            const particleArray = initialParticles[i].toFloat32Array();
            particleData.set(particleArray, i * 4);
        }

        /*
         * 粒子 buffer 创建对应关系（双缓冲）：
         * C++: for (uint32_t i = 0; i < 2; ++i) {
         *          particleBuffers[i] = dawn::utils::CreateBufferFromData(
         *              device, initialParticles.data(), sizeof(Particle) * kNumParticles,
         *              wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex | wgpu::BufferUsage::Storage);
         *      }
         */
        for (let i = 0; i < 2; i++) {
            this.particleBuffers[i] = this.device.createBuffer({
                size: particleData.byteLength,  // 对应 C++: sizeof(Particle) * kNumParticles
                usage: GPUBufferUsage.STORAGE | GPUBufferUsage.VERTEX | GPUBufferUsage.COPY_DST,
                mappedAtCreation: true
            });
            new Float32Array(this.particleBuffers[i].getMappedRange()).set(particleData);
            this.particleBuffers[i].unmap();
        }

        /*
         * 模拟参数 buffer 创建对应关系：
         * C++: SimParams params = {0.04f, 0.1f, 0.025f, 0.025f, 0.02f, 0.05f, 0.005f, kNumParticles};
         *      updateParams = dawn::utils::CreateBufferFromData(device, &params, sizeof(params),
         *                                                        wgpu::BufferUsage::Uniform);
         */
        const simParams = new SimParams();
        const paramsData = simParams.toFloat32Array();
        
        this.updateParams = this.device.createBuffer({
            size: paramsData.byteLength,                          // 对应 C++: sizeof(params)
            usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST, // 对应 C++: wgpu::BufferUsage::Uniform
            mappedAtCreation: true
        });
        new Float32Array(this.updateParams.getMappedRange()).set(paramsData);
        this.updateParams.unmap();
    }

    // 对应 C++ 的 void initRender() 方法
    initRender() {
        /*
         * 顶点着色器对应关系：
         * C++: wgpu::ShaderModule vsModule = dawn::utils::CreateShaderModule(device, R"(shader_code)");
         */
        const vsModule = this.device.createShaderModule({
            code: `
                struct VertexIn {
                    @location(0) a_particlePos : vec2f,
                    @location(1) a_particleVel : vec2f,
                    @location(2) a_pos : vec2f,
                };

                @vertex
                fn main(input : VertexIn) -> @builtin(position) vec4f {
                    var angle : f32 = -atan2(input.a_particleVel.x, input.a_particleVel.y);
                    var pos : vec2f = vec2f(
                        (input.a_pos.x * cos(angle)) - (input.a_pos.y * sin(angle)),
                        (input.a_pos.x * sin(angle)) + (input.a_pos.y * cos(angle)));
                    return vec4f(pos + input.a_particlePos, 0.0, 1.0);
                }
            `
        });

        /*
         * 片段着色器对应关系：
         * C++: wgpu::ShaderModule fsModule = dawn::utils::CreateShaderModule(device, R"(shader_code)");
         */
        const fsModule = this.device.createShaderModule({
            code: `
                @fragment
                fn main() -> @location(0) vec4f {
                    return vec4f(1.0, 1.0, 1.0, 1.0);
                }
            `
        });

        /*
         * 渲染管线描述符对应关系：
         * C++: dawn::utils::ComboRenderPipelineDescriptor descriptor;
         *      descriptor.vertex.module = vsModule;
         *      descriptor.vertex.bufferCount = 2;
         *      descriptor.cBuffers[0].arrayStride = sizeof(Particle);
         *      descriptor.cBuffers[0].stepMode = wgpu::VertexStepMode::Instance;
         *      ...
         */
        const renderPipelineDescriptor = {
            layout: 'auto',                    // 对应 C++: 自动推断布局
            vertex: {
                module: vsModule,              // 对应 C++: descriptor.vertex.module = vsModule
                entryPoint: 'main',
                buffers: [
                    {                          // 对应 C++: descriptor.cBuffers[0] (Particle buffer)
                        arrayStride: 4 * 4,    // 对应 C++: sizeof(Particle) = 4 floats * 4 bytes
                        stepMode: 'instance',  // 对应 C++: wgpu::VertexStepMode::Instance
                        attributes: [
                            {                  // 对应 C++: descriptor.cAttributes[0] (pos)
                                shaderLocation: 0,
                                offset: 0,     // 对应 C++: offsetof(Particle, pos)
                                format: 'float32x2'  // 对应 C++: wgpu::VertexFormat::Float32x2
                            },
                            {                  // 对应 C++: descriptor.cAttributes[1] (vel)
                                shaderLocation: 1,
                                offset: 2 * 4, // 对应 C++: offsetof(Particle, vel)
                                format: 'float32x2'
                            }
                        ]
                    },
                    {                          // 对应 C++: descriptor.cBuffers[1] (model buffer)
                        arrayStride: 2 * 4,    // 对应 C++: 2 * sizeof(float)
                        stepMode: 'vertex',
                        attributes: [
                            {                  // 对应 C++: descriptor.cAttributes[2]
                                shaderLocation: 2,
                                offset: 0,
                                format: 'float32x2'
                            }
                        ]
                    }
                ]
            },
            fragment: {                        // 对应 C++: descriptor.cFragment.module = fsModule
                module: fsModule,
                entryPoint: 'main',
                targets: [{
                    format: this.preferredFormat  // 对应 C++: GetPreferredSurfaceTextureFormat()
                }]
            },
            primitive: {
                topology: 'triangle-list'      // 默认值
            }
        };

        // 对应 C++: renderPipeline = device.CreateRenderPipeline(&descriptor);
        this.renderPipeline = this.device.createRenderPipeline(renderPipelineDescriptor);
    }

    // 对应 C++ 的 void initSim() 方法
    initSim() {
        /*
         * 计算着色器对应关系：
         * C++: wgpu::ShaderModule module = dawn::utils::CreateShaderModule(device, R"(shader_code)");
         */
        const computeModule = this.device.createShaderModule({
            code: `
                struct Particle {
                    pos : vec2f,
                    vel : vec2f,
                };
                struct SimParams {
                    deltaT : f32,
                    rule1Distance : f32,
                    rule2Distance : f32,
                    rule3Distance : f32,
                    rule1Scale : f32,
                    rule2Scale : f32,
                    rule3Scale : f32,
                    particleCount : u32,
                };
                struct Particles {
                    particles : array<Particle>,
                };
                @binding(0) @group(0) var<uniform> params : SimParams;
                @binding(1) @group(0) var<storage, read_write> particlesA : Particles;
                @binding(2) @group(0) var<storage, read_write> particlesB : Particles;

                // https://github.com/austinEng/Project6-Vulkan-Flocking/blob/master/data/shaders/computeparticles/particle.comp
                @compute @workgroup_size(64)
                fn main(@builtin(global_invocation_id) GlobalInvocationID : vec3u) {
                    var index : u32 = GlobalInvocationID.x;
                    if (index >= params.particleCount) {
                        return;
                    }
                    var vPos : vec2f = particlesA.particles[index].pos;
                    var vVel : vec2f = particlesA.particles[index].vel;
                    var cMass : vec2f = vec2f(0.0, 0.0);
                    var cVel : vec2f = vec2f(0.0, 0.0);
                    var colVel : vec2f = vec2f(0.0, 0.0);
                    var cMassCount : u32 = 0u;
                    var cVelCount : u32 = 0u;
                    var pos : vec2f;
                    var vel : vec2f;

                    for (var i : u32 = 0u; i < params.particleCount; i = i + 1u) {
                        if (i == index) {
                            continue;
                        }

                        pos = particlesA.particles[i].pos.xy;
                        vel = particlesA.particles[i].vel.xy;
                        if (distance(pos, vPos) < params.rule1Distance) {
                            cMass = cMass + pos;
                            cMassCount = cMassCount + 1u;
                        }
                        if (distance(pos, vPos) < params.rule2Distance) {
                            colVel = colVel - (pos - vPos);
                        }
                        if (distance(pos, vPos) < params.rule3Distance) {
                            cVel = cVel + vel;
                            cVelCount = cVelCount + 1u;
                        }
                    }

                    if (cMassCount > 0u) {
                        cMass = (cMass / vec2f(f32(cMassCount), f32(cMassCount))) - vPos;
                    }

                    if (cVelCount > 0u) {
                        cVel = cVel / vec2f(f32(cVelCount), f32(cVelCount));
                    }
                    vVel = vVel + (cMass * params.rule1Scale) + (colVel * params.rule2Scale) +
                        (cVel * params.rule3Scale);

                    // clamp velocity for a more pleasing simulation
                    vVel = normalize(vVel) * clamp(length(vVel), 0.0, 0.1);
                    // kinematic update
                    vPos = vPos + (vVel * params.deltaT);

                    // Wrap around boundary
                    if (vPos.x < -1.0) {
                        vPos.x = 1.0;
                    }
                    if (vPos.x > 1.0) {
                        vPos.x = -1.0;
                    }
                    if (vPos.y < -1.0) {
                        vPos.y = 1.0;
                    }
                    if (vPos.y > 1.0) {
                        vPos.y = -1.0;
                    }

                    // Write back
                    particlesB.particles[index].pos = vPos;
                    particlesB.particles[index].vel = vVel;
                    return;
                }
            `
        });

        /*
         * 绑定组布局创建对应关系：
         * C++: auto bgl = dawn::utils::MakeBindGroupLayout(
         *          device, {
         *              {0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},
         *              {1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},
         *              {2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},
         *          });
         */
        const bindGroupLayout = this.device.createBindGroupLayout({
            entries: [
                {                                     // 对应 C++: binding 0, Uniform
                    binding: 0,
                    visibility: GPUShaderStage.COMPUTE,
                    buffer: { type: 'uniform' }
                },
                {                                     // 对应 C++: binding 1, Storage
                    binding: 1,
                    visibility: GPUShaderStage.COMPUTE,
                    buffer: { type: 'storage' }
                },
                {                                     // 对应 C++: binding 2, Storage
                    binding: 2,
                    visibility: GPUShaderStage.COMPUTE,
                    buffer: { type: 'storage' }
                }
            ]
        });

        /*
         * 计算管线创建对应关系：
         * C++: wgpu::PipelineLayout pl = dawn::utils::MakeBasicPipelineLayout(device, &bgl);
         *      wgpu::ComputePipelineDescriptor csDesc;
         *      csDesc.layout = pl;
         *      csDesc.compute.module = module;
         *      csDesc.compute.entryPoint = "main";
         *      updatePipeline = device.CreateComputePipeline(&csDesc);
         */
        const pipelineLayout = this.device.createPipelineLayout({
            bindGroupLayouts: [bindGroupLayout]       // 对应 C++: MakeBasicPipelineLayout
        });

        this.updatePipeline = this.device.createComputePipeline({
            layout: pipelineLayout,                   // 对应 C++: csDesc.layout = pl
            compute: {
                module: computeModule,                // 对应 C++: csDesc.compute.module = module
                entryPoint: 'main'                    // 对应 C++: csDesc.compute.entryPoint = "main"
            }
        });

        /*
         * 绑定组创建对应关系（双缓冲）：
         * C++: for (uint32_t i = 0; i < 2; ++i) {
         *          updateBGs[i] = dawn::utils::MakeBindGroup(
         *              device, bgl,
         *              {
         *                  {0, updateParams, 0, sizeof(SimParams)},
         *                  {1, particleBuffers[i], 0, kNumParticles * sizeof(Particle)},
         *                  {2, particleBuffers[(i + 1) % 2], 0, kNumParticles * sizeof(Particle)},
         *              });
         *      }
         */
        for (let i = 0; i < 2; i++) {
            this.updateBGs[i] = this.device.createBindGroup({
                layout: bindGroupLayout,              // 对应 C++: bgl
                entries: [
                    {                                 // 对应 C++: binding 0, updateParams
                        binding: 0,
                        resource: { buffer: this.updateParams }
                    },
                    {                                 // 对应 C++: binding 1, particleBuffers[i]
                        binding: 1,
                        resource: { buffer: this.particleBuffers[i] }
                    },
                    {                                 // 对应 C++: binding 2, particleBuffers[(i + 1) % 2]
                        binding: 2,
                        resource: { buffer: this.particleBuffers[(i + 1) % 2] }
                    }
                ]
            });
        }
    }

    // 对应 C++ 的 wgpu::CommandBuffer createCommandBuffer() 方法
    createCommandBuffer(backbufferView, i) {
        // 对应 C++: auto& bufferDst = particleBuffers[(i + 1) % 2];
        const bufferDst = this.particleBuffers[(i + 1) % 2];

        // 对应 C++: wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        const encoder = this.device.createCommandEncoder();

        /*
         * 计算通道对应关系：
         * C++: {
         *          wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
         *          pass.SetPipeline(updatePipeline);
         *          pass.SetBindGroup(0, updateBGs[i]);
         *          pass.DispatchWorkgroups(kNumParticles / 64);
         *          pass.End();
         *      }
         */
        {
            const computePass = encoder.beginComputePass();  // 对应 C++: BeginComputePass()
            computePass.setPipeline(this.updatePipeline);    // 对应 C++: SetPipeline(updatePipeline)
            computePass.setBindGroup(0, this.updateBGs[i]);  // 对应 C++: SetBindGroup(0, updateBGs[i])
            computePass.dispatchWorkgroups(Math.ceil(kNumParticles / 64)); // 对应 C++: DispatchWorkgroups(kNumParticles / 64)
            computePass.end();                               // 对应 C++: End()
        }

        /*
         * 渲染通道对应关系：
         * C++: {
         *          dawn::utils::ComboRenderPassDescriptor renderPass({backbufferView});
         *          wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
         *          pass.SetPipeline(renderPipeline);
         *          pass.SetVertexBuffer(0, bufferDst);
         *          pass.SetVertexBuffer(1, modelBuffer);
         *          pass.Draw(3, kNumParticles);
         *          pass.End();
         *      }
         */
        {
            const renderPassDescriptor = {               // 对应 C++: ComboRenderPassDescriptor
                colorAttachments: [{
                    view: backbufferView,
                    clearValue: { r: 0.0, g: 0.0, b: 0.0, a: 1.0 },
                    loadOp: 'clear',
                    storeOp: 'store'
                }]
            };

            const renderPass = encoder.beginRenderPass(renderPassDescriptor); // 对应 C++: BeginRenderPass
            renderPass.setPipeline(this.renderPipeline);                       // 对应 C++: SetPipeline(renderPipeline)
            renderPass.setVertexBuffer(0, bufferDst);                          // 对应 C++: SetVertexBuffer(0, bufferDst)
            renderPass.setVertexBuffer(1, this.modelBuffer);                   // 对应 C++: SetVertexBuffer(1, modelBuffer)
            renderPass.draw(3, kNumParticles);                                 // 对应 C++: Draw(3, kNumParticles)
            renderPass.end();                                                  // 对应 C++: End()
        }

        // 对应 C++: return encoder.Finish();
        return encoder.finish();
    }

    // 对应 C++ 的 bool SetupImpl() override 方法
    async setupImpl() {
        /*
         * 初始化顺序对应关系：
         * C++: initBuffers();
         *      initRender();
         *      initSim();
         *      return true;
         */
        this.initBuffers();  // 对应 C++: initBuffers();
        this.initRender();   // 对应 C++: initRender();
        this.initSim();      // 对应 C++: initSim();
        return true;         // 对应 C++: return true;
    }

    // 对应 C++ 的 void FrameImpl() override 方法
    frameImpl() {
        /*
         * 帧渲染对应关系：
         * C++: wgpu::SurfaceTexture surfaceTexture;
         *      surface.GetCurrentTexture(&surfaceTexture);
         *      wgpu::CommandBuffer commandBuffer =
         *          createCommandBuffer(surfaceTexture.texture.CreateView(), pingpong);
         *      queue.Submit(1, &commandBuffer);
         *      pingpong = (pingpong + 1) % 2;
         */
        const currentTexture = this.context.getCurrentTexture();  // 对应 C++: GetCurrentTexture
        const commandBuffer = this.createCommandBuffer(
            currentTexture.createView(),                          // 对应 C++: surfaceTexture.texture.CreateView()
            this.pingpong                                         // 对应 C++: pingpong
        );
        this.device.queue.submit([commandBuffer]);               // 对应 C++: queue.Submit(1, &commandBuffer)
        this.pingpong = (this.pingpong + 1) % 2;                 // 对应 C++: pingpong = (pingpong + 1) % 2
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

        // 对应 C++: sample->Run(16000); (运行指定帧数)
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
 *          ComputeBoidsSample* sample = new ComputeBoidsSample();
 *          sample->Run(16000);
 *      }
 */
async function main() {
    if (!navigator.gpu) {
        console.error('WebGPU not supported');
        return 1;
    }

    try {
        const sample = new ComputeBoidsSample();  // 对应 C++: new ComputeBoidsSample()
        await sample.run();                       // 对应 C++: sample->Run(16000)
        console.log('ComputeBoids sample started successfully');
        return 0;
    } catch (error) {
        console.error('Failed to run ComputeBoids sample:', error);
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
    module.exports = ComputeBoidsSample;
}
