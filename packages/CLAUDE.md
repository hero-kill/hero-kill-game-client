[根目录](../../CLAUDE.md) > [hero-kill-game](../../hero-kill-game/CLAUDE.md) > **packages**

# Lua 游戏脚本文档

> 最后更新: 2026-01-23 20:06:37

## 概述

本目录包含英雄杀游戏的 Lua 脚本，基于 **FreeKill (新月杀)** 框架开发。通过 Lua 脚本实现游戏模式、武将技能、卡牌效果等游戏逻辑，与 Java 后端通过 JSON-RPC 通信。

## 目录结构

```
packages/
└── gamemode/                    # 游戏模式扩展包
    ├── init.lua                 # 主入口，加载所有模块
    ├── i18n/                    # 国际化
    │   └── en_US.lua
    ├── qml/                     # QML UI 组件
    │   └── 1v1.qml
    └── pkg/                     # 子包
        ├── gamemodes/           # 游戏模式定义
        ├── jiange_generals/     # 守卫剑阁武将
        ├── 3v3_generals/        # 3v3专属武将
        ├── gamemode_generals/   # 模式专属武将
        ├── 1v1_generals/        # 1v1专属武将
        ├── variation_cards/     # 应变卡牌
        ├── mobile_cards/        # 移动端卡牌
        ├── chaos_mode_cards/    # 混战卡牌
        └── ...                  # 其他卡牌包
```

## 主入口 (init.lua)

`gamemode/init.lua` 是扩展包的主入口，负责：

1. 加载游戏模式规则技能
2. 注册所有游戏模式
3. 加载卡牌包和武将包
4. 加载国际化翻译

```lua
local extension = Package:new("gamemode", Package.SpecialPack)

-- 加载游戏模式
extension:addGameMode(require "packages/gamemode/pkg/gamemodes/role")
extension:addGameMode(require "packages/gamemode/pkg/gamemodes/3v3")
-- ...

-- 返回所有模块
return { extension, mobile_cards, jiange_generals, ... }
```

---

## 游戏模式

### 模式列表

| 模式 | 文件 | 人数 | 说明 |
|------|------|------|------|
| 身份模式 | `role.lua` | 2-10 | 经典身份局 |
| 1v1 | `1v1.lua` | 2 | 单挑模式 |
| 1v2 | `1v2.lua` | 3 | 1打2模式 |
| 1v2乱斗 | `1v2_brawl.lua` | 3 | 1v2变体 |
| 1v3 | `1v3.lua` | 4 | 1打3模式 |
| 2v2 | `2v2.lua` | 4 | 双人组队 |
| 3v3 | `3v3.lua` | 6 | 王者之战 |
| 混战 | `chaos_mode.lua` | 2-8 | 自由混战 |
| 谍战 | `espionage.lua` | 5-10 | 暗身份模式 |
| 应变 | `variation.lua` | 2-8 | 应变卡牌 |
| 卧龙 | `vanished_dragon.lua` | 2-8 | 卧龙模式 |
| 七夕 | `qixi.lua` | 2-8 | 七夕活动 |
| 僵尸 | `zombie_mode.lua` | 5-10 | 僵尸模式 |
| 抗秦 | `kangqin.lua` | 2-8 | 抗秦模式 |
| 守卫剑阁 | `jiange.lua` | 8 | 蜀魏对抗 |
| 乱斗 | `melee.lua` | 2-8 | 乱斗模式 |

### 模式定义结构

```lua
local mode = fk.CreateGameMode{
  name = "m_3v3_mode",           -- 模式ID
  minPlayer = 6,                 -- 最小人数
  maxPlayer = 6,                 -- 最大人数
  logic = getLogicFunction,      -- 游戏逻辑类
  rule = "#m_3v3_rule&",         -- 规则技能
  surrender_func = function(...) end,  -- 投降条件
  whitelist = { "3v3_cards" },   -- 允许的卡牌包
  winner_getter = function(...) end,   -- 胜利判定
  get_adjusted = function(...) end,    -- 属性调整
  build_draw_pile = function(...) end, -- 构建牌堆
  reward_punish = function(...) end,   -- 击杀奖惩
  friend_enemy_judge = function(...) end, -- 敌友判定
}
```

---

## 武将包

### 守卫剑阁武将 (jiange_generals)

**模式限定**: `jiange_mode`

#### 蜀国英雄 (jiange_hero = true)

| 武将 | ID | 体力 | 技能 |
|------|-----|------|------|
| 烈帝玄德 | `jiange__liubei` | 6 | 激阵、凌风、擒震 |
| 天侯孔明 | `jiange__zhugeliang` | 5 | 变天、八阵 |
| 工神月英 | `jiange__huangyueying` | 5 | 工神、智囊、精妙 |
| 浴火士元 | `jiange__pangtong` | 5 | 浴火、奇雾、天佑 |
| 翊汉云长 | `jiange__guanyu` | 6 | 骁锐、虎臣、天降 |
| 扶危子龙 | `jiange__zhaoyun` | 6 | 封剑、克定、龙威 |
| 威武翼德 | `jiange__zhangfei` | 5 | 猛武、虎魄、蜀魂 |
| 神箭汉升 | `jiange__huangzhong` | 5 | 奇险、精攻、悲矢 |

#### 魏国英雄 (jiange_hero = true)

| 武将 | ID | 体力 | 技能 |
|------|-----|------|------|
| 佳人子丹 | `jiange__caozhen` | 5 | 驰影、惊帆、震袭 |
| 断狱仲达 | `jiange__simayi` | 5 | 控魂、反噬、玄雷 |
| 绝尘妙才 | `jiange__xiahouyuan` | 5 | 穿云、雷厉、风行 |
| 巧魁儁乂 | `jiange__zhanghe` | 5 | 火地、绝击 |
| 百计文远 | `jiange__zhangliao` | 5 | 狡谐、帅领 |
| 枯目元让 | `jiange__xiahoudun` | 5 | 霸势、胆惊、统军 |
| 古之恶来 | `jiange__dianwei` | 5 | 鹰击、震遏、卫主 |
| 毅勇文则 | `jiange__yujin` | 5 | 悍军、劈挂、征击 |

#### 蜀国机关兽 (jiange_machine = true)

| 武将 | ID | 体力 | 技能 |
|------|-----|------|------|
| 云屏青龙 | `jiange__qinglong` | 5 | 魔剑、机关 |
| 机雷白虎 | `jiange__baihu` | 5 | 镇威、奔雷、机关 |
| 炽羽朱雀 | `jiange__zhuque` | 5 | 天陨、浴火、机关 |
| 灵甲玄武 | `jiange__xuanwu` | 5 | 毅重、机关、灵御 |

#### 魏国机关兽 (jiange_machine = true)

| 武将 | ID | 体力 | 技能 |
|------|-----|------|------|
| 缚地狴犴 | `jiange__bihan` | 5 | 地动、机关 |
| 食火狻猊 | `jiange__suanni` | 4 | 机关、炼狱 |
| 吞天螭吻 | `jiange__chiwen` | 6 | 机关、贪食、吞噬 |
| 裂石睚眦 | `jiange__yazi` | 6 | 机关、奈落 |

### 3v3专属武将 (3v3_generals)

**模式限定**: `m_3v3_mode`

| 武将 | ID | 势力 | 体力 | 技能 |
|------|-----|------|------|------|
| 诸葛瑾 | `v33__zhugejin` | 吴 | 3 | 缓释、弘援、明哲 |
| 文聘 | `v33__wenpin` | 魏 | 4 | 镇威 |
| 夏侯惇 | `v33__xiahoudun` | 魏 | 4 | 刚烈 |
| 关羽 | `v33__guanyu` | 蜀 | 4 | 武圣、忠义 |
| 赵云 | `v33__zhaoyun` | 蜀 | 4 | 龙胆、救主 |
| 吕布 | `v33_nos__lvbu` | 群 | 4 | 无双、战神 |
| 黄权 | `v33__huangquan` | 蜀 | 3 | 酬金、忠谏 |
| 徐盛 | `v33__xusheng` | 吴 | 4 | 疑城 |

### 模式专属武将 (gamemode_generals)

包含 2v2、1v1、忠胆、应变、文和乱武等模式的专属武将。

---

## 技能系统

### 技能定义结构

```lua
local skillName = fk.CreateSkill {
  name = "jiange__longwei",  -- 技能ID (格式: 包名__技能名)
}

-- 加载翻译
Fk:loadTranslationTable{
  ["jiange__longwei"] = "龙威",
  [":jiange__longwei"] = "当友方角色进入濒死状态时，你可以减1点体力上限，令其回复1点体力。",
  ["#jiange__longwei-invoke"] = "龙威：是否减1点体力上限，令 %dest 回复1点体力？",
}

-- 添加技能效果
skillName:addEffect(fk.EnterDying, {
  anim_type = "support",           -- 动画类型
  can_trigger = function(...) end, -- 触发条件
  on_cost = function(...) end,     -- 消耗/询问
  on_use = function(...) end,      -- 执行效果
})

return skillName
```

### 常用事件类型

| 事件 | 说明 |
|------|------|
| `fk.EnterDying` | 进入濒死 |
| `fk.Damage` | 造成伤害 |
| `fk.Damaged` | 受到伤害 |
| `fk.CardUsing` | 使用卡牌 |
| `fk.DrawNCards` | 摸牌阶段 |
| `fk.EventPhaseStart` | 阶段开始 |
| `fk.Death` | 角色死亡 |
| `fk.GameStart` | 游戏开始 |

### 常用 Room API

```lua
room:changeMaxHp(player, -1)           -- 修改体力上限
room:recover{ who = target, num = 1 }  -- 回复体力
room:damage{ from = p, to = t, damage = 1 } -- 造成伤害
room:drawCards(player, 2, "skillName") -- 摸牌
room:askToSkillInvoke(player, {...})   -- 询问发动
room:askToChoosePlayers(player, {...}) -- 选择角色
```

---

## 卡牌包

| 卡牌包 | 文件 | 说明 |
|--------|------|------|
| mobile_cards | `mobile_cards/` | 移动端专属卡牌 |
| chaos_mode_cards | `chaos_mode_cards/` | 混战模式卡牌 |
| vanished_dragon_cards | `vanished_dragon_cards/` | 卧龙模式卡牌 |
| variation_cards | `variation_cards/` | 应变模式卡牌 |
| certamen_cards | `certamen_cards/` | 竞技卡牌 |
| derived_cards | `derived_cards/` | 衍生卡牌 |
| 3v3_cards | `3v3_cards/` | 3v3专属卡牌 |
| 1v1_cards | `1v1_cards/` | 1v1专属卡牌 |

### 应变卡牌示例

| 卡牌 | 文件 | 效果 |
|------|------|------|
| 冰杀 | `ice__slash.lua` | 冰属性杀 |
| 水淹七军 | `drowning.lua` | 群体伤害 |
| 出其不意 | `unexpectation.lua` | 突袭效果 |
| 太公阴符 | `taigong_tactics.lua` | 策略卡 |
| 铜雀台 | `bronze_sparrow.lua` | 场地卡 |

---

## 开发指南

### 新增武将

1. 在对应武将包的 `skills/` 目录下创建技能文件
2. 在武将包的 `init.lua` 中注册武将和技能
3. 添加翻译文本

```lua
-- skills/newskill.lua
local newskill = fk.CreateSkill { name = "pkg__newskill" }
Fk:loadTranslationTable{ ["pkg__newskill"] = "新技能", ... }
newskill:addEffect(fk.EventType, { ... })
return newskill

-- init.lua
General:new(extension, "pkg__general", "kingdom", hp):addSkills { "pkg__newskill" }
```

### 新增游戏模式

1. 在 `gamemodes/` 目录下创建模式文件
2. 实现 `GameLogic` 子类
3. 使用 `fk.CreateGameMode` 创建模式
4. 在 `init.lua` 中注册

### 调试技巧

- 使用 `fk.qCritical("message")` 输出日志
- 检查 Java 端 `LuaRpcDispatcher` 日志
- 确保技能 ID 唯一，格式为 `包名__技能名`

---

## 与 Java 后端交互

Lua 脚本通过 JSON-RPC 与 Java 后端通信：

- **Java → Lua**: `LuaGameCaller` 调用 Lua 函数
- **Lua → Java**: 通过 `@LuaRpcMethod` 注解的方法

### 配置路径

```yaml
# application.yml
game:
  server:
    packages-path: hero-kill-game/packages

lua:
  path: /opt/homebrew/bin/lua5.4
  entry-script: lua/server/rpc/entry.lua
```

---

## 变更记录

| 日期 | 变更内容 |
|------|----------|
| 2026-01-23 | 初始化 Lua 脚本文档 |
