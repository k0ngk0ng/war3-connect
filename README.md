# War3 Connect

将魔兽争霸3（Warcraft III）的局域网广播数据包重定向到指定IP地址，实现跨互联网的局域网联机。

## 原理

魔兽争霸3在创建/搜索局域网游戏时，会向 `255.255.255.255:6112` 发送 UDP 广播包。本工具通过 DLL 注入 + API Hook（IAT 钩子）的方式，拦截 `sendto()` 调用，将广播包额外发送到你指定的目标 IP 地址，从而让不在同一局域网的玩家也能互相发现游戏。

## 组件

- **war3-connect.exe** — 启动器/注入器，负责写入配置、启动 War3 并注入 DLL
- **war3hook.dll** — 钩子 DLL，Hook `sendto()` 函数，重定向广播包

## 编译

### 环境要求

- CMake 3.15+
- MSVC (Visual Studio 2019/2022) 或 MinGW-w64

### 使用 MSVC 编译

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A Win32
cmake --build . --config Release
```

> ⚠️ 魔兽争霸3 (1.2x) 是 32 位程序，请务必编译为 **x86 (Win32)** 架构。

### 使用 MinGW 编译

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_C_FLAGS="-m32"
cmake --build .
```

## 使用方法

1. 将 `war3-connect.exe` 和 `war3hook.dll` 放到魔兽争霸3的安装目录（与 `war3.exe` 同目录）
2. 运行命令：

```bash
# 基本用法：指定一个目标IP
war3-connect.exe -ip 192.168.1.100

# 指定多个目标IP（向多个远程玩家发送）
war3-connect.exe -ip 10.0.0.5 -ip 10.0.0.6

# 指定 war3.exe 路径
war3-connect.exe -ip 192.168.1.100 -war3 "D:\Games\Warcraft III\war3.exe"

# 传递额外参数给 war3 并等待退出
war3-connect.exe -ip 192.168.1.100 -args "-window" -wait
```

## 联机步骤

假设玩家 A (IP: 1.2.3.4) 和玩家 B (IP: 5.6.7.8) 要联机：

1. **双方** 都需要确保 UDP 端口 **6112** 已开放（路由器端口转发/防火墙放行）
2. 玩家 A 运行：`war3-connect.exe -ip 5.6.7.8`
3. 玩家 B 运行：`war3-connect.exe -ip 1.2.3.4`
4. 任意一方创建局域网游戏，另一方即可在局域网列表中看到

## 注意事项

- 本工具仅修改网络广播目标地址，不修改游戏本身
- 需要以管理员权限运行
- 仅支持 Warcraft III 经典版（1.2x 版本），不支持重制版
- 广播包仍会发送到本地局域网（255.255.255.255），不影响局域网内的正常游戏发现

## 许可证

MIT License
