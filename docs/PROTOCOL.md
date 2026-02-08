# War3 对战平台 - 网络协议文档

## 传输层

- **协议**: TCP
- **默认端口**: 12000
- **帧格式**: 长度前缀帧

```
┌──────────────────┬──────────────────────────┐
│  4 字节 (大端)    │  JSON Payload            │
│  payload 长度     │  UTF-8 编码              │
└──────────────────┴──────────────────────────┘
```

## 消息格式

所有消息均为 JSON 对象，必须包含 `"type"` 字段。

---

## 客户端 → 服务端

### login - 登录
```json
{"type": "login", "username": "玩家名"}
```

### room_list - 获取房间列表
```json
{"type": "room_list"}
```

### room_create - 创建房间
```json
{"type": "room_create", "name": "房间名", "max_players": 10}
```

### room_join - 加入房间
```json
{"type": "room_join", "room_id": 1}
```

### room_leave - 离开房间
```json
{"type": "room_leave"}
```

### chat - 发送聊天消息
```json
{"type": "chat", "message": "大家好"}
```

### heartbeat - 心跳
```json
{"type": "heartbeat"}
```

---

## 服务端 → 客户端

### login_ok - 登录成功
```json
{"type": "login_ok", "user_id": 1, "username": "玩家名"}
```

### login_fail - 登录失败
```json
{"type": "login_fail", "reason": "用户名已被占用"}
```

### room_list_result - 房间列表
```json
{
  "type": "room_list_result",
  "rooms": [
    {"id": 1, "name": "来打DOTA", "player_count": 3, "max_players": 10},
    {"id": 2, "name": "RPG房", "player_count": 2, "max_players": 8}
  ]
}
```

### room_created - 房间创建成功
```json
{"type": "room_created", "room_id": 1, "name": "来打DOTA"}
```

### room_joined - 成功加入房间
```json
{"type": "room_joined", "room_id": 1, "name": "来打DOTA"}
```

### room_peers - 房间成员列表 (加入/离开时广播)
```json
{
  "type": "room_peers",
  "peers": [
    {"username": "玩家1", "ip": "1.2.3.4"},
    {"username": "玩家2", "ip": "5.6.7.8"}
  ]
}
```

> 客户端收到此消息后，提取所有 **其他玩家** 的 IP 写入 `war3hook.cfg`，
> 供 Hook DLL 将 War3 的局域网广播重定向到这些 IP。

### room_left - 已离开房间
```json
{"type": "room_left"}
```

### chat_msg - 聊天消息
```json
{"type": "chat_msg", "from": "玩家1", "message": "大家好"}
```

### player_joined - 有玩家加入房间
```json
{"type": "player_joined", "username": "玩家2", "ip": "5.6.7.8"}
```

### player_left - 有玩家离开房间
```json
{"type": "player_left", "username": "玩家2"}
```

### error - 错误
```json
{"type": "error", "message": "房间已满"}
```

### heartbeat_ack - 心跳回复
```json
{"type": "heartbeat_ack"}
```

---

## 典型交互流程

```
客户端                          服务端
  │                               │
  │──── login ───────────────────>│
  │<─── login_ok ─────────────────│
  │                               │
  │──── room_list ───────────────>│
  │<─── room_list_result ─────────│
  │                               │
  │──── room_create ─────────────>│
  │<─── room_created ─────────────│
  │<─── room_peers ───────────────│  (只有自己)
  │                               │
  │         [玩家B加入房间]         │
  │<─── player_joined ────────────│
  │<─── room_peers ───────────────│  (自己+玩家B)
  │                               │
  │  [客户端写入 war3hook.cfg]      │
  │  [客户端启动 War3 + 注入 DLL]   │
  │                               │
  │──── chat ────────────────────>│
  │<─── chat_msg ─────────────────│  (广播给房间所有人)
  │                               │
  │──── heartbeat ───────────────>│
  │<─── heartbeat_ack ────────────│
  │                               │
  │──── room_leave ──────────────>│
  │<─── room_left ────────────────│
```

## 端口说明

| 端口 | 协议 | 用途 |
|------|------|------|
| 12000 | TCP | 对战平台 客户端↔服务端 通信 |
| 6112 | UDP | War3 局域网游戏发现 (广播重定向) |
| 6112 | TCP | War3 游戏数据传输 (War3自身管理) |
