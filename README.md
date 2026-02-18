# HeroKill - 英雄杀游戏客户端

基于 [FreeKill (新月杀)](https://github.com/Qsgs-Fans/FreeKill) 框架二次开发的卡牌对战游戏客户端，使用 C++ / Lua / QML 构建，支持跨平台运行。

## 项目简介

HeroKill 是一款类三国杀的卡牌对战游戏客户端，负责 UI 渲染、网络通信和游戏逻辑展示。客户端与基于 Spring Boot 微服务架构的 Java 后端服务器配合工作，通过 TCP/CBOR 协议进行实时游戏交互，通过 HTTP API 进行数据查询。

## 技术栈

| 层面 | 技术 |
|------|------|
| 编程语言 | C++20 + Lua 5.4 + QML |
| 构建系统 | CMake 3.22+ |
| UI 框架 | Qt 6.8+ (QtQuick / QML) |
| 网络通信 | Qt Network (TCP Socket)，CBOR 序列化，AES 加密 |
| 脚本引擎 | Lua 5.4 (SWIG 绑定) |
| 本地存储 | SQLite3 |
| 扩展包管理 | libgit2 |
| 加密 | OpenSSL |

## 支持平台

- Windows
- Linux
- macOS
- Android

## 项目结构

```
hero-kill-game-client/
├── src/                    # C++ 源代码
│   ├── main.cpp            #   程序入口
│   ├── herokill.h/cpp      #   主初始化逻辑
│   ├── core/               #   核心模块 (玩家、工具、扩展包管理)
│   ├── client/             #   客户端逻辑 (网络客户端、录像回放)
│   ├── network/            #   网络通信 (TCP Socket、路由、HTTP 代理)
│   ├── swig/               #   SWIG C++/Lua 绑定
│   └── ui/                 #   QML-C++ 后端桥接
├── packages/               # 游戏扩展包 (Lua 脚本)
│   ├── herokill-core/      #   核心扩展包 (QML UI + Lua 核心逻辑)
│   └── gamemode/           #   游戏模式扩展包 (16+ 种游戏模式)
├── include/                # 第三方头文件 (Git 子模块)
├── lib/                    # 预编译第三方库 (Android/Windows)
├── android/                # Android 平台文件
├── image/                  # 图片资源
├── audio/                  # 音频资源
├── fonts/                  # 字体文件
└── docs/                   # 文档
```

## 环境要求

- CMake 3.22+
- Qt 6.8+
- Lua 5.4
- OpenSSL
- libgit2
- SQLite3
- C++20 兼容编译器

## 构建与运行

```bash
# 1. 克隆项目（含子模块）
git clone --recursive <repo-url>

# 2. 构建
mkdir build && cd build
cmake ..
make -j$(nproc)

# 3. 运行
./HeroKill
```

或使用项目提供的脚本：

```bash
./rebuild-and-run.sh
```

## 与服务端的通信

客户端通过两种方式与后端微服务交互：

- **TCP 直连** — 客户端通过 TCP Socket 连接游戏服务 (`:9527`)，使用 CBOR 序列化进行实时游戏交互（登录、出牌、技能、聊天等）
- **HTTP API** — 客户端通过 HTTP 连接网关服务，进行用户数据查询、好友管理、战绩等操作

## 扩展包开发

游戏逻辑通过 Lua 脚本实现，支持自定义武将、技能、卡牌和游戏模式。详见 [`packages/CLAUDE.md`](packages/CLAUDE.md)。

## 许可证

本项目基于 [GPL-3.0](LICENSE) 协议开源。

## 鸣谢

本项目基于 **[FreeKill (新月杀)](https://github.com/Qsgs-Fans/FreeKill)** 开源桌游引擎二次开发，在此向 FreeKill 项目及其所有贡献者致以诚挚的感谢：

- [Notify-ctrl](https://github.com/Notify-ctrl) — FreeKill 项目作者
- [Qsgs-Fans](https://github.com/Qsgs-Fans) — FreeKill 社区组织
- 以及所有 FreeKill 项目的 [贡献者们](https://github.com/Qsgs-Fans/FreeKill/graphs/contributors)

FreeKill 提供了优秀的 Qt + Lua 桌游引擎框架，使得本项目能够专注于游戏内容和服务端架构的开发。感谢开源社区的无私奉献！