# findmouse v0.4.4 发布完成记录

[English](README.en.md)

## 问题是什么

用户要求将默认效果改为心跳，提交代码、打标签并正式发布。发布前远端最新版本为 v0.4.3。

## 为什么

之前 manifest 与运行时默认都为 ring。仅改下拉框默认值不能覆盖直接初始化时缺少设置的路径，
因此必须同时修改 `Settings::effect`、manifest 标签与默认值，并验证未传 `rippleEffect` 的实际初始化。

## 改了什么

默认效果设为 `heart`（三层、1400 ms），保留旧用户显式保存的选择。
发布上一轮完成的 Ping/Heart/Orbit 预设、启动消息就绪、独立自定义回退、连续反向扫描等修复。
更新中英文 README、设计文档、变更记录、发布说明；补拍四种原生效果截图。

## 实测

- 本地 MSVC x64 构建通过，CTest 1/1、178 项断言通过：[build.txt](build.txt)、[ctest.txt](ctest.txt)、[unit.txt](unit.txt)。
- 本地集成测试 77/77，通过无显式效果设置时的默认心跳检查：[integration.txt](integration.txt)。
- 真实 `wsbench` / `WinShunPluginHost` 确认 `effect=heart`、`preset (heart)`、`3 layers, loop=1400ms`：[host-default.txt](host-default.txt)。
- 原生截图来自实际 exe 和独立纯色背板：[captures.json](captures.json)、[capture_effects.py](capture_effects.py)。
  截图第一次尝试受光标移动影响而失败，最终改为读取真实浮层位置并裁取背板内区域；未伪造渲染。
- 功能提交 `2b95d8d`、发布文档提交 `c137e32`，整合主分支为 **43dd1be80c08bf9edc305ac7b6baba914d8adc98**。
- 主分支 CI **42477493842** 成功后才创建并推送 v0.4.4 标签：[ci.json](ci.json)。
- Release workflow **42477583397** 成功：[release-run.json](release-run.json)。
- 正式发布于 **2026-11-08 19:52:39（香港时间）**，非草稿、非预发布、最新正式版：[release.json](release.json)。
- 从线上下载实际发布包重新验证：31 个文件、0 错误、1 条预期的未审核原生包提示；
  SHA256 文件与 GitHub asset digest 均匹配，manifest 默认值为 heart。
  解压后的线上 exe 通过真实宿主和完整 **77/77** 集成断言：
  [published-verification.json](published-verification.json)、[published-host.txt](published-host.txt)、[published-integration.txt](published-integration.txt)。

公开版本：<https://github.com/ra9fael/winshun-findmouse/releases/tag/v0.4.4>

正式包：`ra9fael.findmouse-0.4.4.wsplugin`，769608 字节。
SHA256：`6cdae7c324a985e89fd4c99756566817020b30144c741b75f16cda130c0414a9`。
下载副本位于 `ra9fael.findmouse/dist/release-v0.4.4-public/`。

远端 main 与标签解引用均为 `43dd1be80c08bf9edc305ac7b6baba914d8adc98`，插件工作树干净。
没有官方插件库条目或该插件的待合入 PR，无已有索引需要更新。
未替换用户运行中的安装版本；独立测试进程、临时索引/配置/解压目录已回收，
保留正式包副本、本地构建包及本目录验证原件。本目录为工作台本地记录，不推送到宿主仓库。

## 没测的

真人晃动手感与日常误触发率、混合 DPI 跨屏、独占全屏、提权窗口行为和 Win顺 设置页交互未重测。
静态截图不能代替完整动画体验；已保存为 ring 的用户不会被强制改为 heart。

[English](README.en.md)
