# Popup 开发指南

本指南描述如何在 `hero-kill-game-client` 中新增弹窗，并确保在任意页面都能正确居中显示。

## 1. 新增弹窗 QML

弹窗必须**基于通用弹窗组件**实现。推荐继承 `W.RpgPopup`（`Fk.Widgets`），以获得统一的尺寸、标题栏、缩放与居中行为：

```qml
import QtQuick
import Fk.Widgets as W

W.RpgPopup {
    id: root
    title: "示例弹窗"
    designWidth: 800
    designHeight: 560
    contentPadding: 0

    // 内容区
    // ...
}
```

## 2. 全局弹窗：使用 PopupManager

如果弹窗需要在任意页面触发（例如：私聊回复、全局通知），使用 `PopupManager` 单例动态创建并挂载到全局 Overlay。

### 2.1 注册单例

`Fk/Base/qmldir` 中添加：

```
singleton PopupManager 1.0 PopupManager.qml
```

### 2.2 在 PopupManager 中添加入口

`Fk/Base/PopupManager.qml` 中添加方法：

```qml
function showExamplePopup(parentItem) {
    var component = Qt.createComponent("../Pages/Example/ExamplePopup.qml");
    if (component.status === Component.Ready) {
        var popup = component.createObject(parentItem || null, {});
        if (popup) popup.open();
    }
}
```

### 2.3 调用方式

在任何页面中调用：

```javascript
PopupManager.showExamplePopup(root);
```

`root` 推荐传当前页面的根节点，确保坐标系正确。

## 3. 页面内弹窗（局部弹窗）

如果弹窗只在页面内使用（例如：页面局部配置），可以直接在页面中声明：

```qml
MyPopup { id: myPopup }
```

并通过 `myPopup.open()` 调用。

## 4. LobbyPopup 与 RpgPopup 的关系

`LobbyPopup.qml` 是 **业务弹窗**，继承自通用组件 `W.RpgPopup`：

- `RpgPopup.qml`：通用弹窗基类（标题栏、居中、缩放、遮罩、动画）
- `LobbyPopup.qml`：基于 `RpgPopup` 的大厅业务封装（根据 `PanelType` 加载不同内容）

简单理解：**RpgPopup 是底座，LobbyPopup 是具体业务实现。**

## 5. 常见问题

### 5.1 弹窗不居中

- 确认弹窗父级正确（全局弹窗应挂载到当前页面根节点或 Overlay 层）
- 确认弹窗使用了 `W.RpgPopup`（内部默认 `anchors.centerIn: parent`）

### 5.2 弹窗不显示

- 检查 `Qt.createComponent` 路径是否正确
- 确认组件 `status` 为 `Component.Ready`
- 确认 `open()` 被调用
