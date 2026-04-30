# DuckyClaw-EPD

<div align="center">

**[English](./README.md) | [中文]**

![GitHub Repo Banner](https://images.tuyacn.com/fe-static/docs/img/210f532a-0bb1-4ca5-9037-f5488958a709.jpg)

**Your autonomous AI companion on E-Paper Display hardware.**  
**你的自主 AI 伴侣，专为电子墨水屏硬件打造。**

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://www.apache.org/licenses/LICENSE-2.0)
[![TuyaOpen](https://img.shields.io/badge/TuyaOpen%20Repo-Visit-blue?logo=github)](https://github.com/tuya/TuyaOpen/)
[![Platform](https://img.shields.io/badge/Platform-T5AI%20%7C%20ESP32%20%7C%20RPi%20%7C%20Linux-green)]()

> [!WARNING]
> **🚧 Under Active Development** — This project is in heavy development and things may break. Please open an [Issue](https://github.com/tuya/DuckyClaw/issues) if you encounter any problems.

</div>

---

<a name="english"></a>



### DuckyClaw-EPD 是什么？

DuckyClaw-EPD 是 [DuckyClaw](https://github.com/tuya/DuckyClaw) 项目的**电子墨水屏（EPD / E-Ink）专属版本** — 一个基于 [TuyaOpen C SDK](https://github.com/tuya/TuyaOpen) 构建的、面向硬件的自主 AI Agent 平台。它专为搭载电子墨水屏的设备设计（如 Tuya T5AI E-Ink + NFC 开发板），同时支持从 MCU 到 Linux SoC 的广泛硬件部署。

DuckyClaw-EPD 的核心是在设备本地运行一个 **Claw 风格的自主 Agent 循环**。用户通过熟悉的 IM 频道（Telegram、Discord、飞书）或设备上的显示屏与按键与 Agent 交互。Agent 可以自主调用 MCP 工具（文件读写、定时任务、IoT 设备控制、远程命令执行），维护长期记忆，并驱动丰富的 EPD UI —— 无需用户自己搭建云服务。

---

### 支持的硬件

| 类别 | 型号 / 平台 |
|------|------------|
| **MCU** | Tuya T5AI 模组（E-Ink + NFC）、ESP32-S3 |
| **SoC** | Raspberry Pi 4/5/CM4/CM5、Linux ARM SoC（高通 / 瑞芯微 / 全志等） |
| **PC** | Ubuntu Linux（x86-64） |

**默认目标板：** `TUYA_T5AI_EINK_NFC` — Tuya T5AI + 电子墨水屏 + NFC。

---

### 为什么选择 DuckyClaw-EPD？

大多数 AI Agent 框架虽然强大，但往往过于沉重：需要云服务器、订阅费用、复杂的部署流程。DuckyClaw-EPD 提供了一条不同的路：

- **一个 TuyaOpen Key** — 一把钥匙解锁涂鸦云平台、设备–云 AI Agent，以及 GPT / Claude / DeepSeek / 通义千问等主流模型。
- **运行在十几元的 MCU 上** — 同一套代码运行在微控制器、树莓派或桌面 PC 上。
- **原生 E-Ink UI** — 专为低功耗电子墨水屏打造的多屏 UI，包含首页、通知、阅读器、文件浏览器、设置、定时任务、待机等界面。
- **无 Node.js，无 Python 框架** — 纯 C + TuyaOpen，极低资源占用。
- **对话式配置** — 通过聊天修改 Agent 行为，无需编辑配置文件。

| | DuckyClaw-EPD | OpenClaw / 其他 |
|---|---|---|
| **运行时** | TuyaOpen C — ARM Cortex-M/A + x64 | Node.js 22+，需完整 OS |
| **部署目标** | MCU、SoC、PC — 一套代码 | 仅服务器 / 桌面 |
| **显示** | 原生 E-Ink 多屏 UI | 终端或独立 UI |
| **设备–云** | 一个 Key → 涂鸦云 + 本地 AI | 自托管，需单独 API 订阅 |
| **成本** | 低 — 统一 Key 接入 | Claude Pro / OpenAI API（¥140–1400/月） |
| **IoT 控制** | 通过 MCP 工具原生控制涂鸦设备 | 通常不支持 |
| **语音 ASR** | 部分开发板支持硬件语音识别 | 原生不支持 |

---

### 架构

```
┌──────────────────────────────────────────────────────┐
│                    IM 频道层                           │
│  Telegram │ Discord │ 飞书 │ WebSocket │ 串口 CLI     │
└──────────────────────┬───────────────────────────────┘
                       │  im_msg_t
               ┌───────▼───────┐
               │   消息总线    │  线程安全双队列
               └───────┬───────┘
                       │
               ┌───────▼───────┐
               │   Agent 循环  │  外层：等待消息
               │               │  内层：≤10 次工具迭代
               └───┬───────┬───┘
                   │       │
         ┌─────────▼─┐  ┌──▼──────────┐
         │  上下文   │  │  云端 AI    │
         │  构建器   │  │ (ai_agent)  │
         └─────┬─────┘  └──────┬──────┘
               │               │
     ┌─────────▼───────────────▼──────────┐
     │             MCP 工具层              │
     │  文件 │ 定时任务 │ 执行 │ IoT 控制  │
     └─────────┬───────────────┬──────────┘
               │               │
     ┌─────────▼────┐  ┌───────▼──────┐
     │   记忆 /     │  │  EPD UI /    │
     │   会话持久化  │  │   网关       │
     └──────────────┘  └──────────────┘
```

**数据流：**
1. IM 频道（或定时任务 / ACP 事件）构建一条 `im_msg_t` → 推入**消息总线**。
2. **Agent 循环**线程唤醒，组装系统提示词（记忆、技能、人格、工具），调用云端 AI。
3. AI 可能调用 **MCP 工具**（最多 10 次迭代）后输出最终回复。
4. 回复通过原始 IM 频道发回，并在 **EPD 显示屏**上渲染。

---

### 功能特性

- **电子墨水屏 UI** — 多屏界面（首页、通知、阅读器、文件浏览器、设置、定时任务、待机），内置字体和 Emoji 资源，专为 E-Ink 面板优化。
- **统一 IM 输入** — Telegram、Discord、飞书、WebSocket、串口 CLI 通过线程安全消息总线统一汇入同一个 Agent 循环。
- **设备–云混合 AI Agent** — 本地 Agent 循环 + 涂鸦云 AI，设备与云端智能无缝切换。
- **MCP 设备工具：**
  - `FILE` — 读 / 写 / 编辑 / 列出 / 查找 Flash 或 SD 卡上的文件。
  - `CRON` — 调度周期性或一次性任务，持久化到 `cron.json`。
  - `IoT 控制` — 管理涂鸦生态中的智能设备。
  - `EXEC` — 远程 Shell 执行（仅 Linux / Raspberry Pi）。
- **持久化记忆** — `MEMORY.md`、每日笔记（`YYYY-MM-DD.md`）、人格配置（`SOUL.md`）、用户画像（`USER.md`）存储于设备本地。
- **技能 / 插件系统** — 在 `skills/` 目录下放一个 `.md` 文件即可为 Agent 添加新能力，无需重新编译。
- **心跳服务** — 由 `HEARTBEAT.md` 驱动的周期性 AI 心跳。
- **定时服务** — 内存任务表 + `cron.json` 持久化的后台调度器。
- **硬件语音 ASR** — 支持语音输入和语音识别（部分开发板）。
- **NFC 支持** — T5AI E-Ink NFC 开发板上的 NFC 硬件集成。
- **OTA 升级** — 通过涂鸦云进行固件空中升级。
- **SD 卡存储** — Flash ↔ SD 卡无缝切换抽象（`claw_*` 文件系统宏）。

---

### 快速开始

#### 克隆项目

```shell
git clone https://github.com/tuya/DuckyClaw.git DuckyClaw-EPD
cd DuckyClaw-EPD
git submodule update --init
```

#### 配置凭证

复制 Secrets 模板并填写您的密钥：

```shell
cp include/tuya_app_config_secrets.h.example include/tuya_app_config_secrets.h
# 编辑：TUYA_OPENSDK_UUID、TUYA_OPENSDK_AUTHKEY、IM Token 等
```

#### 选择开发板配置并编译

```shell
# T5AI E-Ink NFC（默认）
tos.py config choice config/TUYA_T5AI_EINK_NFC.config
tos.py build

# Raspberry Pi
tos.py config choice config/RaspberryPi.config
tos.py build

# ESP32-S3
tos.py config choice config/ESP32S3_BREAD_COMPACT_WIFI.config
tos.py build
```

#### 平台快速入门文档

- [T5AI 快速开始](https://tuyaopen.ai/docs/duckyclaw/ducky-quick-start-T5AI)
- [Raspberry Pi 5 快速开始](https://tuyaopen.ai/docs/duckyclaw/ducky-quick-start-raspberry-pi-5)
- [ESP32-S3 快速开始](https://tuyaopen.ai/docs/duckyclaw/ducky-quick-start-ESP32S3)

---

### 技能 / 插件开发

技能是放在 `skills/` 目录下的 `.md` 文件。Agent 会读取所有已安装技能的摘要并自动遵循其指令。

```markdown
# 技能标题

一句话描述这个技能的作用。

## 使用时机
当用户询问 X 时，或当 Y 条件满足时。

## 使用方法
1. 以参数 ... 调用 tool_A
2. 然后以参数 ... 调用 tool_B
3. 以结果作为回复。

## 示例
用户："10 分钟后提醒我" → 调用 get_current_time，然后 cron_add，然后回复"提醒已设置。"
```

**添加技能：** 将新的 `name.md` 放入 `skills/` 目录（例如通过运行时的 `write_file` MCP 工具，或直接编辑源码）。内置技能在 `skills/skill_loader.c` 中注册。

---

### 项目结构

```
DuckyClaw-EPD/
├── agent/                  # 核心 Agent 循环与上下文构建
│   ├── agent_loop.c/h      #   外层 + 内层工具迭代循环（信号量同步）
│   └── context_builder.c/h #   系统提示词组装（规则、记忆、技能、人格）
├── IM/                     # 统一即时通讯抽象层
│   ├── bus/                #   线程安全的入站/出站消息总线
│   ├── channels/           #   Telegram / Discord / 飞书 / 微信 Bot
│   ├── cli/                #   本地串口 / CLI 输入频道
│   ├── proxy/              #   TLS HTTP 代理客户端
│   └── certs/              #   TLS CA 证书包
├── tools/                  # MCP 工具实现
│   ├── tool_files.c/h      #   文件操作
│   ├── tool_cron.c/h       #   定时任务调度
│   ├── tool_exec.c/h       #   Shell 执行（仅 Linux）
│   └── tool_hw.c/h         #   硬件外设工具
├── memory/                 # 持久化记忆与会话管理
│   ├── memory_manager.c/h  #   MEMORY.md、每日笔记、SOUL.md、USER.md
│   └── session_manager.c/h #   会话 JSONL 持久化
├── display/                # 电子墨水屏显示层
│   ├── display.c/h         #   主显示模块
│   ├── display_i18n.c/h    #   国际化（中 / 英）
│   └── screens/            #   首页/通知/阅读器/文件浏览/设置/定时/待机
├── cron_service/           # 后台定时调度服务
├── heartbeat/              # 心跳服务（由 HEARTBEAT.md 驱动）
├── skills/                 # 技能加载器与内置技能 .md 文件
├── gateway/                # WebSocket 服务端 + OpenClaw ACP 客户端
├── src/                    # 应用入口与业务逻辑胶水层
│   ├── tuya_app_main.c     #   入口点、SDK 初始化、事件循环
│   ├── ducky_claw_chat.c   #   AI 流式事件处理、信号量桥接
│   └── app_im.c            #   IM ↔ Agent 桥接
├── ai_components/          # TuyaOpen AI 组件适配器
├── config/                 # 各开发板 Kconfig 快照
│   ├── TUYA_T5AI_EINK_NFC.config         # 默认：T5AI E-Ink NFC
│   ├── RaspberryPi.config
│   ├── ESP32S3_BREAD_COMPACT_WIFI.config
│   └── ...
├── include/                # 全局头文件与凭证配置
├── CMakeLists.txt
├── Kconfig
├── app_default.config      # 当前激活的开发板配置
└── TuyaOpen/               # TuyaOpen C SDK（git 子模块）
```

---

### 问题反馈与贡献

请通过[新建 Issue](https://github.com/tuya/DuckyClaw/issues) 来报告问题，提交前请先确认是否已有相同问题。欢迎贡献代码和提交 Pull Request！

### 开源协议

本项目基于 [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0) 开源。

### 致谢

- [TuyaOpen](https://github.com/tuya/TuyaOpen/) — 本项目基于此硬件 AIoT OS 构建。
- [OpenClaw](https://github.com/openclaw/openclaw) — 原始 Agent 架构灵感来源。
- [MimiClaw](https://github.com/memovai/mimiclaw) — ESP32 本地 Agent 与技能系统灵感来源。

### 作者

由 [TuyaOpen Team](https://tuyaopen.ai/) 创建，感谢所有[贡献者](https://github.com/tuya/DuckyClaw/graphs/contributors)的支持。

[![contributors](https://contrib.rocks/image?repo=tuya/DuckyClaw)](https://github.com/tuya/duckyclaw/graphs/contributors)

---

<div align="center">

如果这个项目对你有帮助，请给个 Star ⭐  
If you find this project useful, please leave a star ⭐

[![TuyaOpen](https://img.shields.io/badge/TuyaOpen%20Repo-Visit-blue?logo=github)](https://github.com/tuya/TuyaOpen/)

</div>
