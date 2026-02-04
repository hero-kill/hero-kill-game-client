## Lua Debug Logs 记录流程

这份文档记录近期在客户端 Lua 侧排查 `getEffectiveId` 报错时的调试流程，方便下次直接复用。

### 目标
- 追踪客户端收到的命令顺序与 payload 长度
- 在 `GameLog/LogEvent` 解析崩溃前，确认 `card` 结构是否异常
- 避免 `io.open` / `qInfo` 使用错误导致二次报错

### 调试点 1：统一入口 `ClientCallback`
文件：`packages/freekill-core/lua/client/client.lua`

用途：记录每个命令的到达顺序和 payload 长度，定位“哪条命令触发了崩溃”。

示例做法（伪代码，按需粘贴）：
```
local function debugLog(msg)
  if type(fk) == "table" and type(fk.qInfo) == "function" then
    fk.qInfo("[DEBUG] " .. msg)
  end
end

ClientCallback = function(_self, command, jsonData, isRequest)
  debugLog("ClientCallback command=" .. tostring(command)
    .. ",len=" .. tostring(jsonData and #jsonData or 0))
  ...
end
```

注意：
- 客户端环境多数情况下 `io.open` 不可用，优先用 `fk.qInfo` 输出
- `fk.qInfo` 只接受 **1 个字符串参数**，不要用类似 `fmt, arg1, arg2` 的传参方式

### 调试点 2：`GameLog` 解析入口 `parseMsg`
文件：`packages/freekill-core/lua/lunarltk/client/client.lua`

用途：确认 `card` 类型与内容是否符合预期，尤其是 `card` 内元素是否包含 `getEffectiveId` 方法。

示例做法（伪代码，按需粘贴）：
```
local function debugLog(msg)
  if type(fk) == "table" and type(fk.qInfo) == "function" then
    fk.qInfo("[DEBUG] " .. msg)
  end
end

function Client:parseMsg(msg, nocolor, visible_data)
  local data = msg
  local card = data.card or Util.DummyTable
  debugLog("parseMsg type=" .. tostring(data.type)
    .. ",cardType=" .. tostring(type(card))
    .. ",cardLen=" .. tostring(#card))

  for _, id in ipairs(card) do
    if type(id) == "table" and type(id.getEffectiveId) ~= "function" then
      debugLog("card missing getEffectiveId, keys=" .. table.concat(table.keys(id), ","))
    end
  end
  ...
end
```

建议只在复现阶段打开，验证后移除。

### 典型日志片段（示例）
```
[DEBUG] ClientCallback command=GameLog,len=72
[DEBUG] parseMsg type=#UseCardToTargets,cardType=table,cardLen=1
[DEBUG] card missing getEffectiveId, keys=2,6,7,9
```

### 这次问题的结论
崩溃发生在 `GameLog` 中 `card` 元素退化成普通 table（无 `getEffectiveId`）时。
根因是服务端 Lua 通知被“解码再编码”，导致对象信息丢失。
修复方式：服务端对 Lua 通知改为 **原始 CBOR 透传**。

### 清理原则
调试确认完成后：
- 删除或注释临时 `debugLog` 与相关输出
- 保持生产代码无额外日志噪音
