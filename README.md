# War3 对战平台

一个为魔兽争霸3（Warcraft III）玩家打造的对战平台，让不同网络的玩家可以像在局域网一样联机。

## 功能特性

- 🏠 **房间系统** — 创建/加入房间，房间内的玩家自动组成虚拟局域网
- 💬 **实时聊天** — 房间内文字聊天
- 🎮 **一键启动** — 点击"启动游戏"自动配置并启动 War3
- 🔄 **热重载** — 房间成员变化时自动更新配置，无需重启游戏
- 🖥️ **图形界面** — 原生 Win32 GUI，无需命令行操作
- 🌐 **跨平台服务端** — 服务端可运行在 Windows / Linux / macOS

## 原理

魔兽争霸3在创建/搜索局域网游戏时，会向 `255.255.255.255:6112` 发送 UDP 广播包。本工具通过 DLL 注入 + Inline Hook 的方式，拦截 `ws2_32.dll!sendto()` 调用，将广播包额外发送到房间内其他玩家的 IP 地址，从而让不在同一局域网的玩家也能互相发现游戏。

## 组件

| 文件 | 说明 |
|------|------|
| `war3-platform.exe` | 客户端 GUI — 登录、房间管理、启动游戏 |
| `war3-lobby-server.exe` | 服务端 — 管理用户和房间 |
| `war3hook.dll` | Hook DLL — 注入 War3 进程，重定向广播包 |

## 架构

```
┌─────────────────┐         TCP/12000          ┌──────────────────┐
│  war3-platform   │◄────── JSON over TCP ─────►│ war3-lobby-server│
│  (Win32 GUI)     │                            │  (跨平台)         │
└────────┬────────┘                            └──────────────────┘
         │
         │ CreateProcess + DLL Inject
         ▼
┌─────────────────┐
│   war3.exe       │
│  + war3hook.dll  │──── UDP 6112 ────► 其他玩家
└─────────────────┘
```

## 编译

### 环境要求

- CMake 3.15+
- **客户端 + Hook DLL**: MSVC (Visual Studio 2019/2022)，必须编译为 **Win32 (x86)**
- **服务端**: 任意 C11 编译器（GCC / Clang / MSVC）

### Windows 全量编译（客户端 + 服务端 + Hook DLL）

```bash
cmake -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
```

> ⚠️ 魔兽争霸3 (1.2x) 是 32 位程序，客户端和 Hook DLL 请务必编译为 **x86 (Win32)** 架构。

### 仅编译服务端（Linux / macOS）

```bash
cmake -B build
cmake --build build --target war3-lobby-server
```

编译产物：
- `build/Release/war3-platform.exe` — 客户端
- `build/Release/war3-lobby-server.exe` — 服务端
- `build/Release/war3hook.dll` — Hook DLL

## 使用方法

### 1. 部署服务端

在一台有公网 IP 的服务器上运行：

```bash
# 默认监听端口 12000
./war3-lobby-server

# 自定义端口（例如 8888）
./war3-lobby-server 8888
```

### 2. 玩家使用客户端

1. 将 `war3-platform.exe` 和 `war3hook.dll` 放到魔兽争霸3的安装目录（与 `war3.exe` 同目录）
2. 运行 `war3-platform.exe`
3. 输入服务器地址（如 `1.2.3.4:12000`）和用户名，点击"登录"
4. 在大厅中创建房间或加入已有房间
5. 房间内所有玩家点击"启动游戏"
6. 在 War3 中创建/加入局域网游戏即可联机

### 典型联机流程

```
玩家A                     服务端                     玩家B
  │                         │                         │
  │──── 登录 ──────────────►│◄──────── 登录 ──────────│
  │                         │                         │
  │──── 创建房间 ──────────►│                         │
  │◄─── 房间创建成功 ───────│                         │
  │                         │                         │
  │                         │◄──── 加入房间 ──────────│
  │◄─── room_peers ─────────│──── room_peers ────────►│
  │  (写入 war3hook.cfg)    │   (写入 war3hook.cfg)   │
  │                         │                         │
  │  [点击启动游戏]          │      [点击启动游戏]      │
  │  War3 启动 + DLL注入     │      War3 启动 + DLL注入 │
  │                         │                         │
  │◄════ UDP 6112 广播重定向 ════════════════════════►│
  │         玩家可在局域网列表互相看到                    │
```

## 端口说明

| 端口 | 协议 | 用途 |
|------|------|------|
| 12000 | TCP | 对战平台 客户端↔服务端 通信 |
| 6112 | UDP | War3 局域网游戏发现（广播重定向） |
| 6112 | TCP | War3 游戏数据传输（War3 自身管理） |

> 💡 玩家需要确保 UDP/TCP 6112 端口已开放（路由器端口转发/防火墙放行）

## 目录结构

```
war3-connect/
├── common/              # 共享协议层
│   ├── protocol.h/c     # 长度前缀帧编解码
│   └── message.h        # 消息类型常量
├── server/              # 服务端（跨平台）
│   ├── server.h/c       # select() 事件循环
│   ├── handler.h/c      # 消息处理器
│   ├── user.h/c         # 用户管理
│   ├── room.h/c         # 房间管理
│   └── main.c           # 入口
├── client/              # 客户端 GUI（Windows）
│   ├── gui.h/c          # 主窗口框架
│   ├── gui_login.c      # 登录页
│   ├── gui_lobby.c      # 大厅页（房间列表）
│   ├── gui_room.c       # 房间页（聊天+玩家列表）
│   ├── net_client.h/c   # TCP 网络客户端
│   ├── game_launcher.h/c # 启动 War3 + 注入
│   ├── injector.h/c     # DLL 注入
│   ├── resource.h       # 控件 ID
│   └── main.c           # WinMain 入口
├── hook_dll/            # Hook DLL（Windows x86）
│   ├── hook.h/c         # sendto() inline hook
│   ├── config.h/c       # 配置热重载
│   └── dllmain.c        # DLL 入口
├── third_party/cJSON/   # JSON 解析库
├── docs/PROTOCOL.md     # 网络协议文档
└── CMakeLists.txt       # 构建脚本
```

## 注意事项

- 本工具仅修改网络广播目标地址，不修改游戏本身
- 客户端需要以管理员权限运行（DLL 注入需要）
- 仅支持 Warcraft III 经典版（1.2x 版本），不支持重制版
- 广播包仍会发送到本地局域网（255.255.255.255），不影响局域网内的正常游戏发现
- Hook DLL 每 3 秒检查一次配置文件变化，支持热重载

## 许可证

MIT License
