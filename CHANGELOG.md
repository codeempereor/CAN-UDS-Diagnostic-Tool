# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

#### 第一优先级：核心质量
- ISO-TP超时机制：流控超时、连续帧超时、序列号错误、溢出检测，4种错误类型回调
- UDS请求队列与超时：半双工保护（拒绝并发请求）、请求-响应serviceId匹配、3秒请求超时
- DBC VAL_值表解析：解析`VAL_`行，信号新增`valueDescriptions`映射表、`getValueDescription()`、`hasValueTable()`接口
- DBC大端位提取测试：新增7个测试用例（8/4/16/12/24位、全0、全1），覆盖跨字节、非对齐起始位边界场景

#### 第二优先级：功能完整
- 报文发送面板（SendPanel）：CAN ID输入、扩展帧、十六进制数据、单次发送、循环发送（1~60000ms周期）、发送计数
- TesterPresent自动保活：非默认会话且无pending请求时自动发送保活帧（suppressPositiveResponse=true），默认3秒周期可配置
- UDS安全访问状态机（0x27服务）：4种状态（Idle→WaitingForSeed→WaitingForKeyResponse→Unlocked），`startSecurityAccess()`一键完成请求种子→计算密钥→发送密钥流程，密钥计算函数由上层注入
- 信号数值面板（SignalValuePanel）：表格实时显示所有DBC信号当前值、单位、原始值、描述，信号名过滤搜索

#### 第三优先级：工程化
- .clang-format 代码风格配置：基于LLVM，4空格缩进，120列宽，指针靠左，include排序，C++17标准
- GitHub Actions CI：Ubuntu/Windows双平台矩阵，Qt6自动安装，CMake构建，ctest运行测试，main分支自动上传exe产物（保留30天）
- 代码覆盖率支持：`ENABLE_COVERAGE` CMake选项（gcov插桩），`scripts/coverage.ps1`一键生成覆盖率报告脚本
- CHANGELOG.md：Keep a Changelog格式，语义化版本，完整版本变更记录

#### 第四优先级：锦上添花
- 报文导出CSV（TracePanel）：导出序号、时间戳、CAN ID、扩展帧、DLC、数据，UTF-8编码
- 信号数据导出CSV（SignalValuePanel）：导出信号名、当前值、单位、原始值、描述，仅导出可见信号
- 统计面板（StatsPanel）：6个统计卡片（总帧数、总字节、负载率颜色告警、唯一ID数、错误帧、远程帧）+ ID分布表格（帧数、占比、柱状图），按帧数降序实时更新
- 波形面板信号显示/隐藏控制：左侧信号列表复选框勾选状态真正生效，调用`setSignalVisible()`控制波形绘制

### Fixed
- DBC大端(Motorola)位提取bug：跨字节时`byteIdx = startByte - (bitPos/8)`计算错误导致越界读取，重写为位号递减+跨字节跳转到bit7算法，7个测试用例验证通过
- ISO-TP回调重入死锁：sendData加锁→回调→对方回流控→回调→自己handleReceivedFrame又要加锁，mutex改为recursive_mutex
- DbcMessage成员名signals与Qt的signals宏冲突：全局改名为signalList
- Qt6 QRegExp已移除：改用QRegularExpression
- QColor头文件找不到：CMakeLists添加QtGui组件链接
- QMap operator[]在const上下文返回临时值不能取地址：改用constFind()+value()
- QTableWidget无removeRows方法：循环调用removeRow(0)
- std::map迭代器无key()/value()方法：改用first/second
- HAL层include路径配置错误：CMake修正为src目录

### Changed
- UdsClient新增PendingRequest结构，sendRequest在有pending请求时返回false拒绝并发
- UdsClient新增SecurityAccessState状态机、KeyCalculator密钥计算回调、SecurityAccessCallback完成回调
- IsoTpClient新增ErrorCallback错误回调、checkTimeout()超时检查、IsoTpErrorType 4种错误类型
- DiagManager新增100ms超时检查定时器、TesterPresent保活定时器、udsErrorOccurred信号
- SignalData新增unit字段，SignalManager新增setSignalUnit()接口
- DbcSignal新增valueDescriptions映射表、getValueDescription()、hasValueTable()接口
- DbcMessage新增getSignalValueDescription()便捷查询方法
- TracePanel新增exportToCsv()方法和导出按钮
- SignalValuePanel新增exportToCsv()方法和导出按钮
- SignalPanel信号列表复选框连接itemChanged信号，真正控制波形显示/隐藏

## [1.0.0] - 2026-08-16

### Added
- 四层架构：核心协议栈（零Qt依赖）→ 硬件抽象层 → 业务编排层 → GUI视图层
- CAN协议：帧结构、过滤器（白名单/黑名单/掩码）、总线统计
- DBC解析：BO_报文、SG_信号（大小端、有符号/无符号、系数偏移、最小最大、单位）
- ISO-TP多帧传输：单帧、首帧、连续帧、流控帧，支持块大小和STmin
- UDS诊断：0x10会话控制、0x22读DID、0x2E写DID、0x31例程控制、0x3E保活、0x11复位、0x27安全访问基础
- 硬件抽象：VirtualCanAdapter（模拟ECU回环）、SocketCanAdapter（Linux）
- 业务编排：CanManager（双缓冲批量处理）、DbcManager（外观模式）、DiagManager、SignalManager（deque缓存）、LogManager（ASC格式）
- GUI：主窗口（中介者模式）、TracePanel（报文监控）、SignalPanel（QPainter波形）、DiagPanel（多标签诊断）、ConfigPanel、ReplayPanel（日志回放）
- 单元测试：test_can_filter、test_isotp、test_uds、test_dbc
- CMake构建系统：Qt5/6自动检测、分层编译、自动MOC/RCC/UIC
- 示例DBC文件：sample.dbc（3报文9信号）
- 完整技术文档：5份深度剖析文档（设计理念、CAN/DBC、ISO-TP/UDS、HAL/业务层、GUI/问题总结）

### Architecture
- 核心协议栈纯C++实现，零Qt依赖，可独立移植和测试
- 设计模式：观察者、策略、状态、适配器、模板方法、外观、中介者、RAII
- 线程安全：mutex/atomic/双缓冲，接收线程与GUI线程分离
- 跨平台：Windows（MinGW）、Linux（SocketCAN）

[Unreleased]: https://github.com/yourusername/CAN_UDS_Tool/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/yourusername/CAN_UDS_Tool/releases/tag/v1.0.0
