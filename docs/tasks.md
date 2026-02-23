# Firefly 游戏引擎 - Task List

## Implementation Tasks

### Phase 1: 项目基础设施

- [ ] 1. **项目初始化与构建系统**
    - [ ] 1.1. 创建项目目录结构
        - *Goal*: 建立完整的源码、测试、示例目录结构
        - *Details*: 创建 src/, tests/, examples/, third_party/ 等目录，按模块划分
        - *Requirements*: 设计文档目录结构
    - [ ] 1.2. 配置 CMake 构建系统
        - *Goal*: 设置 CMake + Ninja 构建环境
        - *Details*: 创建 CMakeLists.txt, CMakePresets.json，配置 C++20 模块支持
        - *Requirements*: C++20, Ninja, CMake 3.28+
    - [ ] 1.3. 配置 vcpkg 依赖管理
        - *Goal*: 设置第三方库依赖
        - *Details*: 创建 vcpkg.json，配置 glfw, flecs, glm, spdlog 等依赖
        - *Requirements*: 依赖列表
    - [ ] 1.4. 配置 wgpu-native
        - *Goal*: 集成 wgpu-native 库
        - *Details*: 下载或编译 wgpu-native，配置头文件和库文件路径
        - *Requirements*: wgpu-native 22.0+
    - [ ] 1.5. 验证构建系统
        - *Goal*: 确保项目能够成功构建
        - *Details*: 运行 cmake --preset=default && cmake --build --preset=debug

### Phase 2: 核心模块 (Core Module)

- [ ] 2. **Core 模块 - 基础类型**
    - [ ] 2.1. 实现基础类型定义 (types.cppm)
        - *Goal*: 定义引擎使用的基础类型别名和工具类
        - *Details*: i8/i16/i32/i64, u8/u16/u32/u64, f32/f64, String, Ptr, SharedPtr, Result, ScopeExit
        - *Requirements*: 设计文档 types.cppm
    - [ ] 2.2. 实现日志系统 (logger.cppm)
        - *Goal*: 基于 spdlog 的日志封装
        - *Details*: 支持 Trace/Debug/Info/Warn/Error/Fatal 级别，支持文件和控制台输出
        - *Requirements*: 日志系统
    - [ ] 2.3. 实现时间管理 (time.cppm)
        - *Goal*: 提供高精度时间和帧时间管理
        - *Details*: DeltaTime, FixedTimestep, FPS 计数，使用 std::chrono
        - *Requirements*: 时间管理

- [ ] 3. **Core 模块 - 事件系统**
    - [ ] 3.1. 实现事件基类和分发器 (event.cppm)
        - *Goal*: 提供灵活的事件订阅/发布机制
        - *Details*: Event 基类, EventDispatcher 模板, 常用事件类型
        - *Requirements*: 事件系统

- [ ] 4. **Core 模块 - 应用程序框架**
    - [ ] 4.1. 实现 Application 类 (application.cppm)
        - *Goal*: 提供应用程序生命周期管理
        - *Details*: OnInit/OnUpdate/OnRender/OnShutdown 回调，主循环，配置加载
        - *Requirements*: Application 设计
    - [ ] 4.2. 实现 Engine 单例 (engine.cppm)
        - *Goal*: 提供全局引擎访问点
        - *Details*: 单例模式，管理核心系统实例
        - *Requirements*: Engine 设计

### Phase 3: 平台模块 (Platform Module)

- [ ] 5. **Platform 模块 - 窗口管理**
    - [ ] 5.1. 实现 Window 类 (window.cppm)
        - *Goal*: 封装 GLFW 窗口操作
        - *Details*: 窗口创建、销毁、事件回调、VSync、全屏切换
        - *Requirements*: 窗口管理
    - [ ] 5.2. 实现窗口事件处理
        - *Goal*: 将 GLFW 回调转换为引擎事件
        - *Details*: 窗口大小变化、键盘、鼠标事件
        - *Requirements*: 事件系统

- [ ] 6. **Platform 模块 - 输入系统**
    - [ ] 6.1. 实现 Input 静态类 (input.cppm)
        - *Goal*: 提供全局输入状态查询
        - *Details*: 键盘状态、鼠标位置和按钮、滚轮、游戏手柄
        - *Requirements*: 输入系统

### Phase 4: ECS 模块 (Flecs 封装)

- [ ] 7. **ECS 模块 - Flecs 封装**
    - [ ] 7.1. 实现 World 封装 (world.cppm)
        - *Goal*: 封装 Flecs world 提供更友好的 API
        - *Details*: create_entity, add_component, get_component, query, system
        - *Requirements*: Flecs 集成
    - [ ] 7.2. 定义内置组件 (components.cppm)
        - *Goal*: 定义引擎常用的组件类型
        - *Details*: Transform, MeshComponent, CameraComponent, LightComponent, NameComponent
        - *Requirements*: 组件定义
    - [ ] 7.3. 实现内置系统 (systems.cppm)
        - *Goal*: 实现引擎内置的 ECS 系统
        - *Details*: TransformSystem, RenderCollectSystem, ScriptSystem
        - *Requirements*: 系统定义

### Phase 5: 渲染模块 (Renderer Module)

- [ ] 8. **Renderer 模块 - WGPU 设备封装**
    - [ ] 8.1. 实现 RenderDevice (device.cppm)
        - *Goal*: 封装 WGPU Instance/Adapter/Device/Surface
        - *Details*: 初始化、资源创建、错误处理
        - *Requirements*: WGPU 封装
    - [ ] 8.2. 实现 SwapChain 管理
        - *Goal*: 管理交换链和帧呈现
        - *Details*: 创建 SwapChain，获取当前纹理，Present
        - *Requirements*: 帧呈现

- [ ] 9. **Renderer 模块 - 资源类型**
    - [ ] 9.1. 实现 Buffer (buffer.cppm)
        - *Goal*: 封装 WGPU Buffer
        - *Details*: VertexBuffer, IndexBuffer, UniformBuffer, StagingBuffer
        - *Requirements*: 缓冲区管理
    - [ ] 9.2. 实现 Texture (texture.cppm)
        - *Goal*: 封装 WGPU Texture 和 TextureView
        - *Details*: 2D 纹理、深度纹理、采样器
        - *Requirements*: 纹理管理
    - [ ] 9.3. 实现 Shader (shader.cppm)
        - *Goal*: 封装 WGSL 着色器加载和编译
        - *Details*: 从文件加载 WGSL，创建 ShaderModule
        - *Requirements*: 着色器系统

- [ ] 10. **Renderer 模块 - 渲染管线**
    - [ ] 10.1. 实现 CommandBuffer (command.cppm)
        - *Goal*: 封装 WGPU CommandEncoder 和 RenderPass
        - *Details*: 开始/结束渲染通道，绑定资源，绘制命令
        - *Requirements*: 命令录制
    - [ ] 10.2. 实现 RenderPipeline (pipeline.cppm)
        - *Goal*: 封装 WGPU RenderPipeline
        - *Details*: 管线状态对象，混合模式，深度测试
        - *Requirements*: 渲染管线
    - [ ] 10.3. 实现 Material (material.cppm)
        - *Goal*: 材质系统，管理着色器和参数
        - *Details*: MaterialProperties, BindGroup 创建和更新
        - *Requirements*: 材质系统

- [ ] 11. **Renderer 模块 - 网格和相机**
    - [ ] 11.1. 实现 Mesh (mesh.cppm)
        - *Goal*: 网格数据容器
        - *Details*: VertexArray, IndexBuffer, 子网格，包围盒
        - *Requirements*: 网格系统
    - [ ] 11.2. 实现 Camera (camera.cppm)
        - *Goal*: 相机视图和投影管理
        - *Details*: ViewMatrix, ProjectionMatrix, 视锥体
        - *Requirements*: 相机系统

- [ ] 12. **Renderer 模块 - 高层渲染器**
    - [ ] 12.1. 实现 Renderer 类 (renderer.cppm)
        - *Goal*: 统一的渲染接口
        - *Details*: BeginFrame, EndFrame, DrawMesh, SetCamera, 灯光管理
        - *Requirements*: 渲染器接口

### Phase 6: 资源模块 (Resource Module)

- [ ] 13. **Resource 模块 - 资源管理**
    - [ ] 13.1. 实现 Handle 和 ResourceManager (manager.cppm)
        - *Goal*: 类型安全的资源句柄和管理器
        - *Details*: Handle<T>, Load, LoadAsync, Get, Unload, 引用计数
        - *Requirements*: 资源管理

- [ ] 14. **Resource 模块 - 导入器**
    - [ ] 14.1. 实现纹理导入器 (texture.cppm)
        - *Goal*: 加载图片文件为 GPU 纹理
        - *Details*: 使用 stb_image 加载 PNG/JPEG，上传到 GPU
        - *Requirements*: 纹理加载
    - [ ] 14.2. 实现网格导入器 (mesh.cppm)
        - *Goal*: 加载 GLTF 模型
        - *Details*: 使用 tinygltf 解析，转换为引擎 Mesh 格式
        - *Requirements*: 模型加载

### Phase 7: 场景模块 (Scene Module)

- [ ] 15. **Scene 模块 - 场景管理**
    - [ ] 15.1. 实现 Scene 和 SceneNode (scene_manager.cppm)
        - *Goal*: 场景层级结构管理
        - *Details*: 节点树，父子关系，变换累积，与 Flecs 层级集成
        - *Requirements*: 场景管理
    - [ ] 15.2. 实现场景序列化
        - *Goal*: 场景的保存和加载
        - *Details*: JSON 序列化，实体和组件的序列化
        - *Requirements*: 场景序列化

### Phase 8: 示例和测试

- [ ] 16. **示例程序**
    - [ ] 16.1. Hello Triangle 示例
        - *Goal*: 验证基础渲染功能
        - *Details*: 初始化引擎，创建三角形，渲染循环
        - *Requirements*: 基础框架验证
    - [ ] 16.2. Basic Scene 示例
        - *Goal*: 验证完整功能
        - *Details*: 加载模型，创建相机和灯光，ECS 系统
        - *Requirements*: 完整功能验证

- [ ] 17. **单元测试**
    - [ ] 17.1. Core 模块测试
        - *Goal*: 测试核心功能
        - *Details*: 测试 Event, Time, Result 等基础组件
        - *Requirements*: 测试覆盖率 80%+
    - [ ] 17.2. ECS 模块测试
        - *Goal*: 测试 Flecs 封装
        - *Details*: 测试 Entity 创建、组件添加、系统执行
        - *Requirements*: 测试覆盖率 90%+

## Task Dependencies

```
Phase 1 (基础设施)
    │
    ├──▶ Phase 2 (Core 模块) ──▶ Phase 3 (Platform 模块)
    │         │                         │
    │         └─────────────────────────┼──▶ Phase 4 (ECS 模块)
    │                                   │
    └───────────────────────────────────┴──▶ Phase 5 (Renderer 模块)
                                                    │
                              Phase 6 (Resource) ◀──┘
                                    │
                              Phase 7 (Scene) ◀─────┘
                                    │
                              Phase 8 (示例/测试) ◀──┘
```

### 并行执行
- Phase 2 (Core) 和 Phase 3 (Platform) 可以部分并行
- Phase 6 (Resource) 和 Phase 7 (Scene) 可以并行
- 各模块的测试可以在模块完成后立即进行

### 关键路径
1. Phase 1 → Phase 2 → Phase 4 → Phase 5 → Phase 8
2. Phase 1 → Phase 3 → Phase 5

## Priority Order

### P0 - 必须完成 (MVP)
- 1.1 ~ 1.5 (项目基础设施)
- 2.1, 2.2 (基础类型、日志)
- 3.1 (事件系统)
- 4.1 (Application)
- 5.1 (Window)
- 7.1, 7.2 (ECS 基础)
- 8.1, 9.1, 9.3, 10.1 (渲染基础)
- 12.1 (Renderer)
- 16.1 (Hello Triangle)

### P1 - 重要功能
- 6.1 (Input)
- 9.2, 10.2, 10.3 (Texture, Pipeline, Material)
- 11.1, 11.2 (Mesh, Camera)
- 13.1, 14.1, 14.2 (Resource)
- 16.2 (Basic Scene)

### P2 - 增强功能
- 2.3 (Time)
- 4.2 (Engine 单例)
- 5.2 (窗口事件)
- 7.3 (内置系统)
- 8.2 (SwapChain)
- 15.1, 15.2 (Scene)
- 17.1, 17.2 (测试)

## Notes

- 每个 Task 应该有明确的完成标准
- 完成一个 Task 后立即进行代码审查
- 优先保证核心功能可工作，再逐步扩展
- 保持模块间的接口稳定
