# Firefly 游戏引擎 - Requirements Document

创建一个基于 wgpu-native 和 glfw 的现代 C++ 游戏引擎，使用 C++20 模块和 Flecs ECS 架构

## Core Features

### 1. 渲染系统 (Rendering System)
- 基于 wgpu-native 实现跨平台渲染后端
- 支持 Vulkan、Metal、DirectX 12、OpenGL 等后端（由 wgpu-native 自动选择）
- 前向渲染管线（MVP）
- 基础材质系统（MVP），PBR 材质（后期目标）
- 基础光照（方向光、点光源）（MVP），阴影映射（后期目标）
- 后处理效果栈（后期目标：Bloom、Tone Mapping、FXAA 等）
- 延迟渲染管线（后期目标）
- 2D 渲染（后期目标）

### 2. 窗口与输入管理 (Window & Input Management)
- 基于 glfw 实现跨平台窗口管理
- 单窗口渲染（MVP），多窗口渲染（后期目标）
- 完整的输入系统（键盘、鼠标）（MVP），游戏手柄（后期目标）
- 输入映射和动作绑定系统（后期目标）

### 3. ECS 架构 (Entity Component System)
- **使用 Flecs 4.0+ 作为 ECS 框架**
- 实体 (Entity) - 轻量级 ID 标识
- 组件 (Component) - 纯数据容器
- 系统 (System) - 处理逻辑的独立模块
- 世界 (World) - 管理所有实体和系统的容器
- 支持组件的动态添加/移除
- 支持系统依赖和执行顺序控制
- 支持 Flecs 内置的层级系统和变换模块

### 4. 资源管理系统 (Resource Management)
- 同步资源加载（MVP），异步资源加载（后期目标）
- 资源缓存和生命周期管理
- 支持的资源格式：
  - 纹理：PNG、JPEG（MVP），Basis Universal、KTX（后期目标）
  - 模型：GLTF/GLB（MVP），OBJ（后期目标）
  - 着色器：WGSL
- 资源热重载支持（后期目标）

> **注意**：音频系统（WAV、OGG、MP3）不在当前版本范围内，作为后期独立模块规划。

### 5. 场景管理 (Scene Management)
- 场景层级结构（节点树）
- 场景序列化/反序列化
- 预制体 (Prefab) 系统（后期目标）
- 场景切换（MVP），过渡效果（后期目标）

### 6. 脚本系统 (Scripting System)
- 支持 C++ 原生组件扩展
- 预留脚本语言绑定接口（Lua/Python 等）
- 生命周期回调（OnStart、OnUpdate、OnDestroy 等）

### 7. 核心基础设施 (Core Infrastructure)
- 事件系统 (Event System)
- 日志系统 (Logging)
- 配置管理 (Configuration)（后期目标）
- 时间管理 (Delta Time、Fixed Timestep)
- 对象池 (Object Pooling)（后期目标）
- 文件系统抽象 (File System Abstraction)

## User Stories

### 渲染相关
- 作为游戏开发者，我希望能够使用统一的 API 进行跨平台渲染，这样我不需要为每个平台编写不同的渲染代码
- 作为游戏开发者，我希望能够创建 PBR 材质并应用到模型上，以实现逼真的视觉效果
- 作为游戏开发者，我希望能够添加后处理效果，以增强游戏的视觉表现

### ECS 相关
- 作为游戏开发者，我希望能够通过组合组件来创建游戏对象，以实现灵活的游戏逻辑
- 作为游戏开发者，我希望能够创建自定义系统和组件，以扩展引擎功能
- 作为游戏开发者，我希望系统能够按照正确的顺序执行，以确保游戏逻辑的正确性
- 作为游戏开发者，我希望能够使用 Flecs 提供的丰富功能（观察者、触发器、模块等）

### 资源管理相关
- 作为游戏开发者，我希望能够异步加载大型资源，以避免游戏卡顿
- 作为游戏开发者，我希望能够热重载资源，以加速开发迭代
- 作为游戏开发者，我希望能够方便地加载各种格式的资源文件

### 场景管理相关
- 作为游戏开发者，我希望能够通过层级结构组织游戏对象，以方便管理复杂的场景
- 作为游戏开发者，我希望能够保存和加载场景，以实现关卡系统
- 作为游戏开发者，我希望能够创建预制体，以复用游戏对象

### 脚本相关
- 作为游戏开发者，我希望能够在 C++ 中编写游戏逻辑，以获得最佳性能
- 作为游戏开发者，我希望能够通过生命周期回调控制组件行为，以响应游戏事件

## Acceptance Criteria

### 基础框架
- [ ] 项目能够使用 CMake + Ninja 正确构建
- [ ] C++20 模块能够正确编译
- [ ] 能够创建窗口并显示基本内容
- [ ] 渲染管线能够正确初始化 wgpu 设备

### 渲染系统
- [ ] 能够渲染基本几何体（三角形、立方体）
- [ ] 能够加载和渲染 GLTF 模型
- [ ] 能够应用纹理到模型
- [ ] 支持基础材质系统
- [ ] 支持基础光照（方向光、点光源）

### ECS 系统 (Flecs)
- [ ] 能够正确集成 Flecs 库
- [ ] 能够创建和销毁实体
- [ ] 能够添加和移除组件
- [ ] 能够创建和运行系统
- [ ] 支持组件查询
- [ ] 支持实体层级关系

### 资源管理
- [ ] 能够加载 PNG 纹理
- [ ] 能够加载 GLTF 模型
- [ ] 能够加载 WGSL 着色器
- [ ] 实现基本的资源缓存

### 场景管理
- [ ] 能够创建场景层级
- [ ] 实现基本的场景序列化

### 输入系统
- [ ] 能够响应键盘输入
- [ ] 能够响应鼠标输入

## Non-functional Requirements

### 性能要求 (Performance)
- 渲染帧率：在主流硬件上达到 60 FPS
- 资源加载：大型场景加载时间不超过 5 秒
- 内存管理：合理的内存占用，支持资源卸载

### 可移植性 (Portability)
- 支持 Windows 10/11（MVP 首要目标）
- 支持 Linux (Ubuntu 22.04+)（后期目标，受限于 C++20 模块的编译器支持）
- 支持 macOS 12+（后期目标，受限于 C++20 模块的编译器支持）
- 使用 **C++20 标准**
- 使用 **C++20 模块 (Modules)**

> **注意**：C++20 模块在 GCC/Clang + CMake 上的支持仍不成熟。MVP 阶段以 MSVC (Windows) 为主要目标平台，跨平台支持随编译器生态成熟逐步推进。

### 可扩展性 (Extensibility)
- 模块化架构，支持功能扩展
- 插件系统支持（后期目标）
- 自定义渲染管线支持

### 可维护性 (Maintainability)
- 清晰的代码结构和命名规范
- 完整的 API 文档
- 单元测试覆盖核心模块

### 兼容性 (Compatibility)
- wgpu-native 版本：22.0.0+
- glfw 版本：3.4+
- **Flecs 版本：4.0+**
- 支持 Vulkan 1.2+、Metal 2.0+、DirectX 12

## Technical Constraints

### 构建系统
- **CMake 3.28+** (支持 C++20 模块)
- **Ninja** 构建工具
- 第三方依赖直接下载到 **third_party/** 目录，通过 CMake `add_subdirectory` 集成

### 语言标准
- **C++20** (Modules, Concepts, Ranges, std::jthread)
- 代码风格遵循 C++ Core Guidelines

### 版本控制
- 使用 Git 进行版本控制

## Dependencies

### 必需依赖
| 库 | 版本 | 用途 |
|---|---|---|
| wgpu-native | 22.0.0+ | 图形渲染 |
| GLFW | 3.4+ | 窗口管理 |
| **Flecs** | **4.0+** | **ECS 框架** |
| GLM | 0.9.9+ | 数学库 |
| spdlog | 1.12+ | 日志 |
| nlohmann/json | 3.x | JSON 解析 |
| doctest | 2.4+ | 单元测试（header-only） |

### 可选依赖
| 库 | 版本 | 用途 |
|---|---|---|
| ImGui | 1.89+ | 调试 UI |
| Tracy | 0.10+ | 性能分析 |
| stb_image | 2.x | 图像加载 |
| tinygltf | 2.x | GLTF 加载 |
