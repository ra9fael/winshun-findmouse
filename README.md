# ra9fael.findmouse — 晃动鼠标找光标（Win顺 插件）

> **简体中文（默认）** · [English](README.en.md)
>
> Win顺 默认使用中文界面，也自带英文模式。因此中文说明放在 `README.md`，
> 英文说明放在 `README.en.md`，两份内容保持一致，可互相跳转。

**快速晃动鼠标**，指针处就会闪出定位光效。光效会**跟随指针**，直到你按下鼠标按键，
再也不用满屏找光标。键盘路径由 `mouse` 关键词覆盖：手动触发光效、开关晃动检测、
复制诊断信息。

## 设计目的

多显示器、高 DPI 屏幕下，光标很容易丢失。PowerToys Find My Mouse 这类大型桌面工具
要么靠任意点击触发，要么得记一个快捷键。本插件用的是你找光标时本来就会做的动作：
快速左右晃动。动作出现之前屏幕上什么都没有——没有误触点击、不用记快捷键、
也没有常驻的 HUD。

## 安装

1. 从 [最新 Release](https://github.com/ra9fael/winshun-findmouse/releases/latest)
   下载 `ra9fael.findmouse-<version>.wsplugin`。可选：用旁边的 `.sha256` 文件核对
   SHA-256（`Get-FileHash <file> -Algorithm SHA256`）。
2. 在 Win顺 里打开 **设置 → 插件 → 已安装 → 添加插件 → 压缩包（.zip / .wsplugin）**，
   选中该文件。
3. 从本地安装包安装的插件**不会自动启用**。请在列表里打开开关；启用这一步就是你
   在确认它申请的权限（见 [权限说明](#权限说明)）。

**更新：** 用同样方式安装新版的 `.wsplugin`。Win顺 认出是同一个 id，会先询问是否
替换旧版本，并保留插件的数据目录。

**卸载：** 在 **设置 → 插件 → 已安装** 里移除。

本插件是原生 exe，不会出现在 Win顺 应用内的插件库里（社区索引只收脚本插件）。
本仓库的 Release 是唯一的分发渠道。

## 从源码构建

环境要求：Windows 10/11 x64、CMake 3.21+，以及 Visual Studio 2022 及以上（MSVC）
或 MinGW-w64 GCC 13+。打包和集成测试需要 Python 3.10+。

```powershell
cmake -S . -B build -A x64                 # MSVC；MinGW 用 -G "MinGW Makefiles"
cmake --build build --config Release       # 在 plugin.json 旁边生成 findmouse.exe
ctest --test-dir build -C Release --output-on-failure
python tools/package.py                    # dist/ra9fael.findmouse-<version>.wsplugin
```

本地构建如何直接在 Win顺 里运行，以及测试和发版流程，见
[CONTRIBUTING.md](CONTRIBUTING.md)。

## 关键词与示例

| 输入 | 效果 |
|---|---|
| `mouse` | 三行结果：显示光效 / 开关检测 / 复制诊断 |
| `mouse 显示` | 过滤出文本匹配的行（宿主也参与匹配） |
| `mouse` + **Enter** | 在指针处显示光效，并跟随到点击为止 |
| `mouse` + **Ctrl+Enter** | 开关晃动检测（立即生效并持久化） |
| `mouse` + **Shift+Enter** | 复制诊断信息到剪贴板 |

动作按 `plugin.json` 里声明的顺序排列（`key` 只是显示文本；Enter / Ctrl+Enter /
Shift+Enter 的顺序才是实际绑定）。所有功能都能从键盘到达，没有隐藏关键词。

## 手势与光效

**检测**跟踪指针的**水平方向反转**，带 8 px 滞回。一次手势 = 300 ms 内 4 次反转，
且每段行程的峰值速度不低于 500 px/s（以上均为默认值）；触发后检测器静默 1.5 s。
竖直晃动、拖拽（任意按键处于按下状态）、注入输入（远控工具、测试程序）一律忽略。

**光效**每帧跟随指针，在按下**鼠标左键或右键**时结束（每帧用 `GetAsyncKeyState`
轮询，结束逻辑不经过 hook），或到达 `followTimeoutMs` 设定的时长后结束
（**0–60000 ms，默认 0 = 永不自动消失**）。它不是突然消失，而是在 `fadeOutMs`
内**淡出**（**0–3000 ms，默认 400 ms**；整窗不透明度按二次曲线衰减，期间仍在动画
并继续跟随指针；0 = 立即隐藏）。淡出期间不再轮询按键。跟随模式下再次触发只是重启动画；
淡出期间触发会把光效恢复到全不透明。每种样式都保留一个固定的中心点，保证指针的
精确位置不会丢。`rippleEffect` 设置可切换五种样式：

| 样式 | 效果 |
|---|---|
| `ring`（默认） | 单个圆环从 35% 半径扩张到满半径，每周期淡出 |
| `waves` | 三个错开的扩张涟漪，始终至少有一个在动 |
| `halo` | 指针周围低透明度的柔和呼吸光晕圆盘 |
| `radar` | 静态圆环内旋转的声呐扫描 |
| `custom` | 由你自己的 JSON 图层组成（见下方「自定义光效」） |

可调参数（解析时全部钳制到安全范围，单位写在设置标题里）：

| 设置 | 默认值 | 含义 |
|---|---|---|
| `shakeStrokePx` | 8 px | 最小行程距离；更短的回退算滞回，不算反转 |
| `shakeMinSpeed` | 500 px/s | 速度门槛；峰值速度更低的算拖动而非晃动，0 = 关闭 |
| `shakeReversals` | 4 | 需要的方向变化次数 |
| `shakeWindowMs` | 300 ms | 最近若干次反转必须落在这个时间窗内 |
| `shakeCooldownMs` | 1500 ms | 触发后的静默时间 |
| `rippleEffect` | ring | 光效样式：ring / waves / halo / radar / custom |
| `followTimeoutMs` | 0（不消失） | 0–60000 ms；0 = 一直跟随到按下鼠标键 |
| `fadeOutMs` | 400 ms | 0–3000 ms；点击或超时后的淡出时长；0 = 立即 |
| `rippleMs` | 700 ms | 脉冲周期；未指定 `loopMs` 的自定义样式也用它 |
| `rippleSize` | 64 px | 静止时的半径（以 96 dpi 为基准，随显示缩放） |
| `rippleColor` | #2AA8FF | 主色；自定义 JSON 里的 `"accent"` 会解析成它 |
| `customStyle` | 空 | 自定义样式 JSON，仅当 `rippleEffect` 为 `custom` 时使用 |

刻意的设计选择：门槛是**每段行程的峰值速度**，不是加速度。在 1000 Hz 上报率下，
逐事件加速度几乎全是噪声；速度下限能达到同样目的（慢速移动和拖拽不会触发），
而且可度量。

## 自定义光效（JSON）

把 `rippleEffect` 设为 `custom`，再把样式文档粘进 `customStyle` 设置。它是
**声明式** JSON：半径是结构化的曲线对象，不是用户表达式，因此没有注入空间。
每个数值都会被钳制，任何未知的 shape、mode 或 color 都会让整份文档被拒绝；
解析失败时保留上一个样式，并在诊断信息（Shift+Enter）里显示
`custom style: parse error`。

```json
{
  "loopMs": 900,
  "layers": [
    {"shape": "ring",  "from": 0.2, "to": 1.0, "ease": "outCubic",
     "width": 3.5, "offset": 0, "phase": 0,
     "alpha": {"mode": "linearFade", "value": 1.0}, "color": "accent"},
    {"shape": "ring",  "from": 0.2, "to": 1.0, "ease": "outCubic",
     "width": 1.6, "offset": 6.5, "phase": 0,
     "alpha": {"mode": "linearFade"}, "color": "#FFFFFF"},
    {"shape": "disc",  "radius": 0.05, "alpha": 1.0, "color": "accent"},
    {"shape": "sweep", "speed": 1.0, "trailDeg": 120, "fill": true,
     "alpha": {"mode": "constant", "value": 0.85}, "color": "accent"}
  ]
}
```

| 字段 | 含义 |
|---|---|
| `loopMs` | 200–5000 ms 周期长度；缺省则用 `rippleMs` 设置 |
| `layers` | 1–12 个图层，合成时 alpha 最大者胜出 |
| `shape` | `ring` / `disc` / `sweep` |
| `from` / `to` + `ease` | 周期内变化的半径；静态则用 `radius` |
| 半径单位 | `rippleSize` 设置的比例（1.0 = 满半径） |
| `width` / `offset` | 96 dpi 下的 px：圆环半宽 / 向外偏移（是描边！） |
| `phase` | 0–1 的图层周期偏移（0、⅓、⅔ → 连续波纹） |
| `alpha` | `{"mode": "constant"/"linearFade"/"holdFade"/"sin", "value": 0–1}` 或直接一个数值（等价于 constant） |
| `color` | `"accent"`（跟随 `rippleColor`）或 `"#RRGGBB"` |
| `speed` / `trailDeg` / `fill` | 仅 `sweep`：每周期圈数（负数 = 逆时针）、拖尾角度 1–360、下方是否加淡色圆盘 |

可直接复制的起点：

* **Ring**：上面前两个图层（主色圆环 + 白色描边）——即内置 ring 的样子。
* **Waves**：三个 ring 图层，除 `"phase": 0 / 0.333 / 0.667` 外完全相同。
* **Halo**：`{"shape": "disc", "from": 0.0, "to": 0.95, "ease": "sin", "alpha":
  {"mode": "sin", "value": 0.3}}` 再加一个 `radius` 1.0 的静态细环。
* **Radar**：半径 1.0 的静态环 + 一个 `trailDeg` 120 的 `sweep` 图层。

## 运行时依赖

- Windows 10 1809+ / Windows 11，x64（构建时拒绝 32 位）。
- 以静态链接的 `findmouse.exe` 发布；不需要运行库、不需要 Python、不联网。
- 常驻插件（`mode: resident`）、`start: eager`、`idleExitSec: 0`——启动器关闭时它也
  必须活着，所以永不因空闲退出。
- 构建：`cmake -B build && cmake --build build --config Release`。exe 生成在本目录，
  也就是 `plugin.json` 里指定的位置。

## 权限说明

- **`ui`** —— 唯一的权限。用于读取 Win顺 的主题和语言、绘制置顶窗口（涟漪）、
  注册全局鼠标 hook。hook 本身不需要具名权限：Win顺 没有 `input:read` 这个概念，
  这里明确写出来而不是藏起来。
- **不申请 `elevated`** —— hook 能看到管理员窗口上的输入，取决于 UIPI 允许的程度；
  光效也可能画不到提权的全屏应用上面。这是 Windows 完整性级别的事实，不是插件 bug。
- **不申请 `network`、插件侧 `clipboard`、`files`** —— 插件不发起网络请求、不读剪贴板、
  不碰用户文件。剪贴板*效果*（`copy`）通过 action 协议请求，由宿主执行。
- **`sandbox: normal` 而非 `strict`** —— 让低完整性进程安装全局 `WH_MOUSE_LL` hook
  是未经验证的配置（低 IL 下 hook 能否收到事件没有文档保证）。插件不需要 strict
  sandbox 的保证：除了自己的数据目录它什么也不写，所以 `normal` 是如实申报，
  不是偷懒。

## 数据位置

只有一个文件：插件数据目录（由宿主在 initialize 时通过 `WS_DATA_DIR` 提供）里的
`state.json`，保存最后一次用户开关状态：

```json
{"shakeEnabled": true, "version": 1}
```

- 每次显式选择都会立即持久化：Ctrl+Enter 开关会写入，在 Win顺 设置界面改
  **启用晃动检测** 也会覆盖它。以最后一次显式操作为准；`state.json`（若存在）
  在插件启动时作为初值。
- 没有日志、缓存和密钥。删掉这个文件是安全的。

## 失败处理

- hook 安装失败（策略或资源限制）：通过 `winshun/log` 记录，结果行显示
  "hook not running"，诊断信息仍然可用。
- 浮层绘制失败：hook 继续运行，下次触发时重试。
- 设置出错：所有数值型设置都被钳制；颜色非法则保留默认主色。损坏的 `state.json`
  会被忽略（回退到默认值）。`customStyle` 解析失败时保留上一个样式（从未成功过就
  用 ring），并在诊断信息里说明——光效不会变成空白。
- 检测器不吞输入：hook 过程总是以 `CallNextHookEx` 结束，注入事件原样放行。

## 测试

- `findmouse_tests`（ctest）：纯手势状态机（快速晃动、慢速移动、亚行程抖动、漂移、
  冷却、配置切换）以及设置解析 / 钳制 / 颜色。
- `tests/integration.py`：通过 JSON-RPC stdio 驱动真实 exe ——
  initialize → query → actions → settingsChanged → shutdown，断言协议形状、
  副作用规则（query 无副作用）和状态持久化。

## 许可证

MIT，见 [LICENSE](LICENSE)。发布历史见 [CHANGELOG.md](CHANGELOG.md)。

## 诚实说明

- 物理手势的误触发率（触控板手掌扫过、游戏、快速框选文字）在日常使用之前
  **未经实测**；诊断信息里的计数器（moves / reversals / slow strokes / triggers）
  就是用来调这些阈值的。
- 光效能否显示在**独占全屏**或**管理员权限**窗口之上，取决于 Windows 完整性级别，
  不作保证（见权限说明）。
- DPI 快照在触发时取一次；跟随过程中移到不同缩放的显示器，会一直沿用旧尺寸，
  直到下次触发才更新。
- v0.2 起在**任意位置**按下左 / 右键都会结束光效（窗口是 click-through，
  所以这次点击会落到指针下面那个窗口上）。
- v0.1 有意跳过了宿主热键（`winshun/hotkeys`）：本地宿主构建尚未暴露
  `hostHotkeys` 能力协商，键盘路径因此用普通 query action 代替。

---

简体中文（默认） · [English →](README.en.md)
