# v0.4.4 发布验证

[English](release-validation-v0.4.4.en.md)

日期：2026-11-08。用户要求默认效果改为心跳并提交、打标签、正式发布。

## 构建回归

Windows x64、Visual Studio 18、Windows SDK 10.0.26100.0 构建通过。
CTest 1/1 通过（178 项断言）；独立 JSON-RPC 集成测试 77/77 通过。
新增断言确认未提供 `rippleEffect` 时得到心跳三层文档、1400 ms 周期。

## 本轮修正

manifest 默认值和 `Settings` 默认值同时改为 `heart`；同步下拉框标签、README 和发布说明。
保留显式保存的效果选择；未知 id 和无有效历史的自定义解析失败仍回退到圆环。
前一轮完成的三个预设、启动就绪、自定义回退隔离、轨道连续性修复一起发布。

## 生产界面与截图

使用实际 exe 和独立背板补拍涟漪、心跳、声呐与轨道，见 [动效截图](../screenshots.md)。
真实 `wsbench` → `WinShunPluginHost` 往返确认默认 `effect=heart`、`preset (heart)`、
`3 layers, loop=1400ms`，进程为 medium integrity，查询和诊断动作完成。

## 插件库同步

发布前检查 `LingCore/WinShun-plugins`：没有此插件条目，也没有其待合入投稿 PR。
本次为作者仓库正式发布，通过 GitHub Release 附件安装；无现有索引条目需要更新。

## 交付包 SHA256

GitHub release workflow 从标签的干净提交构建，运行 CTest，再生成 `.wsplugin` 和 `.sha256`。
发布后的下载、校验和、manifest 与真实宿主检查结果记录在工作台
`docs/validation/findmouse-release-v0.4.4/`。本文件随标签记录本地发布前验证。

## 公开发布核对

按顺序推送主分支、等待 CI 成功、推送 v0.4.4 标签、等待 release workflow 成功，
最后核对公开 Release、附件、下载哈希和标签提交。未通过时不宣告发布完成。

## 没测的

真人晃动手感与日常误触发率、混合 DPI 跨屏、独占全屏、管理员窗口行为及设置页交互未在本轮重测。
静态动效截图不能证明完整动画流畅性。未替换用户当前运行的安装版本。

[English](release-validation-v0.4.4.en.md)
