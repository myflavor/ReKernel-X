# ReKernel-X

[![C](https://img.shields.io/badge/language-C-%23f34b7d.svg?style=plastic)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Android](https://img.shields.io/badge/platform-Android-0078d7.svg?style=plastic)](https://en.wikipedia.org/wiki/Android_(operating_system))
[![AArch64](https://img.shields.io/badge/arch-AArch64-red.svg?style=plastic)](https://en.wikipedia.org/wiki/AArch64)

为墓碑模块提供内核支持

基于 [Re:Kernel](https://github.com/Sakion-Team/Re-Kernel) v9.2 二次开发

## 作用

进程被冻结后，无法正常响应，ReKernel-X 在内核监听并提供如下事件

| 类型 | 触发条件 | 适用场景 |
|---|---|---|
| Binder | 冻结进程收到 Binder 调用 | 系统服务或其他应用访问冻结进程 |
| Signal | 冻结进程收到 SIGKILL 等关键信号 | 感知杀进程等行为 |
| Network | 被监控 UID 收到入站网络包 | 消息类应用接收推送 |

## 依赖环境

- 内核 ≥ 5.10
- [KernelSU](https://kernelsu.org/zh_CN/guide/installation.html)
  或 [Magisk](https://topjohnwu.github.io/Magisk/install.html)
  或 [APatch](https://github.com/bmax121/APatch)

## 安装

从 [GitHub Releases](https://github.com/myflavor/ReKernel-X/releases) 下载模块并刷入，重启后自动加载

## 开发接入

用户态通过 Generic Netlink 接收内核事件，仓库提供现成的 AAR 客户端

- 接入方式见 [Develop](Develop)
- 内核源码与编译说明见 [LKM-Source](LKM-Source)

## 许可

本项目使用 [GPL](LICENSE)，修改或二次分发请保持开源
