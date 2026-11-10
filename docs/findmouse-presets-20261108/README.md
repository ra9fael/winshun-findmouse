# findmouse 预设收尾验证

[English](README.en.md)

对应实现：`ra9fael.findmouse` 工作区版本 0.4.4，日期 2026-11-08。

## 问题是什么

继续完成 findmouse 未提交的 Ping、Heart、Orbit 预设和阈值改动。代码检查还发现浮层线程启动时可能丢失初始样式消息、轨道负速扫描跨周期跳变，以及自定义 JSON 失败时可能沿用预设文档。

## 为什么

浮层 `start()` 原先在线程创建后立即返回，窗口消息队列尚未建立；随后 `updateStyle()` 的 `PostThreadMessage` 可能失败。Generic renderer 只拿到取模后的脉冲进度，负速轨道在每个周期末回到角度起点。预设和用户自定义文档共用一个指针时，解析失败无法区分两种回退来源。

## 改了什么

- 新增 Ping、Heart、Orbit 的注册表文档，manifest、运行时枚举和下拉选项保持一致。
- 浮层启动等待窗口和消息队列就绪；样式消息投递失败及退出清理未处理消息。
- 预设文档与上一份有效用户自定义文档分开保存；清空 JSON 会清除自定义回退。
- 轨道使用累计周期计算角度，修正负速尾迹方向；注册表查询接受大小写混合 id。
- 增加 manifest/L1、JSON-RPC/L2、真实宿主/L3 回归和双语设计文档。

## 实测

环境：Windows x64，Visual Studio 18.11.1，Windows SDK 10.0.26100.0；测试使用独立临时数据目录，未改动用户安装目录。

命令和证据：

- `cmake --build _msvc/findmouse --config Release`：通过，输出见 [build.txt](build.txt)。
- `ctest --test-dir _msvc/findmouse -C Release --output-on-failure`：1/1 通过；直接运行带 manifest 参数为 **178 passed, 0 failed**，见 [ctest.txt](ctest.txt)。
- `python tests/integration.py`：**76 passed, 0 failed**；覆盖协议、查询无副作用、窗口跟随、淡出、预设初始化、动态切换和自定义回退，见 [integration.txt](integration.txt)。
- `wsbench --plugin ... --setting rippleEffect=ping/heart/orbit --action copyDiagnostics`：真实 `WinShunPluginHost.exe` 加载成功；诊断分别确认 3 layers、1100/1400/1600 ms，见 [host-ping.txt](host-ping.txt)、[host-heart.txt](host-heart.txt)、[host-orbit.txt](host-orbit.txt)。
- 同一宿主分别执行三个预设的 `showNow`，均返回 `hide` 且正常完成；见 [host-show-ping.txt](host-show-ping.txt)、[host-show-heart.txt](host-show-heart.txt)、[host-show-orbit.txt](host-show-orbit.txt)。窗口可见性和超时的自动断言由 L2 测试负责。
- 本地开发包 22 个文件，校验 **0 错误、1 条未审核原生包提示**；必需源文件齐全，包内 exe 与验证过的 exe 完全一致，见 [package.json](package.json)。从解压后的包再次通过真实宿主加载 Orbit，见 [host-package.txt](host-package.txt)。
- `_msvc/validate_docs.py`：README 链接、锚点与语言互链全部通过。

包位于 `ra9fael.findmouse/dist/presets-20261108/ra9fael.findmouse-0.4.4.wsplugin`。
SHA256：`fe036f7227e4739960ad7ed8d1c596923dcee4c52dfc9dd831e8ce905b4ad6b9`。
这是工作区开发快照；通过临时 Git index 把尚未提交的新源文件纳入原有打包脚本，没有改变真实暂存区。

验证过程中修正了一次重命名残留导致的编译错误；初版启动测试还错误地把 SDK 的 initialize 回复当成工作线程就绪，改用紧随其后的诊断请求作为同步点后全部通过。未通过的中间运行没有用于交付。

## 没测的

真人晃动手感和误触发率、多显示器跨 DPI、独占全屏、管理员窗口上的可见性，以及实际设置页面下拉框截图未在本轮自动化验证。`wsbench` 验证了真实宿主往返，但不代替完整 Win顺 设置页验收。

## 收尾

复用既有 `_msvc/findmouse` 构建目录；集成测试、宿主临时配置、打包临时索引和解压目录均已清理。
保留本地开发包和上述精选验证证据。没有替换运行中的用户插件、没有发布版本；源码修改保留未提交。

[English](README.en.md)
