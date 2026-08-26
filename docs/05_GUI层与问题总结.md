# 05 — GUI视图层、问题总结与知识点全景

## 5.1 Qt GUI核心概念

### 信号槽（Signal/Slot）

Qt的观察者模式实现：
- 一个对象发信号（emit），连接到的槽函数自动调用
- 可以跨线程（自动排队）
- 可以一对多（一个信号连多个槽）

### QDockWidget 可停靠面板

- 每个功能面板是一个QDockWidget
- 用户可以拖拽、浮动、关闭
- 主窗口用 `addDockWidget` 添加

### Q_OBJECT宏

- 启用Qt元对象系统（信号槽、属性）
- 包含这个宏的类会被moc（Meta-Object Compiler）预处理
- 生成moc_xxx.cpp文件，实现信号槽的底层机制

---

## 5.2 MainWindow 主窗口

**文件：** `src/gui/main_window.h` + `.cpp`

### 职责

1. 创建所有面板
2. 创建所有Manager
3. 连接信号槽，把各模块串起来

### setupUi 构建界面

```cpp
void MainWindow::setupUi() {
    // 1. 创建面板
    m_tracePanel = new TracePanel(this);
    m_signalPanel = new SignalPanel(this);
    m_diagPanel = new DiagPanel(this);
    m_configPanel = new ConfigPanel(this);
    m_replayPanel = new ReplayPanel(this);

    // 2. 添加到停靠区域
    addDockWidget(Qt::LeftDockWidgetArea, m_configPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_diagPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_signalPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_replayPanel);
    setCentralWidget(m_tracePanel);  // 中央区域是报文监控

    // 3. 允许面板并排停靠
    splitDockWidget(m_signalPanel, m_replayPanel, Qt::Horizontal);
}
```

### setupManagers 创建业务层并连接（中介者模式）

```cpp
void MainWindow::setupManagers() {
    m_canManager = new CanManager(this);
    m_dbcManager = new DbcManager(this);
    m_diagManager = new DiagManager(this);
    m_signalManager = new SignalManager(this);
    m_logManager = new LogManager(this);

    // CAN收到帧 → Trace面板显示
    connect(m_canManager, &CanManager::frameReceived,
            m_tracePanel, &TracePanel::addFrame);

    // CAN收到帧 → 解析DBC信号 → SignalManager更新
    connect(m_canManager, &CanManager::frameReceived,
            this, &MainWindow::onFilteredFrameReceived);

    // 诊断管理器连接CAN
    m_diagManager->setCanManager(m_canManager);

    // 诊断响应 → Diag面板显示
    connect(m_diagManager, &DiagManager::responseReceived,
            m_diagPanel, &DiagPanel::onResponseReceived);
}
```

### 这就是整个系统的"接线"过程

- 每个Manager是一个组件
- MainWindow负责把它们的信号槽连起来
- 组件之间不直接依赖，都通过MainWindow连接
- 这是**中介者模式（Mediator）**：MainWindow作为中介，协调各组件

### onFilteredFrameReceived 帧处理枢纽

```cpp
void MainWindow::onFilteredFrameReceived(const CanFrame& frame) {
    // 1. 记录日志
    if (m_logManager->isRecording()) {
        m_logManager->logFrame(frame);
    }

    // 2. 解析DBC信号
    if (m_dbcManager->isLoaded()) {
        auto signalValues = m_dbcManager->parseFrameSignals(frame);
        for (auto it = signalValues.begin(); it != signalValues.end(); ++it) {
            m_signalManager->onSignalValueReceived(it->first, it->second, frame.timestamp_us);
        }
    }
}
```

---

## 5.3 TracePanel 报文监控面板

**文件：** `src/gui/panels/trace_panel.h` + `.cpp`

### 核心问题：CAN帧来的很快，怎么不卡界面？

### 解决方案：双缓冲 + 定时器批量刷新

```cpp
QList<CanFrame> m_frames;         // 已显示的帧
QList<CanFrame> m_pendingFrames;  // 待显示的帧（缓冲）
QTimer* m_updateTimer;            // 刷新定时器
```

### addFrame 来一帧（可能高频调用）

```cpp
void TracePanel::addFrame(const CanFrame& frame) {
    if (m_paused) return;
    m_pendingFrames.append(frame);  // 只放入缓冲，不刷新界面
    m_frameCount++;
}
```
- 来一帧只放缓冲，O(1)操作，很快
- 不操作QTableWidget（操作GUI控件很慢）

### updateDisplay 定时器回调（每50ms）

```cpp
void TracePanel::updateDisplay() {
    if (m_pendingFrames.isEmpty()) return;

    // 批量插入行
    int startRow = m_table->rowCount();
    m_table->insertRows(startRow, m_pendingFrames.size());

    for (int i = 0; i < m_pendingFrames.size(); ++i) {
        const CanFrame& frame = m_pendingFrames[i];
        int row = startRow + i;
        // 设置每列内容...
        m_table->setItem(row, 0, new QTableWidgetItem(QString::number(m_frameCount)));
        // ... ID、DLC、数据列
    }

    m_frames.append(m_pendingFrames);
    m_pendingFrames.clear();

    // 限制最大行数，超过就删旧的
    while (m_table->rowCount() > 5000) {
        m_table->removeRow(0);
    }

    m_table->scrollToBottom();  // 滚动到底部
}
```

### 为什么用批量插入？

- `insertRows` 一次插N行，比循环插N次快很多
- GUI操作是瓶颈，减少操作次数是关键
- 50ms刷新一次，人眼感觉是实时的，但实际每秒只刷新20次

### formatData 格式化数据为十六进制

```cpp
QString TracePanel::formatData(const std::vector<uint8_t>& data) {
    QString result;
    for (uint8_t b : data) {
        if (!result.isEmpty()) result += " ";
        result += QString("%1").arg(b, 2, 16, QChar('0')).toUpper();
    }
    return result;
}
```
- `QString("%1").arg(b, 2, 16, QChar('0'))` — 2位宽度，16进制，不足补0
- `.toUpper()` — 转大写（A-F）

---

## 5.4 SignalPanel 波形面板

**文件：** `src/gui/panels/signal_panel.h` + `.cpp`

### QPainter自绘波形

Qt没有现成的波形图控件（Qwt是第三方库），所以用QPainter自己画。

### paintEvent 绘制函数

```cpp
void SignalPanel::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);  // 抗锯齿

    // 1. 画网格
    painter.setPen(QPen(Qt::lightGray, 1, Qt::DotLine));
    for (int i = 0; i <= 5; i++) {
        int y = margin + i * (height() - 2*margin) / 5;
        painter.drawLine(margin, y, width() - margin, y);
    }

    // 2. 画每个信号的波形
    for (const auto& signalName : m_signalManager->signalNames()) {
        const SignalData* sig = m_signalManager->signalData(signalName);
        if (!sig || !sig->visible) continue;

        painter.setPen(QPen(sig->color, 2));

        QPolygonF polyline;
        for (size_t i = 0; i < sig->values.size(); i++) {
            double x = margin + (double)i / sig->values.size() * (width() - 2*margin);
            double y = height() - margin - (sig->values[i] - sig->minValue) / (sig->maxValue - sig->minValue) * (height() - 2*margin);
            polyline << QPointF(x, y);
        }
        painter.drawPolyline(polyline);
    }
}
```

### 逐行剖析

- `QPainter painter(this)` — 在当前控件上绘画
- `setRenderHint(Antialiasing)` — 开启抗锯齿，线条更平滑
- `QPolygonF` — 点的集合，drawPolyline一次画完，效率高
- 坐标转换：数据值 → 像素坐标
  - x：按索引均匀分布
  - y：按数值范围映射（min→底部，max→顶部）

### 为什么用QPolygonF而不是循环drawLine？

- `drawPolyline` 一次调用画完整条线，比循环N次drawLine快很多
- 减少函数调用开销，Qt内部可以优化

---

## 5.5 DiagPanel 诊断面板

**文件：** `src/gui/panels/diag_panel.h` + `.cpp`

### QTabWidget 多标签页

```cpp
m_tabWidget = new QTabWidget(this);
m_tabWidget->addTab(createSessionTab(), "会话控制");
m_tabWidget->addTab(createReadDidTab(), "读DID");
m_tabWidget->addTab(createWriteDidTab(), "写DID");
m_tabWidget->addTab(createRoutineTab(), "例程控");
```

### onResponseReceived 收到诊断响应

```cpp
void DiagPanel::onResponseReceived(const UdsResponse& response) {
    QString time = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");

    if (response.isPositive()) {
        // 绿色显示肯定响应
        QString hexData = bytesToHex(response.data);
        m_logEdit->setTextColor(Qt::darkGreen);
        m_logEdit->append(QString("[%1] [肯定] 服务0x%2 数据: %3")
            .arg(time)
            .arg(response.serviceId, 2, 16, QChar('0'))
            .arg(hexData));
    } else {
        // 红色显示否定响应
        m_logEdit->setTextColor(Qt::red);
        m_logEdit->append(QString("[%1] [否定] 服务0x%2 NRC: 0x%3 (%4)")
            .arg(time)
            .arg(response.serviceId, 2, 16, QChar('0'))
            .arg(response.nrc, 2, 16, QChar('0'))
            .arg(QString::fromStdString(response.nrcDescription())));
    }
}
```

### 知识点

- `QString::arg()` — 字符串格式化，类似printf但更安全
- `response.serviceId, 2, 16, QChar('0')` — 2位宽度，16进制，不足补0
- `QTextEdit::setTextColor` — 设置后续append的文字颜色
- 诊断日志用颜色区分肯定/否定响应，直观

### parseHexData 解析十六进制输入

```cpp
std::vector<uint8_t> DiagPanel::parseHexData(const QString& text) {
    std::vector<uint8_t> data;
    QStringList bytes = text.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const QString& b : bytes) {
        bool ok;
        uint8_t val = static_cast<uint8_t>(b.toUInt(&ok, 16));
        if (ok) data.push_back(val);
    }
    return data;
}
```
- `QRegularExpression("\\s+")` — 按空白字符分割（Qt6用这个，不用QRegExp）
- `Qt::SkipEmptyParts` — 跳过空字符串
- `b.toUInt(&ok, 16)` — 16进制字符串转无符号整数

---

## 5.6 ConfigPanel 配置面板

**文件：** `src/gui/panels/config_panel.h` + `.cpp`

### 功能

- 硬件配置（适配器类型、接口名、波特率）
- ID过滤（白名单/黑名单、过滤ID）
- 启动/停止按钮

### 信号连接

```cpp
// 启动按钮 → MainWindow的启动槽
connect(m_startBtn, &QPushButton::clicked,
        this, &ConfigPanel::startRequested);
// MainWindow连接到自己的启动逻辑
connect(m_configPanel, &ConfigPanel::startRequested,
        this, &MainWindow::onStartClicked);
```
- 面板只发信号，不直接操作Manager
- MainWindow作为中介，连接信号和业务逻辑

---

## 5.7 ReplayPanel 日志回放面板

**文件：** `src/gui/panels/replay_panel.h` + `.cpp`

### 功能

- 加载ASC格式日志文件
- 播放/暂停/停止
- 进度条拖拽跳转
- 倍速播放（1x~100x）

### 播放逻辑

```cpp
void ReplayPanel::play() {
    if (m_frames.isEmpty()) return;
    m_playing = true;
    m_playTimer->start(10);  // 10ms检查一次是否该发下一帧
}

void ReplayPanel::onPlayTimer() {
    if (!m_playing) return;
    // 根据时间戳和倍速，判断是否该发下一帧
    while (m_currentIndex < m_frames.size()) {
        const LogFrame& lf = m_frames[m_currentIndex];
        double expectedTime = (lf.timestamp - m_startTime) / m_speed;
        if (m_elapsedTime >= expectedTime) {
            emit frameReplayed(lf.frame);
            m_currentIndex++;
        } else {
            break;
        }
    }
    m_elapsedTime += 0.01;  // 10ms
}
```

---

## 第六部分：关键技术问题与解决方案

### 问题1：ISO-TP回调重入死锁

**现象：** 多帧传输测试程序卡死，无输出。

**根因：**
```
sendData() 加锁 → 发首帧 → 回调 → 对方handleReceivedFrame()
→ 对方回流控帧 → 回调 → 自己handleReceivedFrame() → 又要加锁 → 死锁！
```

**解决方案：** `std::mutex` → `std::recursive_mutex`
- 递归锁允许同一个线程多次加锁
- 内部有计数，加锁N次要解锁N次才真正释放
- 在回调重入场景下，这是最直接的解决方案

**更优方案（未来改进）：** 回调中不直接处理，把帧放入队列，用单独线程处理，彻底避免重入。

---

### 问题2：Qt的signals宏与变量名冲突

**现象：** 编译报错 `expected unqualified-id before 'public'`

**根因：** Qt的moc会把 `signals` 定义为宏（实际上是 `protected` 或 `public` 的别名）。如果代码里有变量叫 `signals`，预处理器会把它替换掉，导致语法错误。

**解决方案：** 变量改名 `signals` → `signalList`

**教训：** 在Qt项目中，避免用 `signals`、`slots`、`emit` 作为变量名。这些是Qt的关键字/宏。核心层代码虽然不依赖Qt，但如果被Qt代码include，也会受影响。

---

### 问题3：Qt6移除了QRegExp

**现象：** `'QRegExp' was not declared in this scope`

**根因：** Qt6中QRegExp被废弃移除，改用QRegularExpression。

**解决方案：**
```cpp
// Qt5写法
line.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
// Qt6写法
line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
```

**QRegularExpression的优势：** 性能更好（基于PCRE2），支持更多正则特性，线程安全。

---

### 问题4：GUI高频刷新卡顿

**现象：** 报文量大时界面卡顿。

**根因：** 来一帧就刷新一次QTableWidget，GUI操作太慢。

**解决方案：** 双缓冲 + 定时器批量刷新
- 来帧只放内存缓冲（O(1)）
- 每50ms批量刷新一次界面（每秒20次，人眼无感知）
- 批量insertRows，减少GUI操作次数

---

### 问题5：跨线程数据竞争

**现象：** 偶尔崩溃或数据错乱。

**根因：** 接收线程写数据，GUI线程读数据，没有同步。

**解决方案：**
- 共享数据用mutex保护（QMutexLocker）
- 或者用信号槽跨线程（Qt自动排队）
- 原子变量用于简单标志（std::atomic）

---

### 问题6：QMap operator[]返回临时值不能取地址

**现象：** `taking address of rvalue` 编译错误

**根因：** QMap的const operator[]返回的是值（临时对象），不是引用，不能取地址。

**解决方案：** 用constFind + value()
```cpp
// 错误写法
return &m_signals[name];  // name是临时值，不能取地址
// 正确写法
auto it = m_signals.constFind(name);
if (it == m_signals.constEnd()) return nullptr;
return &it.value();  // 迭代器的value()返回引用
```

---

## 第七部分：知识点全景图

### C++知识点

| 知识点 | 用在何处 |
|--------|---------|
| 初始化列表 | 所有构造函数 |
| RAII（lock_guard） | 所有mutex加锁 |
| std::function | 核心层回调 |
| Lambda表达式 | 回调绑定、线程函数 |
| std::thread | 接收线程 |
| std::atomic | 线程运行标志 |
| std::recursive_mutex | ISO-TP/UDS防重入 |
| 范围for循环 | 遍历容器 |
| auto类型推导 | 简化代码 |
| 移动语义 | 容器swap优化 |
| 枚举类enum class | 所有枚举（类型安全） |
| 纯虚函数/抽象类 | 适配器基类 |
| 组合优于继承 | UdsClient持有IsoTpClient |
| std::chrono | 时间戳、线程睡眠 |
| 位运算 | CAN ID、掩码、信号提取 |
| std::deque | 波形数据双端队列 |
| std::map | ID分布、信号值映射 |
| std::vector | 数据载荷、缓冲 |
| 符号扩展 | DBC有符号信号转换 |
| 大端/小端 | DBC位提取、UDS DID编码 |

### Qt知识点

| 知识点 | 用在何处 |
|--------|---------|
| 信号槽Signal/Slot | 所有模块间通信 |
| Q_OBJECT宏 | 所有QObject子类 |
| QDockWidget | 可停靠面板 |
| QTimer | 定时刷新、统计 |
| QMutexLocker | 跨线程数据保护 |
| QPainter自绘 | 波形图绘制 |
| QTableWidget | 报文列表 |
| QTabWidget | 诊断多标签 |
| QPolygonF | 波形点集 |
| QString::arg | 字符串格式化 |
| QRegularExpression | 字符串分割（Qt6） |
| QMainWindow | 主窗口 |
| QPushButton/QLineEdit/QComboBox | 配置面板控件 |
| QTextEdit | 诊断日志显示 |
| QColor | 信号波形颜色 |

### 汽车电子协议知识点

| 知识点 | 说明 |
|--------|------|
| CAN帧结构 | ID、DLC、数据、扩展帧 |
| 掩码过滤 | 硬件过滤器原理 |
| DBC格式 | BO_报文、SG_信号 |
| 大小端位提取 | Intel/Motorola字节序 |
| ISO-TP | 单帧/首帧/连续帧/流控 |
| UDS服务 | 0x10/0x22/0x2E/0x31/0x3E |
| 肯定响应=ID+0x40 | UDS响应规则 |
| NRC否定响应码 | 诊断失败原因 |
| 总线负载率计算 | 位数/时间/波特率 |
| ASC日志格式 | CANoe通用日志格式 |

### 设计模式知识点

| 模式 | 用在何处 |
|------|---------|
| 观察者模式 | 回调、信号槽 |
| 策略模式 | 适配器替换、过滤模式 |
| 状态模式 | ISO-TP状态机 |
| 适配器模式 | CanAdapterBase统一接口 |
| 模板方法 | 基类通知流程 |
| 外观模式 | Manager封装核心层 |
| 中介者模式 | MainWindow连接各组件 |
| RAII | 锁、文件句柄 |
| 工厂思想 | CanManager根据类型创建适配器 |

---

## 第八部分：如何继续扩展这个项目

### 扩展方向1：添加新的硬件适配器
1. 继承 `CanAdapterBase`
2. 实现 `open/close/sendFrame/isOpen/adapterName`
3. 在 `CanManager::start` 的工厂里加一个类型
4. 业务层和GUI完全不用改

### 扩展方向2：添加新的UDS服务
1. 在 `UdsClient` 加一个方法，封装参数
2. 在 `DiagManager` 加对应的Qt风格方法
3. 在 `DiagPanel` 加一个标签页
4. 核心协议栈的ISO-TP完全不用改

### 扩展方向3：支持CAN FD
1. `CanFrame` 的data最大从8改到64
2. ISO-TP的单帧阈值从7改到62（CAN FD首帧可以带更多数据）
3. 适配器层支持CAN FD配置

### 扩展方向4：添加脚本自动化
1. 嵌入Lua/Python脚本引擎
2. 用脚本编写诊断序列（比如UDS刷写流程）
3. 自动执行、自动判断结果

---

## 总结

这个项目的核心价值不在于代码量，而在于**架构设计**：

1. **分层解耦** — 核心协议栈零Qt依赖，可移植可测试
2. **面向接口** — 硬件可替换，服务可扩展
3. **观察者模式** — 回调+信号槽贯穿全项目，模块间低耦合
4. **线程隔离** — 接收线程与GUI线程分离，不卡界面

吃透这个项目，你就掌握了：
- C++11/14/17现代编程
- Qt GUI开发（信号槽、多线程、自绘控件）
- CAN/DBC/ISO-TP/UDS汽车协议
- 常见设计模式的实际应用
- 分层架构的设计思想

这就是一个嵌入式/汽车电子工程师的**全栈能力样板**。

---

## v1.1 更新补充：3个新面板、导出CSV、波形显示控制

### 一、SendPanel 报文发送面板

**文件：** `src/gui/panels/send_panel.h` + `.cpp`

#### 功能

- CAN ID输入（十六进制，默认7E0）
- 扩展帧复选框
- 数据输入（十六进制，空格分隔，最多8字节）
- 单次发送按钮 + 发送计数
- 循环发送：启用复选框 + 周期配置（1~60000ms）

#### 核心代码

```cpp
void SendPanel::onSendClicked() {
    CanFrame frame = buildFrame();
    emit sendFrameRequested(frame);  // 发送信号，由MainWindow连接到CanManager
    m_sendCount++;
    m_countLabel->setText(QString("已发送: %1 帧").arg(m_sendCount));
}

CanFrame SendPanel::buildFrame() const {
    CanFrame frame;
    bool ok;
    frame.id = static_cast<uint32_t>(m_idEdit->text().toUInt(&ok, 16));  // 十六进制解析
    frame.extended = m_extendedCheck->isChecked();
    parseHexData(m_dataEdit->text(), frame.data);  // 解析十六进制数据
    frame.dlc = static_cast<uint8_t>(frame.data.size());
    frame.timestamp_us = CanFrame::currentTimestampUs();
    return frame;
}
```

#### 循环发送定时器

```cpp
void SendPanel::onCyclicToggled(bool checked) {
    if (checked) {
        m_cyclicTimer->start(m_periodSpin->value());  // 按设定周期启动
        m_sendBtn->setEnabled(false);  // 循环发送时禁用单次发送按钮
    } else {
        m_cyclicTimer->stop();
        m_sendBtn->setEnabled(true);
    }
}
```

#### MainWindow 中的连接

```cpp
connect(m_sendPanel, &SendPanel::sendFrameRequested,
        m_canManager, &CanManager::sendFrame);
```

**设计要点：** 面板只发信号，不直接操作CanManager，MainWindow作为中介者连接。

---

### 二、SignalValuePanel 信号数值面板

**文件：** `src/gui/panels/signal_value_panel.h` + `.cpp`

#### 功能

- 表格实时显示所有DBC信号：信号名、当前值、单位、原始值、描述
- 信号名过滤搜索框（实时过滤）
- 导出CSV按钮

#### 表格结构

```cpp
m_table->setColumnCount(5);
m_table->setHorizontalHeaderLabels({"信号名", "当前值", "单位", "原始值", "描述"});
m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);  // 只读
m_table->setAlternatingRowColors(true);  // 交替行颜色
m_table->verticalHeader()->setVisible(false);  // 隐藏行号
```

#### 实时更新

```cpp
// 连接SignalManager的dataUpdated信号
connect(m_signalManager, &SignalManager::dataUpdated,
        this, &SignalValuePanel::refreshValues);

void SignalValuePanel::refreshValues() {
    for (auto it = m_rowMap.begin(); it != m_rowMap.end(); ++it) {
        updateRowValue(it.value(), it.key());
    }
}

void SignalValuePanel::updateRowValue(int row, const QString& name) {
    const SignalData* sig = m_signalManager->signalData(name);
    if (!sig || sig->values.empty()) return;
    double currentValue = sig->values.back();  // 取最新值
    m_table->item(row, 1)->setText(QString::number(currentValue, 'f', 3));
    m_table->item(row, 2)->setText(sig->unit);  // 显示单位
}
```

#### 信号名过滤

```cpp
void SignalValuePanel::onFilterTextChanged(const QString& text) {
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QString name = m_table->item(row, 0)->text();
        bool match = text.isEmpty() || name.contains(text, Qt::CaseInsensitive);
        m_table->setRowHidden(row, !match);  // 不匹配的行隐藏
    }
}
```

**设计要点：** 用 `m_rowMap`（QMap<QString, int>）记录信号名到行号的映射，O(1)查找更新。

---

### 三、StatsPanel 总线统计面板

**文件：** `src/gui/panels/stats_panel.h` + `.cpp`

#### 功能

- 顶部6个统计卡片：总帧数、总字节、负载率、唯一ID数、错误帧、远程帧
- 负载率颜色告警：绿(<50%) / 橙(<80%) / 红(≥80%)
- 下方ID分布表格：CAN ID、帧数、占比、柱状图
- 按帧数降序排列，实时更新

#### 统计卡片布局

```cpp
auto addStat = [&](int row, int col, const QString& title, QLabel* value) {
    auto* titleLabel = new QLabel(title, this);
    titleLabel->setStyleSheet("color: #666; font-size: 11px;");
    value->setStyleSheet("font-size: 16px; font-weight: bold;");
    statsLayout->addWidget(titleLabel, row * 2, col);
    statsLayout->addWidget(value, row * 2 + 1, col);
};
addStat(0, 0, "总帧数", m_totalFramesLabel);
addStat(0, 1, "总字节", m_totalBytesLabel);
addStat(0, 2, "负载率", m_busLoadLabel);
addStat(1, 0, "唯一ID数", m_uniqueIdsLabel);
addStat(1, 1, "错误帧", m_errorFramesLabel);
addStat(1, 2, "远程帧", m_remoteFramesLabel);
```

#### 负载率颜色告警

```cpp
double load = stats.busLoadPercent(bitrate);
if (load > 80) m_busLoadLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: red;");
else if (load > 50) m_busLoadLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: orange;");
else m_busLoadLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: green;");
```

#### ID分布柱状图（用Unicode方块字符）

```cpp
int barLen = (int)((double)pair.second / maxCount * 20);  // 最多20个方块
QString bar = QString(barLen, QChar(0x2588));  // 0x2588 = 实心方块█
auto* barItem = new QTableWidgetItem(bar);
barItem->setForeground(QBrush(QColor(52, 152, 219)));  // 蓝色
m_idTable->setItem(row, 3, barItem);
```

**设计要点：** 不用QPainter自绘柱状图，用Unicode方块字符 + 前景色，简单高效，在QTableWidget中直接显示。

#### MainWindow 中的更新

```cpp
void MainWindow::onStatsUpdate() {
    const CanStats& stats = m_canManager->stats();
    // ... 更新状态栏 ...
    if (m_statsPanel) {
        m_statsPanel->updateStats(stats, 500000);  // 500kbps
    }
}
```

---

### 四、导出CSV功能

#### TracePanel 报文导出

```cpp
bool TracePanel::exportToCsv(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "No.,Timestamp(us),ID,Extended,DLC,Data\n";  // CSV表头
    for (int i = 0; i < m_frames.size(); ++i) {
        const CanFrame& frame = m_frames[i];
        out << i + 1 << ","
            << frame.timestamp_us << ","
            << "0x" << QString::number(frame.id, 16).toUpper() << ","
            << (frame.extended ? "1" : "0") << ","
            << static_cast<int>(frame.dlc) << ","
            << "\"" << formatData(frame.data) << "\"\n";  // 数据用引号包裹（含空格）
    }
    file.close();
    return true;
}
```

**CSV格式要点：**
- 第一行是表头
- 含空格/逗号的字段用双引号包裹
- UTF-8编码，Excel可直接打开
- CAN ID用 `0x` 前缀十六进制显示

#### SignalValuePanel 信号数据导出

```cpp
bool SignalValuePanel::exportToCsv(const QString& filePath) {
    QTextStream out(&file);
    out << "SignalName,CurrentValue,Unit,RawValue,Description\n";
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (m_table->isRowHidden(row)) continue;  // 只导出可见（未被过滤）的行
        out << "\"" << m_table->item(row, 0)->text() << "\","  // 信号名
            << m_table->item(row, 1)->text() << ","              // 当前值
            << "\"" << m_table->item(row, 2)->text() << "\","   // 单位
            << m_table->item(row, 3)->text() << ","              // 原始值
            << "\"" << m_table->item(row, 4)->text() << "\"\n";  // 描述
    }
}
```

---

### 五、波形面板信号显示/隐藏控制

#### 问题

SignalPanel左侧的信号列表原本有复选框，但勾选状态没有真正控制波形显示——所有信号始终都画。

#### 修复：连接itemChanged信号

```cpp
// setupUi中连接
connect(m_signalList, &QListWidget::itemChanged,
        this, &SignalPanel::onSignalItemChanged);

// 槽函数：勾选状态同步到SignalManager
void SignalPanel::onSignalItemChanged(QListWidgetItem* item) {
    if (!m_signalManager) return;
    QString name = item->text();
    bool visible = (item->checkState() == Qt::Checked);
    m_signalManager->setSignalVisible(name, visible);  // 同步到数据源
}
```

#### SignalPlotWidget 绘制时跳过隐藏信号

```cpp
void SignalPlotWidget::drawSignals(QPainter& painter) {
    for (const auto& signalName : m_signalManager->signalNames()) {
        const SignalData* sig = m_signalManager->signalData(signalName);
        if (!sig || !sig->visible) continue;  // 跳过隐藏的信号
        // ... 绘制波形 ...
    }
}
```

**设计要点：**
- 复选框状态是UI层的，`SignalData::visible` 是数据层的
- 通过 `onSignalItemChanged` 把UI状态同步到数据层
- 绘制时检查 `sig->visible`，隐藏的信号不画
- 这是**MVC模式**的简化版：视图（复选框）→ 控制器（槽函数）→ 模型（SignalData）

---

### 六、v1.1 GUI面板总览（10个面板）

| 面板 | 类名 | 停靠位置 | 功能 |
|------|------|---------|------|
| 配置面板 | ConfigPanel | 左侧 | 硬件配置、ID过滤、启动/停止 |
| 报文监控 | TracePanel | 中央 | 实时报文列表、过滤、暂停、导出CSV |
| 信号波形 | SignalPanel | 底部 | QPainter自绘波形、信号显示/隐藏 |
| 信号数值 | SignalValuePanel | 右侧 | 信号当前值表格、过滤、导出CSV |
| UDS诊断 | DiagPanel | 右侧 | 多标签诊断操作、诊断日志 |
| 日志回放 | ReplayPanel | 底部 | ASC日志加载、播放控制、倍速 |
| 报文发送 | SendPanel | 左侧 | 手动/循环发送CAN帧 |
| 总线统计 | StatsPanel | 右侧 | 统计卡片、ID分布柱状图 |

**8个功能面板 + 主窗口 + 状态栏 = 完整的CAN总线分析工具GUI**
