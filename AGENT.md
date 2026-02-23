# Firefly 游戏引擎 - 开发规范

## 项目概述
基于 C++20 模块的游戏引擎，使用 CMake + Ninja 构建，目标平台 Windows (MSVC)。

## 开发规则

### 测试要求
- **每个任务完成时必须有对应的测试**
- 测试框架：doctest
- 不要把测试推迟到最后阶段，每个模块/功能实现后立即编写测试
- 测试应覆盖核心逻辑、边界条件和错误处理

### 构建与测试流程
- **每次构建或测试前必须先清理对应构建目录，避免编译缓存导致异常**
- 推荐流程（Debug）：`build.bat clean debug` → `build.bat test debug`
- 推荐流程（Release）：`build.bat clean release` → `build.bat test release`

### 文档
- 需求文档：docs/requirements.md
- 设计文档：docs/design.md
- 任务列表：docs/tasks.md
