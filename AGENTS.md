# Repository Guidelines

## Project Structure & Module Organization

Firefly 是基于 C++20 模块的 Windows/MSVC 游戏引擎。核心源码位于 `src/`，按职责拆分为 `core`、`platform`、`ecs`、`renderer`、`resource`、`scene` 等模块；模块接口通常使用 `.cppm`，实现使用 `.cpp`。测试位于 `tests/`，目录结构与源码模块对应。示例程序位于 `examples/`，包括 `hello_triangle`、`forward_sphere`、`model_viewer`、`sponza_viewer`。资源位于 `assets/`，其中大型模型文件通过 Git LFS 管理。第三方依赖固定在 `third_party/`，工具链辅助文件在 `tools/`。

## Build, Test, and Development Commands

优先使用根目录的 `build.bat`，它会初始化 MSVC x64 环境并调用 CMake/Ninja。

- `build.bat clean debug`：清理 Debug 构建目录。
- `build.bat build debug`：配置并构建 Debug。
- `build.bat test debug`：构建并运行全部 CTest 测试。
- `build.bat clean release`、`build.bat test release`：Release 对应流程。
- `cmake --preset=default`：手动配置 Debug 预设。
- `cmake --build --preset=debug`：手动构建 Debug 预设。

每次构建或测试前先清理对应构建目录，避免模块缓存影响结果。

## Coding Style & Naming Conventions

代码使用 C++20，MSVC 编译选项包含 `/W4` 与 `/utf-8`。保持 4 空格缩进，避免无关格式化。类型名使用 `PascalCase`，函数、变量和文件名使用 `snake_case`，常量可使用 `kPascalCase` 或项目内已有模式。模块接口放在 `.cppm`，实现细节放在同名 `.cpp`。新增或修改的代码注释使用中文；代码标识符、日志、错误信息和业务字符串使用英文。

## Testing Guidelines

测试框架为 doctest，并通过 CTest 注册。新增功能应补充对应模块测试，例如 ECS 变更放入 `tests/ecs/`，渲染非 GPU 逻辑放入 `tests/renderer/`。测试文件命名采用 `test_<module>.cpp` 或现有目标名模式。提交前运行 `build.bat clean debug` 和 `build.bat test debug`；涉及优化或发布路径时再运行 Release 测试。

## Commit & Pull Request Guidelines

提交历史采用 Conventional Commits 风格，例如 `feat: ...`、`fix: ...`、`refactor: ...`、`build: ...`、`docs: ...`、`chore: ...`。提交信息应简短说明行为变化。Pull Request 需包含变更摘要、测试结果、关联任务或 issue；涉及示例渲染输出时附截图或说明运行环境。不要提交生成的 `build/` 内容，大型资源应确认已由 Git LFS 跟踪。
