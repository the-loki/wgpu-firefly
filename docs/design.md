# Firefly 游戏引擎 - Design Document

## Overview

Firefly 采用分层架构设计，使用 **C++20 模块 (Modules)** 构建现代游戏引擎。核心设计原则包括：

- **模块化**：使用 C++20 模块实现物理隔离，编译速度更快，依赖更清晰
- **数据驱动**：使用 Flecs ECS 架构将数据与逻辑分离
- **跨平台**：通过抽象层隔离平台差异
- **性能优先**：支持多线程渲染和异步资源加载

### 技术栈
- **语言**：C++20 (Modules, Concepts, Ranges, Coroutines)
- **构建系统**：CMake 3.28+ (支持 C++20 模块)
- **渲染后端**：wgpu-native 22.0+
- **窗口管理**：GLFW 3.4+
- **ECS 框架**：Flecs 4.0+
- **依赖管理**：vcpkg / Conan
- **数学库**：GLM 或 DxLib (DirectXMath 包装)

## Architecture

### 分层架构图

```
┌─────────────────────────────────────────────────────────────┐
│                      Application Layer                       │
│                    (Game Logic / Scripts)                    │
├─────────────────────────────────────────────────────────────┤
│                      Engine Core Layer                       │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐       │
│  │  Flecs   │ │  Scene   │ │  Input   │ │  Script  │       │
│  │   ECS    │ │ Manager  │ │  System  │ │  System  │       │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘       │
├─────────────────────────────────────────────────────────────┤
│                    Resource Layer                            │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐        │
│  │   Asset      │ │   Resource   │ │    Asset     │        │
│  │   Loader     │ │   Cache      │ │   Importer   │        │
│  └──────────────┘ └──────────────┘ └──────────────┘        │
├─────────────────────────────────────────────────────────────┤
│                    Rendering Layer                           │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐       │
│  │ Render   │ │ Material │ │  Shader  │ │  Post    │       │
│  │ Pipeline │ │  System  │ │  System  │ │ Process  │       │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘       │
├─────────────────────────────────────────────────────────────┤
│                    Platform Layer                            │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐       │
│  │  Window  │ │  Graphics│ │   File   │ │  Threading      │
│  │ (GLFW)   │ │  (WGPU)  │ │   I/O    │ │  (std::jthread) │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘       │
└─────────────────────────────────────────────────────────────┘
```

### C++20 模块结构

```
firefly/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── docs/
│   └── spec/
│       ├── requirements.md
│       └── design.md
├── src/
│   ├── firefly/                    # 主模块
│   │   ├── firefly.cppm            # 主模块接口文件
│   │   └── detail/                 # 模块内部实现
│   │
│   ├── core/                       # Core 模块
│   │   ├── core.cppm               # 模块接口
│   │   ├── application.cppm/.cpp   # Application
│   │   ├── engine.cppm/.cpp        # Engine 单例
│   │   ├── event.cppm/.cpp         # 事件系统
│   │   ├── logger.cppm/.cpp        # 日志系统
│   │   ├── time.cppm/.cpp          # 时间管理
│   │   └── types.cppm              # 基础类型定义
│   │
│   ├── platform/                   # Platform 模块
│   │   ├── platform.cppm           # 模块接口
│   │   ├── window.cppm/.cpp        # 窗口管理
│   │   ├── input.cppm/.cpp         # 输入系统
│   │   └── file_system.cppm/.cpp   # 文件系统
│   │
│   ├── ecs/                        # ECS 模块 (Flecs 封装)
│   │   ├── ecs.cppm                # 模块接口
│   │   ├── world.cppm/.cpp         # Flecs World 封装
│   │   ├── components.cppm         # 内置组件定义
│   │   ├── systems.cppm            # 内置系统定义
│   │   └── modules/                # Flecs 模块封装
│   │       ├── transform.cppm/.cpp
│   │       ├── hierarchy.cppm/.cpp
│   │       └── script.cppm/.cpp
│   │
│   ├── renderer/                   # Renderer 模块
│   │   ├── renderer.cppm           # 模块接口
│   │   ├── device.cppm/.cpp        # WGPU 设备封装
│   │   ├── pipeline.cppm/.cpp      # 渲染管线
│   │   ├── command.cppm/.cpp       # 命令缓冲
│   │   ├── buffer.cppm/.cpp        # 缓冲区
│   │   ├── texture.cppm/.cpp       # 纹理
│   │   ├── shader.cppm/.cpp        # 着色器
│   │   ├── material.cppm/.cpp      # 材质
│   │   ├── mesh.cppm/.cpp          # 网格
│   │   ├── camera.cppm/.cpp        # 相机
│   │   └── lighting.cppm/.cpp      # 光照
│   │
│   ├── scene/                      # Scene 模块
│   │   ├── scene.cppm              # 模块接口
│   │   ├── scene_manager.cppm/.cpp # 场景管理器
│   │   ├── scene_node.cppm/.cpp    # 场景节点
│   │   └── prefab.cppm/.cpp        # 预制体
│   │
│   ├── resource/                   # Resource 模块
│   │   ├── resource.cppm           # 模块接口
│   │   ├── manager.cppm/.cpp       # 资源管理器
│   │   ├── cache.cppm/.cpp         # 资源缓存
│   │   ├── loader.cppm/.cpp        # 加载器基类
│   │   └── importers/              # 导入器
│   │       ├── texture.cppm/.cpp
│   │       ├── mesh.cppm/.cpp
│   │       └── shader.cppm/.cpp
│   │
│   └── scripting/                  # Scripting 模块
│       ├── scripting.cppm          # 模块接口
│       └── native_script.cppm/.cpp # 原生脚本
│
├── tests/                          # 单元测试
│   ├── CMakeLists.txt
│   ├── core/
│   ├── ecs/
│   └── renderer/
│
├── examples/                       # 示例程序
│   ├── hello_triangle/
│   └── basic_scene/
│
└── third_party/                    # 第三方库
    ├── flecs/
    ├── glfw/
    └── ...
```

## Components and Interfaces

### 1. Core Module

#### 模块接口 (core.cppm)
```cpp
export module firefly.core;

// 导出所有核心组件
export import firefly.core.types;
export import firefly.core.application;
export import firefly.core.engine;
export import firefly.core.event;
export import firefly.core.logger;
export import firefly.core.time;
```

#### 基础类型 (types.cppm)
```cpp
export module firefly.core.types;

export import std;

export namespace firefly {
    // 基础类型别名
    using i8  = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;
    using u8  = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;
    using f32 = float;
    using f64 = double;

    // 字符串类型
    using String = std::string;
    using StringView = std::string_view;

    // 智能指针
    template<typename T>
    using Ptr = std::unique_ptr<T>;

    template<typename T>
    using SharedPtr = std::shared_ptr<T>;

    template<typename T>
    using WeakPtr = std::weak_ptr<T>;

    // 结果类型
    template<typename T, typename E = String>
    class Result {
    public:
        constexpr bool is_ok() const noexcept { return m_value.has_value(); }
        constexpr bool is_error() const noexcept { return !is_ok(); }
        constexpr auto& value() { return *m_value; }
        constexpr auto& error() { return m_error; }

        static constexpr Result ok(T value) { return Result{std::move(value), {}}; }
        static constexpr Result error(E error) { return Result{{}, std::move(error)}; }

    private:
        std::optional<T> m_value;
        E m_error;
    };

    // Scope guard
    template<typename F>
    class ScopeExit {
    public:
        explicit ScopeExit(F&& f) : m_func(std::forward<F>(f)) {}
        ~ScopeExit() { m_func(); }
    private:
        F m_func;
    };
}
```

#### Application (application.cppm)
```cpp
export module firefly.core.application;

import std;
import firefly.core.types;
import firefly.core.event;
import firefly.platform.window;
import firefly.renderer.renderer;
import firefly.ecs.world;

export namespace firefly {

struct ApplicationConfig {
    String title = "Firefly Engine";
    u32 width = 1280;
    u32 height = 720;
    bool fullscreen = false;
    bool vsync = true;
    bool resizable = true;
    bool enableValidation = true;
};

class Application {
public:
    explicit Application(const ApplicationConfig& config);
    virtual ~Application();

    // 禁止拷贝
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // 生命周期回调
    virtual void on_init() {}
    virtual void on_shutdown() {}
    virtual void on_update(f32 deltaTime) {}
    virtual void on_render() {}
    virtual void on_imgui_render() {}

    // 运行主循环
    void run();

    // 访问器
    auto window() -> Window& { return *m_window; }
    auto renderer() -> Renderer& { return *m_renderer; }
    auto world() -> ecs::World& { return *m_world; }

    static auto instance() -> Application&;

protected:
    Ptr<Window> m_window;
    Ptr<Renderer> m_renderer;
    Ptr<ecs::World> m_world;
    bool m_running = true;
};

} // namespace firefly

// 应用程序入口点宏
#define FIREFLY_APP_MAIN(AppClass) \
    auto main(int argc, char** argv) -> int { \
        AppClass app; \
        app.run(); \
        return 0; \
    }
```

#### Event System (event.cppm)
```cpp
export module firefly.core.event;

import std;
import firefly.core.types;

export namespace firefly {

// 事件基类
struct Event {
    virtual ~Event() = default;
    virtual auto to_string() const -> String = 0;
    bool handled = false;
};

// 使用 std::function 和多态的事件分发器
class EventDispatcher {
public:
    template<std::derived_from<Event> T, typename F>
    void subscribe(F&& callback) {
        auto& handlers = get_handlers<T>();
        handlers.push_back(std::function<void(T&)>(std::forward<F>(callback)));
    }

    template<std::derived_from<Event> T>
    void publish(T& event) {
        auto& handlers = get_handlers<T>();
        for (auto& handler : handlers) {
            handler(event);
            if (event.handled) break;
        }
    }

private:
    template<std::derived_from<Event> T>
    auto get_handlers() -> std::vector<std::function<void(T&)>>& {
        auto key = typeid(T).hash_code();
        if (!m_handlers.contains(key)) {
            m_handlers[key] = std::vector<std::function<void(T&)>>();
        }
        return std::any_cast<std::vector<std::function<void(T&)>>&>(m_handlers[key]);
    }

    std::unordered_map<size_t, std::any> m_handlers;
};

// 常用事件类型
struct WindowResizeEvent : Event {
    u32 width, height;
    auto to_string() const -> String override {
        return std::format("WindowResizeEvent({}, {})", width, height);
    }
};

struct KeyEvent : Event {
    int key, scancode, action, mods;
    auto to_string() const -> String override {
        return std::format("KeyEvent(key={}, action={})", key, action);
    }
};

struct MouseMoveEvent : Event {
    f64 x, y;
    f64 dx, dy; // delta
    auto to_string() const -> String override {
        return std::format("MouseMoveEvent({}, {})", x, y);
    }
};

} // namespace firefly
```

### 2. Platform Module

#### Window (window.cppm)
```cpp
export module firefly.platform.window;

import std;
import firefly.core.types;
import firefly.core.event;

// 前向声明 GLFW 类型
struct GLFWwindow;

export namespace firefly {

struct WindowConfig {
    String title = "Firefly Engine";
    u32 width = 1280;
    u32 height = 720;
    bool fullscreen = false;
    bool vsync = true;
    bool resizable = true;
};

class Window {
public:
    explicit Window(const WindowConfig& config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void poll_events();
    void swap_buffers();
    auto should_close() const -> bool;

    auto width() const -> u32 { return m_config.width; }
    auto height() const -> u32 { return m_config.height; }
    auto native_handle() const -> GLFWwindow* { return m_window; }

    // 事件分发器
    auto events() -> EventDispatcher& { return m_eventDispatcher; }

private:
    GLFWwindow* m_window = nullptr;
    WindowConfig m_config;
    EventDispatcher m_eventDispatcher;

    static void on_glfw_resize(GLFWwindow*, int, int);
    static void on_glfw_key(GLFWwindow*, int, int, int, int);
    static void on_glfw_mouse(GLFWwindow*, double, double);
};

} // namespace firefly
```

#### Input (input.cppm)
```cpp
export module firefly.platform.input;

import std;
import firefly.core.types;

export namespace firefly {

enum class Key : int {
    Unknown = 0,
    A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    // ... 其他键
};

enum class MouseButton : int {
    Left = 0,
    Right = 1,
    Middle = 2
};

class Input {
public:
    static void init();
    static void update();

    // 键盘状态
    static auto is_key_down(Key key) -> bool;
    static auto is_key_pressed(Key key) -> bool;
    static auto is_key_released(Key key) -> bool;

    // 鼠标状态
    static auto is_mouse_down(MouseButton button) -> bool;
    static auto mouse_position() -> std::pair<f64, f64>;
    static auto mouse_delta() -> std::pair<f64, f64>;
    static auto mouse_scroll() -> f64;

    // 游戏手柄
    static auto is_gamepad_connected(int gamepad) -> bool;
    static auto gamepad_axis(int gamepad, int axis) -> f64;

private:
    static inline std::array<bool, 350> s_currentKeys{};
    static inline std::array<bool, 350> s_previousKeys{};
    static inline std::pair<f64, f64> s_mousePos{};
    static inline std::pair<f64, f64> s_mouseDelta{};
    static inline f64 s_mouseScroll{};
};

} // namespace firefly
```

### 3. ECS Module (Flecs 封装)

#### World (world.cppm)
```cpp
export module firefly.ecs.world;

import std;
import flecs;
import firefly.core.types;

export namespace firefly::ecs {

// Flecs World 的 C++20 封装
class World {
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // 实体管理
    auto create_entity(const String& name = "") -> flecs::entity;
    void destroy_entity(flecs::entity entity);

    // 组件操作 (模板函数)
    template<typename T, typename... Args>
    auto add_component(flecs::entity entity, Args&&... args) -> T& {
        return entity.set<T>({std::forward<Args>(args)...});
    }

    template<typename T>
    void remove_component(flecs::entity entity) {
        entity.remove<T>();
    }

    template<typename T>
    auto get_component(flecs::entity entity) -> T* {
        return entity.get_mut<T>();
    }

    template<typename T>
    auto has_component(flecs::entity entity) -> bool {
        return entity.has<T>();
    }

    // 查询
    template<typename... Components>
    auto query() {
        return m_world.query<Components...>();
    }

    template<typename... Components, typename Func>
    void each(Func&& func) {
        m_world.each<Components...>(std::forward<Func>(func));
    }

    // 系统注册
    template<typename... Components>
    auto system(const String& name, auto&& callback) {
        return m_world.system<Components...>(name.c_str())
            .iter(callback);
    }

    // 进度更新
    void progress(f32 deltaTime);

    // 直接访问 Flecs world
    auto flecs_world() -> flecs::world& { return m_world; }

private:
    flecs::world m_world;
};

} // namespace firefly::ecs
```

#### Components (components.cppm)
```cpp
export module firefly.ecs.components;

import std;
import flecs;
import firefly.core.types;
import glm;

export namespace firefly::ecs {

// 变换组件 (使用 Flecs 内置的 Transform 模块或自定义)
struct Transform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    auto world_matrix() const -> glm::mat4;
    void set_world_matrix(const glm::mat4& matrix);
};

struct TransformComponent : Transform {
    // Flecs 可以直接使用这个作为组件
};

// 网格渲染组件
struct MeshComponent {
    u64 meshHandle;      // 资源句柄
    u64 materialHandle;  // 资源句柄
    bool castShadows = true;
    bool receiveShadows = true;
};

// 相机组件
struct CameraComponent {
    f32 fov = 60.0f;
    f32 nearPlane = 0.1f;
    f32 farPlane = 1000.0f;
    f32 aspectRatio = 16.0f / 9.0f;

    auto projection_matrix() const -> glm::mat4;
    auto view_matrix(const Transform& transform) const -> glm::mat4;
};

// 光源组件
struct LightComponent {
    enum class Type : u8 {
        Directional,
        Point,
        Spot
    };

    Type type = Type::Directional;
    glm::vec3 color{1.0f};
    f32 intensity = 1.0f;
    f32 range = 10.0f;
    f32 innerAngle = 30.0f;
    f32 outerAngle = 45.0f;
};

// 名称组件
struct NameComponent {
    String name;
};

// 原生脚本组件
struct NativeScriptComponent {
    virtual ~NativeScriptComponent() = default;
    virtual void on_create() {}
    virtual void on_update(f32 deltaTime) {}
    virtual void on_destroy() {}
};

} // namespace firefly::ecs
```

#### Systems (systems.cppm)
```cpp
export module firefly.ecs.systems;

import std;
import flecs;
import firefly.ecs.world;
import firefly.ecs.components;

export namespace firefly::ecs {

// 注册内置系统到 World
void register_builtin_systems(World& world);

// 变换系统
void register_transform_system(flecs::world& ecs);

// 渲染收集系统
void register_render_system(flecs::world& ecs);

// 脚本系统
void register_script_system(flecs::world& ecs);

} // namespace firefly::ecs
```

### 4. Renderer Module

#### Device (device.cppm)
```cpp
export module firefly.renderer.device;

import std;
import firefly.core.types;
import firefly.renderer.buffer;
import firefly.renderer.texture;
import firefly.renderer.shader;

// WGPU 前向声明
struct WGPUInstanceImpl;
struct WGPUAdapterImpl;
struct WGPUDeviceImpl;
struct WGPUSurfaceImpl;
struct WGPUSwapChainImpl;

export namespace firefly {

using WGPUInstance = WGPUInstanceImpl*;
using WGPUAdapter = WGPUAdapterImpl*;
using WGPUDevice = WGPUDeviceImpl*;
using WGPUSurface = WGPUSurfaceImpl*;
using WGPUSwapChain = WGPUSwapChainImpl*;

struct DeviceConfig {
    bool enableValidation = true;
    bool enableVSync = true;
    u32 maxFramesInFlight = 2;
};

class RenderDevice {
public:
    RenderDevice();
    ~RenderDevice();

    // 初始化
    auto initialize(void* nativeWindow, const DeviceConfig& config = {}) -> Result<void>;
    void shutdown();

    // 资源创建
    auto create_buffer(const BufferDesc& desc) -> Ptr<Buffer>;
    auto create_texture(const TextureDesc& desc) -> Ptr<Texture>;
    auto create_shader(const ShaderDesc& desc) -> Ptr<Shader>;
    auto create_pipeline(const PipelineDesc& desc) -> Ptr<Pipeline>;

    // 帧控制
    auto begin_frame() -> class CommandBuffer*;
    void end_frame();
    void present();

    // WGPU 访问
    auto wgpu_device() const -> WGPUDevice { return m_device; }
    auto wgpu_surface() const -> WGPUSurface { return m_surface; }

private:
    WGPUInstance m_instance = nullptr;
    WGPUAdapter m_adapter = nullptr;
    WGPUDevice m_device = nullptr;
    WGPUSurface m_surface = nullptr;
    WGPUSwapChain m_swapChain = nullptr;
    Ptr<CommandBuffer> m_commandBuffer;
};

} // namespace firefly
```

#### Renderer (renderer.cppm)
```cpp
export module firefly.renderer.renderer;

import std;
import firefly.core.types;
import firefly.renderer.device;
import firefly.renderer.pipeline;
import firefly.renderer.command;
import firefly.renderer.material;
import firefly.renderer.mesh;
import firefly.ecs.components;

export namespace firefly {

struct RendererConfig {
    bool enableValidation = true;
    bool enableVSync = true;
    u32 maxFramesInFlight = 2;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    auto initialize(void* nativeWindow, const RendererConfig& config = {}) -> Result<void>;
    void shutdown();

    // 渲染帧
    void begin_frame();
    void end_frame();
    void present();

    // 渲染命令
    void draw_mesh(const Mesh& mesh, const Material& material, const glm::mat4& transform);
    void draw_instanced(const Mesh& mesh, u32 instanceCount);

    // 相机设置
    void set_camera(const CameraComponent& camera, const Transform& transform);

    // 灯光
    void add_light(const LightComponent& light, const Transform& transform);
    void clear_lights();

    // 后处理
    void add_post_process_effect(SharedPtr<class PostProcessEffect> effect);

    // 访问器
    auto device() -> RenderDevice& { return *m_device; }
    auto command_buffer() -> CommandBuffer& { return *m_commandBuffer; }

private:
    Ptr<RenderDevice> m_device;
    Ptr<RenderPipeline> m_pipeline;
    Ptr<CommandBuffer> m_commandBuffer;
    std::vector<SharedPtr<PostProcessEffect>> m_postProcessEffects;
};

} // namespace firefly
```

### 5. Resource Module

#### Manager (manager.cppm)
```cpp
export module firefly.resource.manager;

import std;
import firefly.core.types;

export namespace firefly {

// 资源句柄
template<typename T>
class Handle {
public:
    Handle() = default;
    Handle(u64 id, class ResourceManager* manager)
        : m_id(id), m_manager(manager) {}

    auto is_valid() const -> bool { return m_id != 0; }
    auto get() -> T*;
    auto get() const -> const T*;

    auto id() const -> u64 { return m_id; }

    auto operator==(const Handle& other) const -> bool {
        return m_id == other.m_id;
    }

private:
    u64 m_id = 0;
    ResourceManager* m_manager = nullptr;
};

// 资源基类
class Resource {
public:
    virtual ~Resource() = default;
    virtual auto type_name() const -> StringView = 0;
};

// 资源管理器
class ResourceManager {
public:
    ResourceManager();
    ~ResourceManager();

    // 同步加载
    template<typename T>
    auto load(const String& path) -> Handle<T>;

    // 异步加载 (使用 C++20 协程)
    template<typename T>
    auto load_async(const String& path) -> std::future<Handle<T>>;

    // 资源访问
    template<typename T>
    auto get(Handle<T> handle) -> T*;

    // 资源管理
    void unload(u64 handleId);
    void unload_unused();
    void reload(u64 handleId);

    // 注册加载器
    template<typename T, typename Loader>
    void register_loader();

private:
    std::unordered_map<u64, SharedPtr<Resource>> m_cache;
    std::unordered_map<std::type_index, Ptr<class LoaderBase>> m_loaders;
    std::mutex m_mutex;
    u64 m_nextId = 1;
};

} // namespace firefly
```

### 6. Scene Module

#### SceneManager (scene_manager.cppm)
```cpp
export module firefly.scene.scene_manager;

import std;
import firefly.core.types;
import firefly.ecs.world;
import firefly.scene.scene_node;

export namespace firefly {

class Scene {
public:
    explicit Scene(const String& name = "Scene");
    ~Scene();

    // 节点管理
    auto create_node(const String& name = "Node") -> SceneNode*;
    void destroy_node(SceneNode* node);
    auto root() -> SceneNode* { return m_root.get(); }

    // 查找
    auto find_node(const String& name) -> SceneNode*;

    // 序列化
    auto save(const String& path) -> bool;
    auto load(const String& path) -> bool;

    // 更新
    void update(f32 deltaTime);

    // ECS 访问
    auto world() -> ecs::World& { return m_world; }

private:
    String m_name;
    Ptr<SceneNode> m_root;
    ecs::World m_world;
    std::unordered_map<String, SceneNode*> m_nodeMap;
};

class SceneManager {
public:
    SceneManager() = default;

    auto create_scene(const String& name) -> Scene*;
    auto current_scene() -> Scene* { return m_currentScene; }
    void set_current_scene(Scene* scene);

private:
    std::vector<Ptr<Scene>> m_scenes;
    Scene* m_currentScene = nullptr;
};

} // namespace firefly
```

## Data Models

### Flecs ECS 数据模型
```
Flecs World:
┌─────────────────────────────────────────────────────────────┐
│ Archetypes (按组件组合分组存储)                               │
│   ┌─────────────────────────────────────────────────────┐  │
│   │ [Transform, MeshComponent]                          │  │
│   │ Entity1: Transform{...}, MeshComponent{...}         │  │
│   │ Entity2: Transform{...}, MeshComponent{...}         │  │
│   │ ...                                                 │  │
│   └─────────────────────────────────────────────────────┘  │
│   ┌─────────────────────────────────────────────────────┐  │
│   │ [Transform, CameraComponent]                        │  │
│   │ Entity3: Transform{...}, CameraComponent{...}       │  │
│   └─────────────────────────────────────────────────────┘  │
│                                                             │
│ Systems (按阶段执行)                                         │
│   OnLoad → PreUpdate → OnUpdate → PostUpdate → OnStore     │
└─────────────────────────────────────────────────────────────┘
```

### Render Graph
```
RenderGraph:
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ Shadow Pass  │────▶│ Geometry Pass│────▶│ Post Process │
│ (Depth)      │     │ (G-Buffer)   │     │ (HDR/Bloom)  │
└──────────────┘     └──────────────┘     └──────────────┘
                            │
                            ▼
                     ┌──────────────┐
                     │ UI Pass      │
                     │ (ImGui)      │
                     └──────────────┘
```

## Error Handling

### 错误处理策略

1. **Contracts (C++20)** - 前置/后置条件检查
```cpp
// 使用 assert 或自定义契约宏
#define FIREFLY_EXPECTS(cond) assert(cond)
#define FIREFLY_ENSURES(cond) assert(cond)
```

2. **Result Type** - 可预期的错误
```cpp
auto device = renderer.initialize(window);
if (device.is_error()) {
    logger.error("Failed to initialize renderer: {}", device.error());
    return -1;
}
```

3. **std::expected (C++23) 兼容类型** - 未来可迁移

4. **WGPU 错误回调**
```cpp
void on_wgpu_error(WGPUErrorType type, const char* message, void* userdata);
```

## Testing Strategy

### 单元测试
- 使用 Google Test 框架
- 测试覆盖核心模块

```cpp
import std;
import <gtest/gtest.h>;
import firefly.ecs.world;
import firefly.ecs.components;

TEST(ECS_Test, CreateEntity) {
    firefly::ecs::World world;
    auto entity = world.create_entity("TestEntity");
    EXPECT_TRUE(entity.is_alive());
}

TEST(ECS_Test, AddComponent) {
    firefly::ecs::World world;
    auto entity = world.create_entity();

    auto& transform = world.add_component<firefly::ecs::Transform>(entity);
    transform.position = {1.0f, 2.0f, 3.0f};

    EXPECT_TRUE(world.has_component<firefly::ecs::Transform>(entity));
}
```

### 测试覆盖率目标
- Core 模块: 80%+
- ECS 封装: 90%+
- Renderer 模块: 60%+

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

### 可选依赖
| 库 | 版本 | 用途 |
|---|---|---|
| ImGui | 1.89+ | 调试 UI |
| Tracy | 0.10+ | 性能分析 |
| stb_image | 2.x | 图像加载 |
| tinygltf | 2.x | GLTF 加载 |

## CMake 配置示例

### CMakePresets.json
```json
{
    "version": 3,
    "configurePresets": [
        {
            "name": "default",
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/build/${presetName}",
            "cacheVariables": {
                "CMAKE_CXX_COMPILER": "cl",
                "CMAKE_CXX_STANDARD": "20",
                "CMAKE_CXX_STANDARD_REQUIRED": "ON",
                "CMAKE_CXX_SCAN_FOR_MODULES": "ON",
                "CMAKE_BUILD_TYPE": "Debug"
            }
        },
        {
            "name": "release",
            "inherits": "default",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release"
            }
        },
        {
            "name": "msvc-debug",
            "inherits": "default",
            "generator": "Ninja Multi-Config",
            "cacheVariables": {
                "CMAKE_CONFIGURATION_TYPES": "Debug;Release"
            }
        }
    ],
    "buildPresets": [
        {
            "name": "debug",
            "configurePreset": "default"
        },
        {
            "name": "release",
            "configurePreset": "release"
        }
    ]
}
```

### CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.28)
project(firefly LANGUAGES CXX)

# C++20 模块支持
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# 导出编译命令 (用于 IDE 集成)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# vcpkg 集成
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# 查找依赖
find_package(glfw3 CONFIG REQUIRED)
find_package(flecs CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# wgpu-native (手动配置)
set(WGPU_NATIVE_DIR "${CMAKE_SOURCE_DIR}/third_party/wgpu-native")
include_directories(${WGPU_NATIVE_DIR}/include)

# 定义模块源文件
set(FIREFLY_MODULE_SOURCES
    src/firefly/firefly.cppm
    src/core/core.cppm
    src/core/types.cppm
    src/core/application.cppm
    src/core/application.cpp
    src/core/engine.cppm
    src/core/engine.cpp
    src/core/event.cppm
    src/core/event.cpp
    src/core/logger.cppm
    src/core/logger.cpp
    src/core/time.cppm
    src/core/time.cpp
    src/platform/platform.cppm
    src/platform/window.cppm
    src/platform/window.cpp
    src/platform/input.cppm
    src/platform/input.cpp
    src/ecs/ecs.cppm
    src/ecs/world.cppm
    src/ecs/world.cpp
    src/ecs/components.cppm
    src/ecs/systems.cppm
    src/renderer/renderer.cppm
    src/renderer/device.cppm
    src/renderer/device.cpp
    src/renderer/pipeline.cppm
    src/renderer/pipeline.cpp
    src/renderer/command.cppm
    src/renderer/command.cpp
    src/renderer/buffer.cppm
    src/renderer/buffer.cpp
    src/renderer/texture.cppm
    src/renderer/texture.cpp
    src/renderer/shader.cppm
    src/renderer/shader.cpp
    src/renderer/material.cppm
    src/renderer/material.cpp
    src/renderer/mesh.cppm
    src/renderer/mesh.cpp
    src/scene/scene.cppm
    src/scene/scene_manager.cppm
    src/scene/scene_manager.cpp
    src/resource/resource.cppm
    src/resource/manager.cppm
    src/resource/manager.cpp
)

# 定义模块库
add_library(firefly STATIC)
target_sources(firefly PUBLIC FILE_SET CXX_MODULES FILES ${FIREFLY_MODULE_SOURCES})

target_include_directories(firefly PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${WGPU_NATIVE_DIR}/include
)

target_link_libraries(firefly PRIVATE
    glfw::glfw
    flecs::flecs_static
    glm::glm
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ${WGPU_NATIVE_DIR}/lib/wgpu_native.lib
)

# 编译选项
if(MSVC)
    target_compile_options(firefly PRIVATE /W4 /utf-8)
else()
    target_compile_options(firefly PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 示例程序
add_subdirectory(examples)

# 测试
option(FIREFLY_BUILD_TESTS "Build tests" ON)
if(FIREFLY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

### 构建命令

```bash
# 配置项目 (使用 Ninja)
cmake --preset=default

# 构建
cmake --build --preset=debug

# 或直接使用 Ninja
cd build/default
ninja

# Release 构建
cmake --preset=release
cmake --build --preset=release
```
