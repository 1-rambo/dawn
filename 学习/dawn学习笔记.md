### Dawn 集成至 Node.js 的原理

#### WebGPU 翻译到底层 GPU API 技术栈

JavaScript 层 -> N-API 绑定层(?) -> Dawn WebGPU 接口层(C++) -> Dawn 原生内核 -> 后端实现(D3D12/Vulkan/Metal/OpenGL) -> GPU 驱动

将 WebGPU 浏览器/Node.js 调用翻译为对应的 C++ 版本

Chrome 浏览器中: JavaScript -> Blink 引擎(Chrome 渲染) -> GPU 进程(Dawn 所在位置)
不同点：
```
Chrome/Chromium WebGPU:
┌─────────────────────┐
│   Blink (C++)       │ ← Chrome 的渲染引擎
│   └── Dawn Native   │ ← 你的 Dawn 项目核心
└─────────────────────┘

Dawn Node.js:
┌─────────────────────┐
│   N-API Bindings    │ ← 你的 Node.js 绑定
│   └── Dawn Native   │ ← 相同的 Dawn 项目核心
└─────────────────────┘
```

#### 实际代码路径对比

###### Chrome 中的调用：
```
navigator.gpu.requestAdapter()
    ↓
Blink: GPUAdapter::requestAdapter()
    ↓
IPC: WebGPUInterface::RequestAdapter()
    ↓
GPU Process: WebGPUDecoderImpl::RequestAdapter()
    ↓
Dawn Native: wgpu::Instance::RequestAdapter()  ←── 这里！
    ↓
D3D12Backend::CreateAdapter() / VulkanBackend::CreateAdapter()
```

###### 你的 Node.js 项目中的调用：
```
navigator.gpu.requestAdapter()
    ↓
N-API: GPU::requestAdapter()
    ↓
Dawn Native: wgpu::Instance::RequestAdapter()  ←── 相同的代码！
    ↓
Any Backend(D3D12/Vulkan)::CreateAdapter()

```

### 本地 Dawn 项目使用方法
迭代克隆: `git submodule update --init --recursive`
cmake 配置: `cmake -S . -B out/Release -DDAWN_FETCH_DEPENDENCIES=ON -DDAWN_ENABLE_INSTALL=ON -DCMAKE_BUILD_TYPE=Release`
cmake 构建: `cmake --build out/Release`
测试方法: 使用 `./src/dawn/samples` 文件夹中的 webgpu C++ 文件进行测试，重新build并运行对应 `./out/Release` 中的可执行文件即可
目前 `samples` 文件夹中的文件效果：
- `Animometer.cpp` 为 MotionMark，配套 fps 输出
- `ComputeBoids.cpp` 若干白色箭头运动
- `DawnInfo.cpp` 输出 dawn 的若干参数含义及取值
- `HelloTriangle.cpp` 渲染三角形
- `ManualSurfaceTest.cpp`
- `SampleUtils.cpp` 基类，初始化代码，不生成 tests

一套 WebGPU 代码可以在原生/Web 环境中运行，使用 Cmake 在原生环境运行，emcc 在 Web 环境中运行

**Emscripten 编译命令:**
```bash
emcc HelloTriangle.cpp SampleUtils.cpp \
    -I../include -I../src \
    -s USE_WEBGPU=1 -s WASM=1 \
    --shell-file shell.html \
    -o HelloTriangle.html
```

#### Dawn 的 WebGPU → GPU 后端转换

**Dawn 的真正功能**：将所有 WebGPU API 调用转换为底层 GPU API (Vulkan/D3D12/Metal)

##### 完整转换流程，包括 HelloTriangle.cpp, Animometer.cpp

1. `InitSample()` 初始化，主要进行参数求解
2. `new HelloTriangle()/Animometer()` 设置静态变量 `sample = this`
3. `SampleUtils.Run()` 各种初始化，分为桌面环境与 Web 环境
  - `sample.Setup()` 通用 setup，配置画布等信息
    - `SetupImpl()` 多态，根据场景调用，本场景为创建顶点缓冲区、着色器代码与渲染管线
  - 循环
    - `sample.FrameImpl()` 多态，根据场景调用，本场景为创建命令缓冲区
    - `sample->surface.Present()`

Tips: 如果后端使用 `--vulkan` 可取消 fps 限制，默认后端为 `--d3d12`

**1. 主要代码翻译：以 WebGPU Buffer 创建为例**
```
// HelloTriangle.cpp
vertexBuffer = dawn::utils::CreateBufferFromData(device, vertexData, sizeof(vertexData), wgpu::BufferUsage::Vertex);

// Dawn 转换过程
1. 应用层调用
   wgpu::Device::CreateBuffer(BufferDescriptor const * descriptor)
   ↓ [C++ wrapper, 位于 webgpu_cpp.h, 生成代码]
2. C API 调用  
   wgpuDeviceCreateBuffer(Get(), reinterpret_cast<WGPUBufferDescriptor const *>(descriptor))
   ↓ [位于 webgpu_cpp.cpp, 生成代码]
3. 函数指针分发
   procs.deviceCreateBuffer(device, descriptor)
   ↓ [由 dawn_proc.cpp 模板经 jinja2 生成函数指针表，运行时分发]
   [其中 procs 是全局函数表，通过调用 dawnProcSetProcs 函数实现，这个函数的绑定根据不同环境有不同实现，native 环境下它绑定到 DawnNative.cpp 中]
4. Native 实现入口
   NativeDeviceCreateBuffer(cSelf, descriptor)
   ↓ [由 ProcTable.cpp 模板经 jinja2 生成代码，在 out/Release/gen/src/native/ProcTable.cpp 中]
5. 类型转换 + 调用
   self->APICreateBuffer(descriptor_)
   ↓ [即 DeviceBase::APICreateBuffer，在 Device.cpp 中]
6. 后端虚函数分发
   CreateBufferImpl(descriptor)
   ↓ [虚函数，根据后端类型分发]
8. 后端具体实现
   Device::CreateBufferImpl() → Buffer::Create()
   ↓ [BufferVk.cpp/BufferD3D12.cpp/BufferMTL.mm]
9. 底层 GPU API
   vkCreateBuffer() / ID3D12Device::CreateCommittedResource() / MTLDevice::newBufferWithLength()
```

Q: 如何选择的后端类型？
A: `Device::CreateBuffer()` 里调用的 `Get()` 函数决定了后端的具体类型。深层次地来看，后端类型选择发生在设备创建时，具体在 `InstanceBase::EnumeratePhysicalDevices()` 中。`Get()` 函数只是返回已经创建好的 C API 句柄。
Q: 很多调用没有找到函数实现，是为什么？
A: 许多代码通过 jinja2 模板生成相关代码

**后端选择的真正时机：**
1. `wgpuInstanceRequestAdapter()` 调用时
2. `InstanceBase::EnumeratePhysicalDevices()` 根据 `RequestAdapterOptions::backendType` 选择
3. `GetBackendConnection()` 创建对应后端连接 (vulkan::Connect, d3d12::Connect 等)
4. 创建的设备对象已经是特定后端类型 (DeviceVk*, DeviceD3D12* 等)
5. `Get()` 只是返回这个已确定类型的设备句柄

**2. WGSL Shader 编译 (Tint 子组件)**
```cpp
// HelloTriangle.cpp - 只是 Dawn 功能的一小部分
wgpu::ShaderModule module = dawn::utils::CreateShaderModule(device, wgslSource);

// Tint 处理 (Dawn 的子组件)
Vulkan: WGSL → SPIRV (src/tint/lang/spirv/writer/)
D3D12: WGSL → HLSL (src/tint/lang/hlsl/writer/)
Metal: WGSL → MSL (src/tint/lang/msl/writer/)
```

##### **Dawn 架构总览**
```
WebGPU API 调用 (HelloTriangle.cpp)
    ↓
Dawn Native (src/dawn/native/)
    ├── Device 管理
    ├── Buffer/Texture 资源管理  
    ├── Pipeline 状态管理
    ├── Command 记录和执行
    └── Tint (shader 编译子组件)
    ↓
后端实现 (src/dawn/native/[vulkan|d3d12|metal]/)
    ↓
底层 GPU API (Vulkan/D3D12/Metal)
```

#### dawn_json_generator.py
用于将 dawn.json 转换为内存形式代码，提供给 jinja2 模板，生成各种代码（头文件、Wire协议代码等等）

### 代码生成系统：模板与生成文件对应关系

Dawn使用Jinja2模板引擎和dawn_json_generator.py脚本来生成代码。以下是主要模板与生成文件的对应关系：

| 模板文件 | 生成文件 | 功能描述 |
|---------|---------|---------|
| generator/templates/dawn/native/ProcTable.cpp | - | 生成Native的实现入口 |
| generator/templates/dawn/native/api_dawn_native_proc.cpp | src/dawn/native/webgpu_dawn_native_proc.cpp | 生成WebGPU Native的C API入口，包含`NativeDeviceCreateBuffer`等函数 |
| generator/templates/dawn/dawn_proc.cpp | src/dawn/dawn_proc.cpp | 生成全局函数指针表和`wgpuDeviceCreateBuffer`等函数 |
| generator/templates/dawn/dawn_thread_dispatch_proc.cpp | src/dawn/dawn_thread_dispatch_proc.cpp | 生成线程分发相关代码 |

#### 代码生成目标

dawn_json_generator.py支持多个生成目标（targets）：

- **proc**: 生成dawn_proc.cpp和dawn_thread_dispatch_proc.cpp
- **webgpu_dawn_native_proc**: 生成webgpu_dawn_native_proc.cpp
- **cpp_headers**: 生成C++ API头文件
- **dawn_headers**: 生成Dawn C API头文件
- **wire**: 生成Dawn Wire协议代码
- **native_utils**: 生成Dawn Native工具代码

#### 生成命令示例

以下命令可以生成Dawn Native的接口代码：
```bash
python generator/dawn_json_generator.py --dawn-json src/dawn/dawn.json --wire-json src/dawn/dawn_wire.json --template-dir generator/templates --output-dir generated_files --targets webgpu_dawn_native_proc
```

#### webgpu_cpp.h
由 dawn.json 文件生成，作为 C++ WebGPU 头文件
文件中声明定义了许多实体类，如 `Device`, `Pipeline` 等等，它们都继承自 `ObjectBase` 模板类。
具体而言，它是一个 CRTP 模式的基类，供所有子类继承，除了 `Derived` 模板参数外，它还有 `CType` 模板参数，用于C风格的对应指针参数传递。
例如，`class Device : public ObjectBase<Device, WGPUDevice>`
