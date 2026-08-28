# CAN/UDS 车载总线分析与诊断工具

> 从0到1自研的车载CAN总线分析与UDS诊断工具，核心协议栈纯C++零Qt依赖，四层架构可移植。

[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://en.cppreference.com/)
[![Qt](https://img.shields.io/badge/Qt-6-green.svg)](https://www.qt.io/)
[![CMake](https://img.shields.io/badge/CMake-3.16+-blue.svg)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey.svg)]()
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![CI](https://github.com/codeempereor/CAN-UDS-Diagnostic-Tool/actions/workflows/ci.yml/badge.svg)](https://github.com/codeempereor/CAN-UDS-Diagnostic-Tool/actions)
[![Tests](https://img.shields.io/badge/Tests-4%20Suites%20%7C%20100%25%20Passing-brightgreen.svg)]()
[![Docs](https://img.shields.io/badge/Docs-5%20Deep%20Dive%20Documents-blue.svg)]()

---

## 功能特性

### 核心协议栈（自研，纯C++）

- **CAN 总线**：帧结构、ID过滤（白名单/黑名单/掩码）、总线统计（负载率/ID分布）
- **DBC 解析**：BO_/SG_/VAL_解析，大小端位提取，有符号/无符号/浮点，系数偏移，枚举值表
- **ISO-TP 多帧传输**（ISO 15765-2）：单帧/首帧/连续帧/流控帧完整状态机，块大小(BS)与最小间隔(STmin)，4种超时与错误检测
- **UDS 诊断**（ISO 14229-1）：会话控制(0x10)/读DID(0x22)/写DID(0x2E)/例程控制(0x31)/ECU复位(0x11)/TesterPresent(0x3E)/安全访问(0x27)，请求队列+3秒超时，安全访问4状态机

### GUI 功能（8个面板）

| 面板 | 功能 |
|------|------|
| **报文监控** | 实时报文列表、ID过滤、暂停/清空、导出CSV |
| **信号波形** | QPainter自绘多通道波形、信号显示/隐藏控制 |
| **信号数值** | 信号当前值表格、单位显示、信号名过滤、导出CSV |
| **UDS诊断** | 多标签诊断操作（会话/读DID/写DID/例程）、诊断日志 |
| **报文发送** | 手动发送、循环发送（周期可配）、发送计数 |
| **总线统计** | 6项统计卡片、负载率颜色告警、ID分布柱状图 |
| **配置面板** | 硬件配置、适配器选择、波特率、ID过滤 |
| **日志回放** | ASC格式日志加载、播放控制、倍速播放 |

### UI 布局特性

- **右侧折叠边栏**：38px垂直窄边栏，点击按钮展开/收起UDS诊断/信号数值/总线统计面板
- **右侧面板标签化**：三个面板共用Tab区域，节省空间
- **窗口自动最大化**：启动即全屏，最小尺寸1280×720
- **面板最小尺寸保护**：每个面板设置最小宽高，防止内容被压缩
- **视图菜单**：所有面板可通过菜单显示/隐藏
- **双缓冲刷新**：报文显示50ms定时器批量刷新，高负载不卡顿

### 工程化

- CMake 分层构建，Qt5/6 自动检测
- 4个单元测试套件，100%通过
- GitHub Actions CI（Ubuntu/Windows 双平台自动构建测试）
- 代码覆盖率支持（gcov）
- .clang-format 代码风格规范
- CHANGELOG 版本变更记录
- 5份深度技术文档（逐函数剖析）

---

## 架构设计

### 四层架构（高内聚低耦合）

```
┌─────────────────────────────────────────────────────────────────┐
│                        GUI 视图层 (Qt Widgets)                    │
│  配置 | 报文监控 | 信号波形 | 信号数值 | UDS诊断 | 报文发送      │
│  总线统计 | 日志回放  (8个功能面板 + MainWindow中介者)            │
└───────────────────────────┬─────────────────────────────────────┘
                            │ 信号槽 (Qt Signal/Slot)
┌───────────────────────────▼─────────────────────────────────────┐
│                     业务编排层 (Qt Core)                          │
│  CanManager | DbcManager | DiagManager | SignalManager | LogManager │
│  双缓冲批量处理 / 外观模式 / 观察者链 / 保活定时器 / 超时检查     │
└───────────────────────────┬─────────────────────────────────────┘
                            │ 纯虚接口 + std::function回调
┌───────────────────────────▼─────────────────────────────────────┐
│              核心协议栈 (纯C++17, 零Qt依赖)                      │
│  CAN帧/过滤/统计 | DBC解析/位提取/值表 | ISO-TP多帧+超时 | UDS诊断+状态机 │
│  可独立编译 / 可单元测试 / 可移植到嵌入式平台                      │
└───────────────────────────┬─────────────────────────────────────┘
                            │ CanAdapterBase 抽象基类
┌───────────────────────────▼─────────────────────────────────────┐
│                     硬件抽象层 (HAL)                              │
│  VirtualCanAdapter (模拟ECU回环) | SocketCanAdapter (Linux)     │
│  可扩展: PCAN / ZLG / USB-CAN / Vector                          │
└─────────────────────────────────────────────────────────────────┘
```

### 设计原则

- **核心协议栈与Qt完全解耦**：纯C++实现，可独立移植到STM32+FreeRTOS等嵌入式平台
- **面向抽象编程**：硬件适配器、各模块均基于抽象接口，可插拔替换
- **单向依赖**：自上而下依赖，下层不感知上层存在
- **独立可测试**：每个模块均可脱离其他模块单独单元测试

---

## 核心技术亮点

1. **自研ISO-TP多帧传输协议栈**：实现单帧/首帧/连续帧/流控帧完整状态机，解决回调重入死锁问题（mutex→recursive_mutex），添加4种超时检测（流控超时/连续帧超时/序列号错误/溢出）

2. **自研UDS诊断客户端**：实现6种核心诊断服务，添加请求队列与3秒超时机制（半双工保护），安全访问4状态机（Idle→WaitingForSeed→WaitingForKey→Unlocked），密钥计算通过策略模式注入

3. **DBC解析器**：实现大小端位提取，**修复大端跨字节位提取越界bug**（原算法byteIdx跨字节时变负数下溢到255，重写为位号递减+跨字节跳转算法），支持VAL_枚举值表解析，7个边界测试用例验证

4. **8种设计模式实战**：观察者（回调+信号槽）、策略（密钥计算）、状态（ISO-TP/UDS/安全访问）、适配器（硬件可插拔）、模板方法、外观（Manager封装）、中介者（MainWindow连接各面板）、RAII（锁/文件句柄）

5. **多线程与性能优化**：接收线程与GUI线程分离，报文显示双缓冲+50ms定时器批量刷新避免卡顿，信号数据deque环形缓存

6. **工程化规范**：CMake分层构建，4个单元测试100%通过，GitHub Actions CI双平台自动构建，代码覆盖率支持，.clang-format代码风格

---

## 目录结构

```
CAN_UDS_Tool/
├── src/
│   ├── core/               # 核心协议栈（纯C++，无Qt依赖）
│   │   ├── can/            # CAN基础（帧/过滤/统计）
│   │   ├── dbc/            # DBC解析（信号/报文/解析器）
│   │   ├── isotp/          # ISO-TP多帧传输
│   │   └── uds/            # UDS诊断协议栈
│   ├── hal/                # 硬件抽象层
│   │   ├── can_adapter_base.h/cpp
│   │   ├── virtual_can_adapter.h/cpp   # 虚拟适配器（模拟ECU）
│   │   └── socket_can_adapter.h/cpp     # Linux SocketCAN
│   ├── business/           # 业务编排层（Qt Core）
│   │   ├── can_manager.h/cpp
│   │   ├── dbc_manager.h/cpp
│   │   ├── diag_manager.h/cpp
│   │   ├── log_manager.h/cpp
│   │   └── signal_manager.h/cpp
│   └── gui/                # GUI视图层（Qt Widgets）
│       ├── main.cpp
│       ├── main_window.h/cpp
│       └── panels/         # 8个功能面板
│           ├── trace_panel        # 报文监控
│           ├── signal_panel       # 信号波形
│           ├── signal_value_panel # 信号数值
│           ├── diag_panel         # UDS诊断
│           ├── send_panel         # 报文发送
│           ├── stats_panel        # 总线统计
│           ├── config_panel       # 配置
│           └── replay_panel       # 日志回放
├── tests/                  # 单元测试（4个套件）
│   ├── test_can_filter.cpp
│   ├── test_isotp.cpp
│   ├── test_uds.cpp
│   └── test_dbc.cpp
├── docs/                   # 技术文档（5份深度剖析）
├── examples/               # 示例DBC文件
├── scripts/                # 工具脚本（覆盖率生成）
├── resources/              # Qt资源文件
├── .github/workflows/      # GitHub Actions CI
├── .clang-format           # 代码风格配置
├── CHANGELOG.md            # 版本变更记录
├── CMakeLists.txt
└── README.md
```

---

## 编译构建

### 依赖要求

- CMake >= 3.16
- C++17 编译器（GCC / Clang / MSVC / MinGW）
- Qt5 或 Qt6（Core + Gui + Widgets 模块）

### Windows 编译（MinGW）

```bash
mkdir build
cd build
cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/mingw_64" ..
mingw32-make -j4
```

### Linux 编译

```bash
sudo apt install qt6-base-dev cmake build-essential
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 使用 Qt Creator

直接用 Qt Creator 打开 `CMakeLists.txt`，配置 Qt Kit 后构建即可。

---

## 运行测试

```bash
cd build
ctest --output-on-failure
```

4个测试套件：
- `test_can_filter` — CAN过滤器（4项）
- `test_isotp` — ISO-TP多帧传输（3项）
- `test_uds` — UDS诊断请求/响应解析（4项）
- `test_dbc` — DBC解析+大端位提取（7项边界测试）

---

## 快速开始

1. **启动程序**：运行 `build/bin/can_uds_tool.exe`（程序自动最大化）
2. **选择适配器**：左侧配置面板选择「虚拟适配器（模拟）」
3. **启动**：点击「启动」按钮，开始接收模拟报文（0x100、0x200）
4. **加载DBC**：工具栏点击「加载DBC文件」，选择 `examples/sample.dbc`
5. **查看波形**：底部信号波形面板开始实时绘制3个信号
6. **尝试诊断**：切换到UDS诊断面板，发送诊断指令

### 虚拟适配器模式

内置虚拟CAN适配器和模拟ECU，无需真实硬件即可体验全部功能：
- 模拟周期报文：0x100、0x200
- 模拟ECU诊断响应：支持 0x10/0x22/0x2E/0x31/0x3E/0x11/0x27 服务
- 测试DID：0xF190 (VIN)、0xF18E (硬件版本)、0xF18A (软件版本)

### 真实硬件（Linux SocketCAN）

```bash
# 虚拟CAN接口
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# 真实CAN适配器
sudo ip link set can0 type can bitrate 500000
sudo ip link set up can0
```

---

## 代码覆盖率

```bash
# 启用覆盖率编译
cmake -B build_cov -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build_cov
cd build_cov && ctest

# 生成报告（Windows PowerShell）
../scripts/coverage.ps1
```

---

## 技术文档

项目包含5份深度技术文档，位于 `docs/` 目录：

| 文档 | 内容 |
|------|------|
| `01_设计理念与架构总览.md` | 四大设计理念、四层架构、设计模式清单、目录结构 |
| `02_核心协议栈_CAN与DBC.md` | CAN帧/过滤/统计、DBC信号/报文/位提取（逐函数剖析） |
| `03_核心协议栈_ISO-TP与UDS.md` | ISO-TP四种帧/状态机/超时、UDS服务/请求队列/安全访问 |
| `04_硬件抽象层与业务编排层.md` | 适配器基类/虚拟适配器、5个Manager、双缓冲/观察者链 |
| `05_GUI层与问题总结.md` | 8个面板剖析、QPainter自绘、8个踩坑问题、知识点全景 |

---

## 作者

**三道渊** (codeempereor)

- GitHub: [@codeempereor](https://github.com/codeempereor)
- 物联网工程专业，嵌入式开发方向
- 技术栈：C/C++、Qt、嵌入式、汽车电子协议

---

## 许可证

[MIT License](LICENSE)
