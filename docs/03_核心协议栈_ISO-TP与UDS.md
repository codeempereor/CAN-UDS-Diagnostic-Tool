# 03 — 核心协议栈：ISO-TP多帧传输与UDS诊断

## 3.1 为什么需要ISO-TP

CAN一帧最多8字节数据，但UDS诊断经常要发更多数据：
- 读VIN码：17字节
- 写DID：可能几十字节
- 刷写程序：几KB甚至几MB

ISO-TP（ISO 15765-2）就是把大数据拆成多帧CAN帧传输的协议。

## 3.2 四种帧类型

| 类型 | PCI高4位 | 格式 | 作用 |
|-----|----------|------|------|
| 单帧 SF | 0x0 | [长度][数据...] | 数据≤7字节，一帧发完 |
| 首帧 FF | 0x1 | [0x1X][长度低8位][前6字节数据] | 大数据的第一帧，包含总长度 |
| 连续帧 CF | 0x2 | [0x2N序列号][最多7字节数据] | 后续数据帧，有序列号 |
| 流控帧 FC | 0x3 | [0x3X标志][块大小BS][最小间隔STmin] | 接收方告诉发送方：可以继续发多少、发多快 |

### 多帧传输流程

```
发送方                    接收方
  |   FF(首帧,总长度=20) →  |
  |                       |  检查缓冲区，回FC
  |  ← FC(BS=8,STmin=0)   |
  |   CF(seq=1) →          |
  |   CF(seq=2) →          |
  |   ...                  |
```

## 3.3 递归锁的使用场景（踩过的坑）

### 死锁场景

```
1. 调用 sendData()，加锁（std::mutex）
2. 发送首帧，回调 m_sendFrameCallback
3. 回调里调用对方的 handleReceivedFrame()
4. 对方处理后回流控帧，又回调 m_sendFrameCallback
5. 回调里又调用自己的 handleReceivedFrame()，又要加锁 → 死锁！
```

### 解决方案

`std::mutex` → `std::recursive_mutex`

- 递归锁允许同一个线程多次加锁
- 内部有计数，加锁N次要解锁N次才真正释放
- 在回调重入场景下，这是最直接的解决方案

**代价：** 性能略低，且可能隐藏设计问题。
**更优方案（未来改进）：** 回调中不直接处理，把帧放入队列，用单独线程处理，彻底避免重入。

## 3.4 IsoTpClient 类结构

**文件：** `src/core/isotp/isotp_client.h`

```cpp
class IsoTpClient {
    // 回调
    ReceiveCallback m_receiveCallback;    // 重组完成后通知上层
    SendFrameCallback m_sendFrameCallback; // 需要发CAN帧时通知底层

    // 发送状态
    std::vector<uint8_t> m_txBuffer;   // 待发送的完整数据
    size_t m_txIndex;                  // 已发送到第几个字节
    uint8_t m_txSequence;              // 连续帧序列号(0-15循环)
    uint8_t m_blockSize;               // 流控规定的块大小
    uint8_t m_stMin;                   // 帧间最小间隔
    uint8_t m_remainingInBlock;        // 当前块还剩几帧
    bool m_waitingForFlowControl;      // 是否在等流控帧

    // 接收状态
    std::vector<uint8_t> m_rxBuffer;   // 接收缓冲区
    uint32_t m_rxTotalLength;          // 期望总长度
    size_t m_rxReceived;               // 已接收字节数
    uint8_t m_rxSequence;              // 期望的下一个序列号

    IsoTpState m_state;  // Idle / Sending / Receiving
    mutable std::recursive_mutex m_mutex;  // 递归锁
};
```

## 3.5 发送流程逐函数剖析

### sendData 入口

```cpp
bool IsoTpClient::sendData(const std::vector<uint8_t>& data) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (data.empty()) return false;
    if (!m_sendFrameCallback) return false;

    if (data.size() <= 7) {
        return sendSingleFrame(data);  // ≤7字节，单帧直接发
    } else {
        // >7字节，多帧传输
        m_txBuffer = data;
        m_txIndex = 0;
        m_txSequence = 1;  // 连续帧序列号从1开始
        m_state = IsoTpState::Sending;
        m_waitingForFlowControl = true;  // 发完首帧要等流控

        // 首帧带前6字节数据
        size_t firstLen = std::min<size_t>(6, data.size());
        std::vector<uint8_t> firstData(data.begin(), data.begin() + firstLen);
        m_txIndex = firstLen;

        return sendFirstFrame(static_cast<uint32_t>(data.size()), firstData);
    }
}
```

### sendFirstFrame 构造首帧

```cpp
bool IsoTpClient::sendFirstFrame(uint32_t totalLen, const std::vector<uint8_t>& firstData) {
    CanFrame frame;
    frame.id = m_txId;
    frame.dlc = 8;
    frame.data.resize(8);

    // 首帧PCI：高4位=0x1，低12位=总长度
    frame.data[0] = 0x10 | ((totalLen >> 8) & 0x0F);  // 高4位类型 + 长度高4位
    frame.data[1] = totalLen & 0xFF;                   // 长度低8位

    // 数据从第3字节开始，最多6字节
    memcpy(frame.data.data() + 2, firstData.data(), firstData.size());

    if (m_sendFrameCallback) m_sendFrameCallback(frame);
    return true;
}
```

**首帧格式：**
```
字节0: [类型4位=1][长度高4位]  → 0x1X
字节1: [长度低8位]
字节2-7: 前6字节数据
```
12位长度最大4095字节，普通UDS够用了。

### handleFlowControlFrame 处理流控帧

```cpp
void IsoTpClient::handleFlowControlFrame(const CanFrame& frame) {
    if (m_state != Sending || !m_waitingForFlowControl) return;

    uint8_t flag = frame.data[0] & 0x0F;  // 流控标志
    uint8_t bs = frame.data[1];           // 块大小
    uint8_t stMin = frame.data[2];        // 最小间隔

    if (flag == Overflow) { reset(); return; }  // 接收方缓冲区溢出，放弃
    if (flag == Wait) { return; }               // 接收方忙，继续等

    // ContinueToSend：可以发了
    m_blockSize = bs;
    m_stMin = stMin;
    m_remainingInBlock = (bs == 0) ? 0xFF : bs;  // bs=0表示不限块大小
    m_waitingForFlowControl = false;

    sendConsecutiveFrames();  // 开始发连续帧
}
```

### sendConsecutiveFrames 发连续帧

```cpp
void IsoTpClient::sendConsecutiveFrames() {
    while (m_txIndex < m_txBuffer.size() && m_remainingInBlock > 0) {
        CanFrame frame;
        size_t remaining = m_txBuffer.size() - m_txIndex;
        size_t frameLen = std::min<size_t>(7, remaining);  // 每帧最多7字节数据

        frame.data[0] = 0x20 | (m_txSequence & 0x0F);  // PCI: 类型2 + 序列号
        memcpy(frame.data.data() + 1, m_txBuffer.data() + m_txIndex, frameLen);

        if (m_sendFrameCallback) m_sendFrameCallback(frame);

        m_txIndex += frameLen;
        m_txSequence = (m_txSequence + 1) & 0x0F;  // 序列号0-15循环
        m_remainingInBlock--;
    }

    if (m_txIndex >= m_txBuffer.size()) {
        m_state = Idle;  // 发完了
        m_txBuffer.clear();
    } else if (m_remainingInBlock == 0) {
        m_waitingForFlowControl = true;  // 一块发完，等下一个流控
    }
}
```

**连续帧格式：**
```
字节0: [类型4位=2][序列号4位]  → 0x2N
字节1-7: 最多7字节数据
```
序列号0-15循环，接收方用来检测丢帧。

## 3.6 接收流程逐函数剖析

### handleReceivedFrame 分发入口

```cpp
void IsoTpClient::handleReceivedFrame(const CanFrame& frame) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (frame.id != m_rxId) return;  // 不是我的ID，忽略
    if (frame.dlc < 1) return;

    uint8_t pci = frame.data[0];
    uint8_t frameType = (pci >> 4) & 0x0F;  // 高4位是帧类型

    switch (static_cast<IsoTpFrameType>(frameType)) {
    case SingleFrame:       handleSingleFrame(frame); break;
    case FirstFrame:        handleFirstFrame(frame); break;
    case ConsecutiveFrame:  handleConsecutiveFrame(frame); break;
    case FlowControlFrame:  handleFlowControlFrame(frame); break;
    }
}
```

### handleFirstFrame 处理首帧

```cpp
void IsoTpClient::handleFirstFrame(const CanFrame& frame) {
    uint32_t totalLen = ((frame.data[0] & 0x0F) << 8) | frame.data[1];

    m_rxBuffer.clear();
    m_rxBuffer.reserve(totalLen);  // 预分配，避免多次扩容
    m_rxTotalLength = totalLen;
    m_rxReceived = 0;
    m_rxSequence = 1;  // 期望下一个连续帧序列号是1
    m_state = Receiving;

    // 首帧里的前6字节数据
    size_t dataLen = std::min<size_t>(6, frame.dlc - 2);
    m_rxBuffer.insert(m_rxBuffer.end(), frame.data.begin() + 2, frame.data.begin() + 2 + dataLen);
    m_rxReceived += dataLen;

    // 回流控帧，告诉发送方可以发了
    sendFlowControl(ContinueToSend, m_rxBlockSize, m_rxStMin);
}
```

### handleConsecutiveFrame 处理连续帧

```cpp
void IsoTpClient::handleConsecutiveFrame(const CanFrame& frame) {
    if (m_state != Receiving) return;

    uint8_t seq = frame.data[0] & 0x0F;
    if (seq != m_rxSequence) {
        reset();  // 序列号不对，丢帧了，重置
        return;
    }

    size_t dataLen = std::min<size_t>(7, frame.dlc - 1);
    size_t remaining = m_rxTotalLength - m_rxReceived;
    size_t copyLen = std::min(dataLen, remaining);  // 最后一帧可能不满7字节

    m_rxBuffer.insert(m_rxBuffer.end(), frame.data.begin() + 1, frame.data.begin() + 1 + copyLen);
    m_rxReceived += copyLen;
    m_rxSequence = (m_rxSequence + 1) & 0x0F;

    if (m_rxReceived >= m_rxTotalLength) {
        m_state = Idle;  // 收齐了
        if (m_receiveCallback) m_receiveCallback(m_rxBuffer);  // 通知上层
        m_rxBuffer.clear();
    }
}
```

### 关键细节

- `copyLen = min(dataLen, remaining)` — 最后一帧数据可能不足7字节，不能多拷贝，否则会把下一帧的数据也拷进来
- 序列号校验：`seq != m_rxSequence` 就重置，保证数据完整性
- 收齐后通过回调把完整数据交给上层（UDS层）

---

## 3.7 UDS诊断模块

### UDS协议基础

UDS（Unified Diagnostic Services，ISO 14229）是汽车诊断标准协议，运行在ISO-TP之上，用"请求-响应"模式通信。

**肯定响应规则：** 响应ID = 请求ID + 0x40
- 请求 0x10（会话控制）→ 响应 0x50
- 请求 0x22（读DID）→ 响应 0x62
- 请求 0x2E（写DID）→ 响应 0x6E

**否定响应格式：** `0x7F + 请求服务ID + NRC码`
- 0x7F 表示否定响应
- 第二个字节是哪个服务的否定响应
- 第三个字节是NRC（否定响应码），说明为什么失败

### 组合而非继承

```cpp
class UdsClient {
    IsoTpClient m_isoTp;  // 持有一个ISO-TP对象，组合关系
};
```
UDS不是ISO-TP的子类，而是"用"ISO-TP。
这是**组合优于继承**的原则：UDS负责诊断服务编码解码，ISO-TP负责多帧传输，各司其职。

### 构造函数中绑定回调

```cpp
UdsClient::UdsClient() {
    m_isoTp.setReceiveCallback([this](const std::vector<uint8_t>& data) {
        onIsoTpReceived(data);  // ISO-TP收齐数据后，调用UDS解析
    });
}
```
- Lambda表达式捕获this指针，作为ISO-TP的接收回调
- 这是**观察者模式**：UDS订阅ISO-TP的接收事件

## 3.8 UDS请求编码逐函数剖析

### 通用请求 sendRequest

```cpp
bool UdsClient::sendRequest(uint8_t serviceId, const std::vector<uint8_t>& data) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    std::vector<uint8_t> request;
    request.push_back(serviceId);  // 第一个字节是服务ID
    request.insert(request.end(), data.begin(), data.end());  // 后面是参数

    return m_isoTp.sendData(request);  // 交给ISO-TP发送（自动分片）
}
```

### 各个诊断服务就是封装参数

#### 0x10 会话控制

```cpp
bool UdsClient::diagnosticSessionControl(UdsSessionType sessionType) {
    std::vector<uint8_t> data;
    data.push_back(static_cast<uint8_t>(sessionType));  // 参数=会话类型(1字节)
    return sendRequest(0x10, data);
}
```

---

## v1.1 更新补充：超时、请求队列与安全访问状态机

### 一、ISO-TP 超时机制

#### 为什么需要超时？

原始实现中，ISO-TP发送方发出首帧后，如果对方不回流控帧，发送方会**永久等待**；接收方收到首帧后，如果对方不发连续帧，也会**永久卡在Receiving状态**。这在真实总线环境中是不可接受的——ECU可能掉线、总线可能拥塞。

#### 四种错误类型

```cpp
enum class IsoTpErrorType {
    None,
    TxFlowControlTimeout,   // 发送方等待流控帧超时（默认1秒）
    RxConsecutiveTimeout,   // 接收方等待连续帧超时（默认1秒）
    SequenceError,          // 连续帧序列号不匹配
    Overflow                // 接收方缓冲区溢出（对方回流控Overflow标志）
};
```

#### 超时检测核心代码

```cpp
bool IsoTpClient::checkTimeout() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_state == IsoTpState::Idle || m_lastFrameTimeUs == 0) return false;

    uint64_t now = CanFrame::currentTimestampUs();
    uint64_t elapsed = now - m_lastFrameTimeUs;
    if (elapsed < TIMEOUT_US) return false;  // TIMEOUT_US = 1000000 (1秒)

    // 判断是哪种超时
    IsoTpErrorType errorType;
    if (m_state == IsoTpState::Sending && m_waitingForFlowControl) {
        errorType = IsoTpErrorType::TxFlowControlTimeout;
    } else if (m_state == IsoTpState::Receiving) {
        errorType = IsoTpErrorType::RxConsecutiveTimeout;
    } else {
        return false;
    }

    ErrorCallback cb = m_errorCallback;
    reset();  // 超时后重置状态机
    if (cb) cb(errorType);  // 通知上层
    return true;
}
```

**设计要点：**
- `m_lastFrameTimeUs` 在发送首帧和收到任何帧时更新
- 外部需定期调用 `checkTimeout()`（DiagManager中每100ms调用一次）
- 超时后自动 `reset()` 状态机，并通过 `ErrorCallback` 通知上层
- 序列号错误和Overflow在 `handleConsecutiveFrame` / `handleFlowControlFrame` 中即时检测

#### 业务层集成

DiagManager构造函数中启动100ms定时器：
```cpp
m_timeoutTimer = new QTimer(this);
connect(m_timeoutTimer, &QTimer::timeout, this, &DiagManager::onCheckTimeout);
m_timeoutTimer->start(100);  // 每100ms检查一次超时
```

---

### 二、UDS 请求队列与超时

#### 为什么需要请求队列？

UDS是**半双工协议**——同一时间只能有一个未完成的请求。原始实现中，`sendRequest` 不检查是否有pending请求，连续发两个请求会导致响应回来无法区分对应哪个请求，也没有超时机制。

#### PendingRequest 结构

```cpp
struct PendingRequest {
    uint8_t serviceId;      // 请求的服务ID
    uint64_t sendTimeUs;    // 发送时间戳
    bool valid;              // 是否有pending请求
};
```

#### sendRequest 半双工保护

```cpp
bool UdsClient::sendRequest(uint8_t serviceId, const std::vector<uint8_t>& data) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // 半双工：有pending请求时拒绝新请求
    if (m_pendingRequest.valid) return false;

    std::vector<uint8_t> request;
    request.push_back(serviceId);
    request.insert(request.end(), data.begin(), data.end());

    bool success = m_isoTp.sendData(request);
    if (success) {
        // 记录pending请求，用于超时检测和响应匹配
        m_pendingRequest.serviceId = serviceId;
        m_pendingRequest.sendTimeUs = CanFrame::currentTimestampUs();
        m_pendingRequest.valid = true;
    }
    return success;
}
```

#### 请求超时检测（在checkTimeout中）

```cpp
// 检查UDS请求超时（默认3秒）
if (m_pendingRequest.valid) {
    uint64_t now = CanFrame::currentTimestampUs();
    uint64_t elapsed = now - m_pendingRequest.sendTimeUs;
    if (elapsed >= m_requestTimeoutUs) {  // DEFAULT_REQUEST_TIMEOUT_US = 3000000
        uint8_t serviceId = m_pendingRequest.serviceId;
        m_pendingRequest.valid = false;
        UdsErrorCallback cb = m_udsErrorCallback;
        if (cb) cb(UdsErrorType::RequestTimeout, serviceId);
        return true;
    }
}
```

#### 响应匹配（在parseResponse中）

```cpp
// 收到响应后，检查serviceId是否与pending请求匹配
if (m_pendingRequest.valid) {
    if (response.serviceId != m_pendingRequest.serviceId) {
        // 服务ID不匹配，通知错误但仍传递响应
        UdsErrorCallback errCb = m_udsErrorCallback;
        if (errCb) errCb(UdsErrorType::ResponseMismatch, response.serviceId);
    }
    m_pendingRequest.valid = false;  // 无论匹配与否，收到响应就清除pending
}
```

**UdsErrorType 三种错误：**
- `RequestTimeout` — 请求超时（ECU未在3秒内响应）
- `ResponseMismatch` — 响应服务ID与请求不匹配
- `IsoTpError` — ISO-TP层错误透传

---

### 三、UDS 安全访问状态机（0x27服务）

#### 安全访问完整流程

UDS安全访问（SecurityAccess，0x27服务）用于解锁ECU的受保护功能（如写DID、刷写）。完整流程：

```
客户端                          ECU
  |--- 0x27 01 (请求种子) ------>|
  |<-- 0x67 01 [seed] -----------|  (返回种子)
  |--- 0x27 02 [key] ----------->|  (发送密钥，level=请求level+1)
  |<-- 0x67 02 ------------------|  (肯定响应=解锁成功)
```

#### 四种状态

```cpp
enum class SecurityAccessState {
    Idle,                   // 未进行安全访问
    WaitingForSeed,         // 已请求种子，等待ECU返回
    WaitingForKeyResponse,  // 已发送密钥，等待ECU确认
    Unlocked                // 已解锁
};
```

#### 一键完成：startSecurityAccess

```cpp
// 密钥计算回调类型：输入种子，输出密钥
using KeyCalculator = std::function<std::vector<uint8_t>(const std::vector<uint8_t>& seed)>;
// 完成回调：success表示是否解锁成功
using SecurityAccessCallback = std::function<void(bool success, uint8_t unlockedLevel)>;

bool UdsClient::startSecurityAccess(uint8_t level, KeyCalculator keyCalculator,
                                      SecurityAccessCallback callback = nullptr);
```

**使用示例：**
```cpp
udsClient.startSecurityAccess(0x01,  // 请求种子level=1（奇数）
    [](const std::vector<uint8_t>& seed) {
        // 密钥计算算法（由上层提供，不同ECU算法不同）
        std::vector<uint8_t> key;
        for (uint8_t b : seed) key.push_back(b ^ 0xFF);  // 示例：简单异或
        return key;
    },
    [](bool success, uint8_t level) {
        if (success) qDebug() << "解锁成功，level:" << level;
        else qDebug() << "解锁失败";
    });
```

#### 状态流转（在parseResponse中自动处理）

```cpp
if (response.serviceId == 0x27) {  // SecurityAccess响应
    if (response.success) {
        if (m_securityState == WaitingForSeed) {
            // 收到种子 → 计算密钥 → 自动发送密钥
            std::vector<uint8_t> seed = response.data;
            if (seed.size() > 1) seed.erase(seed.begin());  // 去掉level字节
            std::vector<uint8_t> key = m_keyCalculator(seed);
            m_securityState = WaitingForKeyResponse;
            securityAccessSendKey(m_securityLevel + 1, key);  // level+1=偶数
        } else if (m_securityState == WaitingForKeyResponse) {
            // 密钥确认 → 解锁成功
            m_securityState = Unlocked;
            m_unlockedLevel = m_securityLevel;
            if (m_securityCallback) m_securityCallback(true, m_unlockedLevel);
        }
    } else {
        // 否定响应 → 安全访问失败
        m_securityState = Idle;
        if (m_securityCallback) m_securityCallback(false, 0);
    }
}
```

#### 状态查询接口

```cpp
SecurityAccessState securityAccessState() const;  // 当前状态
uint8_t unlockedLevel() const;                      // 已解锁的level（0=未解锁）
bool isSecurityUnlocked(uint8_t level = 0) const;  // 是否解锁了指定level
```

**设计理念：密钥计算与协议分离**
- 协议层（UdsClient）只负责状态流转和帧封装
- 密钥算法由上层通过 `KeyCalculator` 回调注入
- 不同ECU的解锁算法不同（异或、AES、自定义算法），协议层无需关心
- 这是**策略模式**的典型应用——算法可替换，流程不变

#### 0x22 读DID（大端编码）

```cpp
bool UdsClient::readDataByIdentifier(uint16_t did) {
    std::vector<uint8_t> data;
    data.push_back((did >> 8) & 0xFF);  // 高字节在前
    data.push_back(did & 0xFF);         // 低字节在后
    return sendRequest(0x22, data);
}
```

**知识点：大端字节序**
- 多字节数值在网络/总线传输时，高字节在前（大端）
- `(did >> 8) & 0xFF` 取高8位
- `did & 0xFF` 取低8位
- 这叫**大端编码**，网络字节序就是大端

#### 0x2E 写DID

```cpp
bool UdsClient::writeDataByIdentifier(uint16_t did, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> req;
    req.push_back((did >> 8) & 0xFF);  // DID高字节
    req.push_back(did & 0xFF);         // DID低字节
    req.insert(req.end(), data.begin(), data.end());  // 要写的数据
    return sendRequest(0x2E, req);
}
```

#### 0x3E TesterPresent（保活）

```cpp
bool UdsClient::testerPresent(bool suppressPositiveResponse) {
    std::vector<uint8_t> data;
    uint8_t subFunc = 0x00;
    if (suppressPositiveResponse) {
        subFunc |= 0x80;  // 最高位置1，表示"不要回复"
    }
    data.push_back(subFunc);
    return sendRequest(0x3E, data);
}
```

- UDS规定子函数最高位是"抑制肯定响应位"
- 置1后ECU不回复，减少总线负载
- TesterPresent一般每3-5秒发一次，保持诊断会话不超时

## 3.9 UDS响应解析 parseResponse

```cpp
void UdsClient::parseResponse(const std::vector<uint8_t>& data) {
    if (data.empty()) return;

    UdsResponse response;

    if (data[0] == 0x7F) {
        // 否定响应: 0x7F + 服务ID + NRC
        response.success = false;
        response.serviceId = data[1];
        response.nrc = data[2];
    } else {
        // 肯定响应: 服务ID+0x40 + 数据
        response.success = true;
        response.serviceId = data[0] - 0x40;  // 反推请求的服务ID
        response.data.assign(data.begin() + 1, data.end());

        // 如果是会话控制响应，更新当前会话状态
        if (response.serviceId == 0x10 && !response.data.empty()) {
            m_currentSession = static_cast<UdsSessionType>(response.data[0]);
        }
    }

    if (m_responseCallback) m_responseCallback(response);  // 通知上层
}
```

### 关键逻辑

- `data[0] == 0x7F` → 否定响应
- 否则是肯定响应，`data[0] - 0x40` 得到原始服务ID
- 解析完通过回调交给业务层（DiagManager）

### 常见NRC否定响应码

| NRC | 含义 |
|-----|------|
| 0x10 | 通用拒绝 |
| 0x11 | 服务不支持 |
| 0x12 | 子功能不支持 |
| 0x13 | 消息长度错误或格式无效 |
| 0x22 | 条件不正确 |
| 0x31 | 请求超出范围 |
| 0x33 | 安全访问被拒绝 |
| 0x35 | 密钥无效 |
| 0x78 | 请求正确接收，响应待定 |
| 0x7E | 当前会话不支持该子功能 |
| 0x7F | 当前会话不支持该服务 |
