# 客户端服务器通讯文档

> 最后更新: 2026-01-28

本文档描述 Hero Kill 游戏客户端与服务端之间的通讯方式、协议格式和使用方法。

---

## 目录

- [通讯方式概览](#通讯方式概览)
- [1. TCP 直连通信](#1-tcp-直连通信)
- [2. Lua RPC 通信](#2-lua-rpc-通信)
- [3. AdminService RPC](#3-adminservice-rpc)
- [协议格式](#协议格式)
- [消息流程图](#消息流程图)
- [最佳实践](#最佳实践)

---

## 通讯方式概览

| 方式 | 通道 | 阻塞 | 适用层 | 主要场景 |
|------|------|------|--------|----------|
| **TCP 直连** | TCP Socket (CBOR) | 否 | QML / Lua Client | 实时游戏交互 |
| **Lua RPC** | stdio (JSON-RPC) | 是 | Lua Server | 底层数据操作 |
| **AdminService** | stdio (封装) | 可选 | Lua Server | 业务逻辑调用 |

---

## 1. TCP 直连通信

### 概述

客户端通过 TCP Socket 直接与游戏服务端通信，使用 CBOR 序列化，支持 AES 加密和压缩。

### 特点

- **低延迟**：直接 TCP 连接，适合实时交互
- **非阻塞**：发送后不等待响应（Notify 模式）
- **双向通信**：支持 Request/Reply/Notify 三种模式

### 使用场景

- 游戏内实时操作（出牌、技能、聊天）
- 心跳保活
- 房间操作（创建、加入、退出）
- 大厅交互

### QML 层使用

```qml
import Fk

// 发送通知到服务端（单向，无需响应）
Cpp.notifyServer("CommandName", {
    key1: "value1",
    key2: 123
})

// 回复服务端请求
Cpp.replyToServer(responseData)
```

### Lua Client 层使用

```lua
-- 发送通知到服务端
self.client:notifyServer("CommandName", jsonData)

-- 回复服务端请求
self.client:replyToServer("CommandName", jsonData)
```

### 接收服务端消息

**QML 层**：

```qml
Component.onCompleted: {
    // 注册回调监听服务端消息
    addCallback("CommandName", function(sender, data) {
        console.log("收到消息:", JSON.stringify(data))
    })

    // 使用 Command 枚举
    addCallback(Command.EnterLobby, function(sender, data) {
        App.enterNewPage("Fk.Pages.Lobby", "Lobby")
    })
}
```

**Lua 层**：

```lua
-- 方式1: 在 Client:initialize() 中注册
self:addCallback("CommandName", self.handleCommand)
self:addCallback("CommandName", self.handleCommand, true)  -- true: 同时通知 UI

-- 方式2: 注册到全局回调表
fk.client_callback["CommandName"] = function(self, data)
    -- 处理消息
end
```

### 消息类型

| 类型 | 说明 | 客户端方法 |
|------|------|------------|
| **Notify** | 单向通知，无需响应 | `notifyServer()` |
| **Request** | 服务端请求，需要响应 | 服务端发起 |
| **Reply** | 响应服务端请求 | `replyToServer()` |

### 关键文件

| 文件 | 说明 |
|------|------|
| `src/network/client_socket.cpp` | TCP Socket 封装 |
| `src/network/router.cpp` | 协议路由层 |
| `src/client/client.cpp` | 客户端核心类 |
| `packages/freekill-core/Fk/Base/CppUtil.qml` | QML 工具类 |
| `packages/freekill-core/lua/client/clientbase.lua` | Lua 客户端基类 |

---

## 2. Lua RPC 通信

### 概述

Lua 游戏进程通过 stdio (stdin/stdout) 与 Java 服务端进行 JSON-RPC 通信。

### 特点

- **同步阻塞**：调用后暂停 Lua 流程，直到收到响应
- **进程间通信**：Lua 进程与 Java 进程通过管道通信
- **底层操作**：用于 Lua 游戏逻辑与 Java 后端数据交互

### 使用场景

- 玩家状态存档读写
- 房间数据管理
- 日志输出
- 底层系统调用

### 内置 RPC 方法

**日志输出**：

```lua
fk.qDebug("调试信息: %s", message)
fk.qInfo("普通信息: %s", message)
fk.qWarning("警告信息: %s", message)
fk.qCritical("严重错误: %s", message)
print("打印输出")  -- 也会通过 RPC 发送
```

**房间操作**：

```lua
-- 延迟
room:delay(1000)  -- 毫秒

-- 胜率更新
room:updatePlayerWinRate(playerId, mode, role, result)
room:updateGeneralWinRate(general, mode, role, result)

-- 游戏结束
room:gameOver()

-- 会话数据
local sessionId = room:getSessionId()
local data = room:getSessionData()
room:setSessionData(jsonData)

-- NPC 管理
local npc = room:addNpc()
room:removeNpc(player)

-- 全局状态存储
room:saveGlobalState(key, jsonData)
local state = room:getGlobalSaveState(key)
```

**玩家操作**：

```lua
-- 请求/响应
player:doRequest(command, jsonData, timeout, timestamp)
local reply = player:waitForReply(timeout)
player:doNotify(command, jsonData)

-- 状态管理
local thinking = player:thinking()
player:setThinking(true)
player:setDied(true)
player:emitKick()

-- 存档
player:saveState(jsonData)
local state = player:getSaveState()
player:saveGlobalState(key, jsonData)
local globalState = player:getGlobalSaveState(key)
```

### 注意事项

> **警告**：所有 `callRpc` 调用都是**同步阻塞**的，会暂停 Lua 执行直到 Java 端返回响应。
> 避免在高频循环中调用 RPC 方法。

### 关键文件

| 文件 | 说明 |
|------|------|
| `packages/freekill-core/lua/server/rpc/fk.lua` | RPC 封装层 |
| `packages/freekill-core/lua/server/rpc/jsonrpc.lua` | JSON-RPC 协议 |
| `packages/freekill-core/lua/server/rpc/stdio.lua` | stdio 通信 |
| `hero-kill-game/src/.../lua/LuaRpcDispatcher.java` | Java 端 RPC 分发器 |

---

## 3. AdminService RPC

### 概述

基于 Lua RPC 封装的通用业务调用接口，提供统一的业务逻辑调用入口。

### 特点

- **统一入口**：所有业务调用走同一接口，易于管理
- **可扩展**：新增业务只需在 Java 端注册 action handler
- **解耦**：Lua 不需要知道具体实现细节
- **支持异步**：`executeAsync` 适合不需要返回值的操作

### 使用场景

- 战局结算、胜率更新
- 玩家数据查询/修改
- 管理员操作
- 任何需要调用 Java 业务逻辑的场景

### API

```lua
-- 同步调用，等待结果返回
-- @param action string 操作名称
-- @param params table 参数表
-- @return table|nil 返回结果（CBOR 解码后）
local result = fk.AdminService:execute(action, params)

-- 异步调用，fire-and-forget
-- Java 端立即返回，不等待业务执行完成
-- @param action string 操作名称
-- @param params table 参数表
fk.AdminService:executeAsync(action, params)
```

### 使用示例

**Lua 端调用**：

```lua
-- 同步调用 - 更新胜率
local result = fk.AdminService:execute("updateWinRate", {
    playerId = 123,
    mode = "melee",
    role = "lord",
    result = "win"
})

if result and result.success then
    fk.qInfo("胜率更新成功")
end

-- 异步调用 - 记录游戏事件（不需要返回值）
fk.AdminService:executeAsync("logGameEvent", {
    event = "game_start",
    roomId = self.room:getId(),
    players = playerIds,
    timestamp = os.time()
})

-- 同步调用 - 查询玩家数据
local playerData = fk.AdminService:execute("getPlayerData", {
    playerId = player:getId()
})
```

**Java 端实现**：

```java
// 1. 实现 AdminActionHandler 接口
@Component
public class UpdateWinRateHandler implements AdminActionHandler {

    @Override
    public String getAction() {
        return "updateWinRate";
    }

    @Override
    public Object execute(Map<String, Object> params) {
        Long playerId = (Long) params.get("playerId");
        String mode = (String) params.get("mode");
        String role = (String) params.get("role");
        String result = (String) params.get("result");

        // 业务逻辑
        winRateService.update(playerId, mode, role, result);

        return Map.of("success", true);
    }
}

// 2. Handler 会自动注册到 AdminService
```

### execute vs executeAsync

| 方法 | 阻塞 | 返回值 | 适用场景 |
|------|------|--------|----------|
| `execute` | 是 | 有 | 需要返回结果的操作 |
| `executeAsync` | 是* | 无 | 不需要返回值的操作 |

> *注：`executeAsync` 的 RPC 调用本身仍是同步的，但 Java 端会立即返回，不等待实际业务执行完成。

### 关键文件

| 文件 | 说明 |
|------|------|
| `packages/freekill-core/lua/server/rpc/fk.lua` | Lua 端 AdminService 封装 |
| `hero-kill-game/src/.../service/AdminService.java` | Java 端服务实现 |

---

## 协议格式

### TCP 消息包结构

```
消息包 (CBOR Array):
[requestId, type, command, data, timeout?, timestamp?]

字段说明:
- requestId: 请求ID，-2 表示 Notify 类型
- type: 消息类型 (PacketType 组合)
- command: 命令名称 (字符串)
- data: CBOR 编码的数据
- timeout: 超时时间（仅 Request）
- timestamp: 时间戳（仅 Request）
```

### PacketType 枚举

```cpp
enum PacketType {
    COMPRESSED = 0x1000,        // 压缩标志位
    TYPE_REQUEST = 0x100,       // Request 类型
    TYPE_REPLY = 0x200,         // Reply 类型
    TYPE_NOTIFICATION = 0x400,  // Notify 类型
    SRC_CLIENT = 0x010,         // 来源：客户端
    SRC_SERVER = 0x020,         // 来源：服务端
    DEST_CLIENT = 0x001,        // 目标：客户端
    DEST_SERVER = 0x002,        // 目标：服务端
};
```

### JSON-RPC 格式 (Lua RPC)

**请求**：
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "methodName",
    "params": ["param1", "param2"]
}
```

**响应**：
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": { "key": "value" }
}
```

**错误响应**：
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "error": {
        "code": -32600,
        "message": "Invalid Request"
    }
}
```

---

## 消息流程图

### TCP 直连通信流程

```
┌─────────────┐                              ┌─────────────┐
│   QML/Lua   │                              │  Java 服务端 │
│   Client    │                              │  (Game)     │
└──────┬──────┘                              └──────┬──────┘
       │                                            │
       │  Cpp.notifyServer("Command", data)         │
       │ ──────────────────────────────────────────>│
       │         [TCP/CBOR Notify]                  │
       │                                            │
       │                                            │
       │         服务端处理后发送通知                  │
       │ <──────────────────────────────────────────│
       │         [TCP/CBOR Notify]                  │
       │                                            │
       │  addCallback("Command", handler)           │
       │  ← 触发回调处理                             │
       │                                            │
```

### Lua RPC 通信流程

```
┌─────────────┐                              ┌─────────────┐
│  Lua 进程    │                              │  Java 服务端 │
│  (游戏逻辑)  │                              │  (Game)     │
└──────┬──────┘                              └──────┬──────┘
       │                                            │
       │  callRpc("method", params)                 │
       │ ──────────────────────────────────────────>│
       │         [stdio/JSON-RPC]                   │
       │                                            │
       │         (Lua 阻塞等待)                      │
       │                                            │
       │         返回结果                            │
       │ <──────────────────────────────────────────│
       │         [stdio/JSON-RPC]                   │
       │                                            │
       │  继续执行                                   │
       │                                            │
```

### AdminService 调用流程

```
┌─────────────┐                              ┌─────────────┐
│  Lua 进程    │                              │  Java 服务端 │
│  (游戏逻辑)  │                              │  (Game)     │
└──────┬──────┘                              └──────┬──────┘
       │                                            │
       │  fk.AdminService:execute("action", params) │
       │ ──────────────────────────────────────────>│
       │         [stdio/JSON-RPC]                   │
       │                                            │
       │                              ┌─────────────┴─────────────┐
       │                              │  AdminService.execute()   │
       │                              │  → 路由到对应 Handler      │
       │                              │  → 执行业务逻辑            │
       │                              └─────────────┬─────────────┘
       │                                            │
       │         返回 CBOR 编码结果                  │
       │ <──────────────────────────────────────────│
       │                                            │
       │  result = cbor.decode(response)            │
       │                                            │
```

---

## 最佳实践

### 1. 选择合适的通信方式

| 场景 | 推荐方式 | 原因 |
|------|----------|------|
| 游戏内实时操作 | TCP 直连 | 低延迟，非阻塞 |
| 业务逻辑调用 | AdminService | 统一入口，可扩展 |
| 底层数据操作 | Lua RPC | 直接访问服务端数据 |
| 不需要返回值 | executeAsync | 减少阻塞时间 |

### 2. 避免阻塞

```lua
-- 不好：在循环中频繁调用同步 RPC
for _, player in ipairs(players) do
    fk.AdminService:execute("updatePlayer", { id = player:getId() })  -- 每次都阻塞
end

-- 好：批量处理
local playerIds = {}
for _, player in ipairs(players) do
    table.insert(playerIds, player:getId())
end
fk.AdminService:execute("updatePlayers", { ids = playerIds })  -- 一次调用
```

### 3. 错误处理

```lua
-- AdminService 调用
local result = fk.AdminService:execute("action", params)
if result == nil then
    fk.qWarning("AdminService 调用失败: action=%s", "action")
    return
end

if not result.success then
    fk.qWarning("业务处理失败: %s", result.error or "unknown")
    return
end
```

### 4. 新增业务接口

**步骤**：

1. **Java 端**：实现 `AdminActionHandler` 接口
2. **注册**：Handler 会自动注册到 AdminService
3. **Lua 端**：调用 `fk.AdminService:execute("actionName", params)`

```java
// Java 端
@Component
public class MyHandler implements AdminActionHandler {
    @Override
    public String getAction() { return "myAction"; }

    @Override
    public Object execute(Map<String, Object> params) {
        // 业务逻辑
        return Map.of("success", true, "data", result);
    }
}
```

```lua
-- Lua 端
local result = fk.AdminService:execute("myAction", {
    param1 = "value1",
    param2 = 123
})
```

---

## 附录：常用命令列表

### 客户端 → 服务端

| 命令 | 说明 |
|------|------|
| `Heartbeat` | 心跳 |
| `Login` | 登录 |
| `Logout` | 登出 |
| `CreateRoom` | 创建房间 |
| `JoinRoom` | 加入房间 |
| `QuitRoom` | 退出房间 |
| `Chat` | 聊天消息 |
| `PlayCard` | 出牌 |
| `UseSkill` | 使用技能 |
| `RefreshRoomList` | 刷新房间列表 |

### 服务端 → 客户端

| 命令 | 说明 |
|------|------|
| `NetworkDelayTest` | 网络延迟测试 |
| `Setup` | 初始化设置 |
| `EnterLobby` | 进入大厅 |
| `EnterRoom` | 进入房间 |
| `StartGame` | 游戏开始 |
| `GameOver` | 游戏结束 |
| `AddPlayer` | 添加玩家 |
| `RemovePlayer` | 移除玩家 |
| `MoveCards` | 卡牌移动 |
| `PlayCard` | 出牌请求 |
| `AskForSkillInvoke` | 询问技能发动 |
| `ErrorMsg` | 错误消息 |
| `Chat` | 聊天消息 |
