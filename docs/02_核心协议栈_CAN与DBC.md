# 02 — 核心协议栈：CAN基础与DBC解析

## 2.1 CanFrame 数据结构

**文件：** `src/core/can/can_frame.h`

```cpp
struct CanFrame {
    uint32_t id = 0;           // CAN帧ID（标准帧11位，扩展帧29位）
    bool extended = false;     // 是否扩展帧
    bool remote = false;       // 是否远程帧
    bool error = false;        // 是否错误帧
    uint8_t dlc = 0;           // 数据长度（0-8）
    std::vector<uint8_t> data; // 数据载荷
    uint64_t timestamp_us = 0; // 时间戳（微秒）
```

### 知识点剖析

1. **结构体默认成员初始化**（C++11特性）：`uint32_t id = 0;` 直接在声明时初始化，构造函数不用写了。
2. **为什么用vector不用固定数组？** 灵活，CAN FD可以到64字节，以后扩展方便。
3. **时间戳为什么用微秒？** CAN帧间隔可能很短，毫秒精度不够。

### 构造函数逐行剖析

```cpp
CanFrame(uint32_t id, const std::vector<uint8_t>& data, bool extended = false)
    : id(id), extended(extended), data(data) {   // 初始化列表
    dlc = static_cast<uint8_t>(data.size());     // size_t强转uint8_t
    auto now = std::chrono::system_clock::now(); // C++11时间库
    timestamp_us = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count());    // 从1970年到现在的微秒数
}
```

- `: id(id), extended(extended), data(data)` — **初始化列表**，比在函数体里赋值效率高（直接构造，不先默认构造再赋值）
- `std::chrono::system_clock::now()` — 获取系统当前时间
- `now.time_since_epoch()` — 从1970年1月1日到现在的时长
- `duration_cast<microseconds>()` — 转换成微秒
- `.count()` — 取出数值

### 静态函数 currentTimestampUs

```cpp
static uint64_t currentTimestampUs() {
    auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count());
}
```
- `static` 静态成员函数，不需要对象就能调用
- 其他地方需要时间戳直接 `CanFrame::currentTimestampUs()`

---

## 2.2 CanFilter 过滤器

**文件：** `src/core/can/can_filter.h` + `.cpp`

### 设计思路

CAN总线上报文很多，用户可能只想看特定ID。过滤器支持：
- **白名单**：只显示列表里的ID
- **黑名单**：不显示列表里的ID
- **掩码匹配**：不是精确匹配，而是按位掩码匹配（硬件过滤器的原理）

### FilterRule 结构体

```cpp
struct FilterRule {
    uint32_t id = 0;
    uint32_t mask = 0x7FF;  // 默认11位全掩码，即精确匹配
    bool extended = false;

    bool match(const CanFrame& frame) const {
        if (extended != frame.extended) return false;
        return (frame.id & mask) == (id & mask);
    }
};
```

### match函数逐行剖析

- `if (extended != frame.extended) return false;` — 帧类型不同直接不匹配
- `(frame.id & mask) == (id & mask)` — **掩码匹配的核心**
  - mask某一位是1：这一位要参与比较
  - mask某一位是0：这一位忽略（两边都是0，相等）
  - 例：id=0x100, mask=0x700 → 只比较高3位，0x100~0x1FF都匹配

### CanFilter::pass 核心过滤函数

```cpp
bool CanFilter::pass(const CanFrame& frame) const {
    if (m_rules.empty()) return true;  // 没有规则，全部放行

    bool matched = false;
    for (const auto& rule : m_rules) {       // 范围for循环（C++11）
        if (rule.match(frame)) { matched = true; break; }
    }

    if (m_mode == FilterMode::Whitelist) return matched;   // 白名单：匹配到才放行
    else return !matched;  // 黑名单：匹配到就拦截
}
```

### 知识点

- `const` 成员函数：承诺不修改成员变量，可以被const对象调用
- `for (const auto& rule : m_rules)` — 范围for循环，auto自动类型推导，&引用避免拷贝
- 白名单逻辑：匹配到→放行；黑名单逻辑：匹配到→拦截

---

## 2.3 CanStats 总线统计

**文件：** `src/core/can/can_stats.h` + `.cpp`

### 线程安全设计

```cpp
mutable std::mutex m_mutex;
```
- `mutable` — 允许在const函数里修改这个成员
- 为什么需要？统计函数都是const的（只读语义），但加锁要修改mutex状态

### update函数

```cpp
void CanStats::update(const CanFrame& frame) {
    std::lock_guard<std::mutex> lock(m_mutex);  // RAII加锁
    if (m_startTimeUs == 0) m_startTimeUs = frame.timestamp_us;
    m_totalFrames++;
    m_totalBytes += frame.dlc;
    if (frame.error) m_errorFrames++;
    if (frame.remote) m_remoteFrames++;
    m_idCount[frame.id]++;  // map的operator[]，不存在就创建为0再++
}
```

### 知识点

- `std::lock_guard` — **RAII锁**，构造时加锁，析构时自动解锁，不会忘记解锁
- `m_idCount[frame.id]++` — std::map用法：
  - 如果key不存在，自动插入默认值0，然后++变成1
  - 如果存在，直接++

### busLoadPercent 负载率计算

```cpp
double CanStats::busLoadPercent(uint32_t bitrate_bps) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_startTimeUs == 0 || bitrate_bps == 0) return 0.0;
    uint64_t now = CanFrame::currentTimestampUs();
    double elapsedSec = (now - m_startTimeUs) / 1000000.0;  // 微秒转秒
    double bitsPerFrame = 47 + 8 * 8;  // 帧开销47位 + 8字节数据
    double totalBits = m_totalFrames * bitsPerFrame;
    double maxBits = bitrate_bps * elapsedSec;
    return (totalBits / maxBits) * 100.0;
}
```

### CAN帧开销为什么是47位？

标准CAN帧结构：
- 帧起始(1) + 仲裁场(12) + 控制场(6) + 数据场(0-64) + CRC场(16) + ACK场(2) + 帧结束(7) + 帧间间隔(3)
- 不含数据的开销 ≈ 47位

---

## 2.4 DbcSignal 信号定义

**文件：** `src/core/dbc/dbc_signal.h`

DBC文件里一个信号长这样：
```
SG_ EngineSpeed : 0|16@0+ (1,0) [0|8000] "rpm" TestTool
```

### rawToPhysical 原始值转物理值

```cpp
double rawToPhysical(uint64_t raw) const {
    double val = 0;
    if (valueType == ValueType::Signed) {
        int64_t signedRaw = static_cast<int64_t>(raw);
        // 符号扩展：如果最高位是1，说明是负数，要把高位全补1
        if (bitLength < 64 && (raw & (1ULL << (bitLength - 1)))) {
            signedRaw |= ~((1ULL << bitLength) - 1);
        }
        val = static_cast<double>(signedRaw);
    } else {
        val = static_cast<double>(raw);
    }
    return val * factor + offset;  // 物理值 = 原始值 × 系数 + 偏移
}
```

### 符号扩展逐行剖析（核心难点）

假设 bitLength=8, raw=0xFF（二进制11111111，代表-1）：

1. `1ULL << (bitLength - 1)` = 1 << 7 = 0x80（最高位掩码）
2. `raw & 0x80` = 0x80 ≠ 0，说明是负数
3. `(1ULL << bitLength) - 1` = (1<<8) - 1 = 0xFF（低8位全1的掩码）
4. `~0xFF` = 0xFFFFFFFFFFFFFF00（高位全1，低8位全0）
5. `signedRaw |= 高位全1` = 0x00000000000000FF | 0xFFFFFFFFFFFFFF00 = 0xFFFFFFFFFFFFFFFF = -1

这就是**符号扩展**：把N位负数变成64位负数，高位全部补1。

### 公式

**物理值 = 原始值 × factor + offset**
这是DBC标准规定的，所有DBC工具都这么算。

---

## 2.5 DbcMessage 位提取（整个项目最难的函数）

**文件：** `src/core/dbc/dbc_message.h`

### extractRawValue 从CAN数据中提取信号原始值

```cpp
uint64_t extractRawValue(const uint8_t* data, uint8_t dataLen, const DbcSignal& signal) const {
    if (signal.startBit + signal.bitLength > dataLen * 8) return 0;  // 越界保护

    uint64_t raw = 0;
    if (signal.byteOrder == ByteOrder::Intel) {
        // 小端（Intel）：低位在前，bit编号从0开始连续递增
        for (int i = signal.bitLength - 1; i >= 0; --i) {
            uint8_t bitPos = signal.startBit + i;
            uint8_t byteIdx = bitPos / 8;   // 第几个字节
            uint8_t bitIdx = bitPos % 8;    // 字节内第几位
            raw = (raw << 1) | ((data[byteIdx] >> bitIdx) & 0x01);
        }
    } else {
        // 大端（Motorola）：高位在前，bit编号是"跳着"的
        uint8_t startByte = signal.startBit / 8;
        uint8_t startBitInByte = signal.startBit % 8;
        for (uint8_t i = 0; i < signal.bitLength; ++i) {
            uint8_t bitPos = startBitInByte + i;
            uint8_t byteIdx = startByte - (bitPos / 8);  // 字节号递减！
            uint8_t bitIdx = 7 - (bitPos % 8);           // 位号从高到低！
            raw = (raw << 1) | ((data[byteIdx] >> bitIdx) & 0x01);
        }
    }
    return raw;
}
```

### 必须彻底理解的大小端差异

#### Intel小端的位编号规则

```
字节0: bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0  → DBC编号 7 6 5 4 3 2 1 0
字节1: bit15 bit14 ... bit8                     → DBC编号 15 14 ... 8
```
bit号连续递增，startBit=0就是字节0的bit0。
所以 `byteIdx = bitPos / 8`，`bitIdx = bitPos % 8`，直接算。

#### Motorola大端的位编号规则（反直觉！）

```
字节0: bit7 bit6 ... bit0  → DBC编号 7 6 ... 0
字节1: bit15 ... bit8     → DBC编号 15 ... 8
```
DBC里startBit=0，实际是字节0的bit7（最高位）！
跨字节时，字节号是**递减**的（先高字节后低字节）。

所以大端的算法：
- `startByte = startBit / 8` — 起始字节
- `startBitInByte = startBit % 8` — 起始字节内的位置
- 每往后一位，`bitPos`加1
- `byteIdx = startByte - (bitPos / 8)` — 字节号递减
- `bitIdx = 7 - (bitPos % 8)` — 字节内从高位往低位走

### 提取位的通用公式

```cpp
raw = (raw << 1) | ((data[byteIdx] >> bitIdx) & 0x01);
```
- `data[byteIdx] >> bitIdx` — 把目标位移到最低位
- `& 0x01` — 只取这一位
- `raw << 1 | bit` — raw左移一位，新的位放到最低位
- 循环bitLength次，就把所有位拼起来了

### ⚠️ v1.1 Bug修复：大端跨字节位提取越界

**原始有bug的算法：**
```cpp
uint8_t startByte = signal.startBit / 8;
uint8_t startBitInByte = signal.startBit % 8;
for (uint8_t i = 0; i < signal.bitLength; ++i) {
    uint8_t bitPos = startBitInByte + i;
    uint8_t byteIdx = startByte - (bitPos / 8);  // ❌ 跨字节时变负数！
    uint8_t bitIdx = 7 - (bitPos % 8);
    raw = (raw << 1) | ((data[byteIdx] >> bitIdx) & 0x01);
}
```

**Bug根因：** 当 `startBitInByte + i > 7` 时（即跨字节），`bitPos / 8 >= 1`，导致 `byteIdx = startByte - 1`。对于 `startBit=7`（在字节0），跨字节时 `byteIdx = 0 - 1 = 255`（uint8_t下溢），读取 `data[255]` 越界！

**修复后的正确算法：**
```cpp
uint8_t byteIdx = signal.startBit / 8;
int8_t bitIdx = static_cast<int8_t>(signal.startBit % 8);
for (uint8_t i = 0; i < signal.bitLength; ++i) {
    raw = (raw << 1) | ((data[byteIdx] >> bitIdx) & 0x01);
    bitIdx--;              // 字节内从高位往低位走
    if (bitIdx < 0) {      // 走到字节最低位后...
        byteIdx++;         // ...跳到下一个字节
        bitIdx = 7;        // ...从最高位继续
    }
}
```

**修复要点：**
- 用 `int8_t` 而非 `uint8_t` 存储bitIdx，允许临时为-1
- 跨字节时 `byteIdx++`（递增），而非 `startByte - n`（递减）
- 大端的字节顺序是从低字节到高字节（data[0]是MSB所在字节），所以跨字节时byteIdx应该递增

**验证：7个测试用例全部通过**
| 测试场景 | startBit | bitLength | 数据 | 期望值 |
|---------|----------|-----------|------|--------|
| 8位完整字节 | 7 | 8 | data[0]=0xAB | 0xAB |
| 4位高半字节 | 7 | 4 | data[0]=0xAB | 0xA |
| 16位跨2字节 | 7 | 16 | data[0]=0x12,data[1]=0x34 | 0x1234 |
| 12位非对齐 | 7 | 12 | data[0]=0x12,data[1]=0x30 | 0x123 |
| 24位跨3字节 | 7 | 24 | data[0..2]=0x11,0x22,0x33 | 0x112233 |
| 全0边界 | 7 | 16 | 全0 | 0x0000 |
| 全1边界 | 7 | 16 | 全0xFF | 0xFFFF |

---

## 2.6 DbcParser DBC文件解析器

**文件：** `src/core/dbc/dbc_parser.h` + `.cpp`

### parse 主解析函数

```cpp
bool DbcParser::parse(const std::string& content) {
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty()) continue;
        if (line.substr(0, 3) == "BO_") parseMessageLine(line);
        else if (line.substr(0, 3) == "SG_") {
            if (m_messages.count(m_currentMsgId))
                parseSignalLine(line, m_messages[m_currentMsgId]);
        }
        else if (line.substr(0, 3) == "CM_") parseCommentLine(line);
    }
    return !m_messages.empty();
}
```

### 设计要点

- DBC文件是行式格式，每行以关键字开头
- `BO_` = 报文，`SG_` = 信号（紧跟在BO_后面，属于该报文）
- `m_currentMsgId` 记录当前解析到哪个报文，SG_行就加到这个报文里
- 这是**状态相关的解析**，SG_行依赖前面最近的BO_行

### parseSignalLine 解析信号行（字符串切割实战）

```cpp
// 格式: SG_ EngineSpeed : 0|16@0+ (1,0) [0|8000] "rpm" TestTool
bool DbcParser::parseSignalLine(const std::string& line, DbcMessage& msg) {
    DbcSignal signal;

    // 1. 提取信号名（冒号前面，跳过"SG_"）
    size_t colonPos = line.find(':');
    signal.name = trim(line.substr(3, colonPos - 3));
    std::string rest = trim(line.substr(colonPos + 1));

    // 2. 解析 0|16@0+
    size_t atPos = rest.find('@');
    std::string bitInfo = rest.substr(0, atPos);  // "0|16"
    size_t pipePos = bitInfo.find('|');
    signal.startBit = std::stoul(bitInfo.substr(0, pipePos));     // 0
    signal.bitLength = std::stoul(bitInfo.substr(pipePos + 1));   // 16
    signal.byteOrder = (rest[atPos + 1] == '0') ? Motorola : Intel;
    signal.valueType = (rest[atPos + 2] == '-') ? Signed : Unsigned;

    // 3. 解析 (factor,offset)
    size_t p1 = rest.find('('), p2 = rest.find(')');
    std::string fo = rest.substr(p1 + 1, p2 - p1 - 1);
    size_t comma = fo.find(',');
    signal.factor = std::stod(trim(fo.substr(0, comma)));
    signal.offset = std::stod(trim(fo.substr(comma + 1)));

    // 4. 解析 [min|max] — 用find('[') find(']') 定位，再find('|')分割
    // 5. 解析 "unit" — 用find('"') 定位两个引号

    msg.signalList.push_back(signal);
    return true;
}
```

### 知识点

- `line.find(':')` — 查找字符位置，找不到返回 `string::npos`
- `line.substr(pos, len)` — 截取子串
- `std::stoul` — string to unsigned long，字符串转无符号长整型
- `std::stod` — string to double，字符串转双精度浮点数
- 整个解析就是**字符串切割**：用find定位分隔符，用substr截取，用stoul/stod转数值

### trim 去空白函数

```cpp
std::string DbcParser::trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(*start)) start++;  // 跳过开头空白
    auto end = s.end();
    do { end--; } while (std::distance(start, end) > 0 && std::isspace(*end));  // 跳过末尾空白
    return std::string(start, end + 1);  // 用迭代器构造新字符串
}
```
- 用**迭代器**操作，不修改原字符串
- `std::isspace` — 判断是否空白字符（空格、tab、换行等）
- `std::distance(start, end)` — 两个迭代器之间的距离

---

## v1.1 更新补充：DBC VAL_ 值表解析

### 为什么需要值表？

很多CAN信号是**枚举类型**，例如：
- `EngineState`: 0=Off, 1=Running, 2=Fault, 3=Cranking
- `Gear`: 0=P, 1=R, 2=N, 3=D, 4=S

DBC文件用 `VAL_` 行定义这些枚举值的描述。原始实现只解析了 `BO_` 和 `SG_`，忽略了 `VAL_`，导致枚举信号只能显示数字，无法显示人类可读的描述文本。

### VAL_ 行格式

```
VAL_ messageId signalName value1 "description1" value2 "description2" ... ;
```

**示例：**
```
VAL_ 100 EngineState 0 "Off" 1 "Running" 2 "Fault" 3 "Cranking" ;
VAL_ 200 Gear 0 "P" 1 "R" 2 "N" 3 "D" 4 "S" ;
```

### DbcSignal 新增成员

```cpp
struct DbcSignal {
    // ... 原有成员 ...
    std::map<int64_t, std::string> valueDescriptions;  // 原始值 -> 描述文本

    std::string getValueDescription(int64_t rawValue) const {
        auto it = valueDescriptions.find(rawValue);
        return (it != valueDescriptions.end()) ? it->second : std::string();
    }
    bool hasValueTable() const { return !valueDescriptions.empty(); }
};
```

### parseValueTableLine 解析实现

```cpp
bool DbcParser::parseValueTableLine(const std::string& line) {
    std::string content = line;

    // 1. 去掉开头的 "VAL_"
    size_t valPos = content.find("VAL_");
    content = content.substr(valPos + 4);

    // 2. 去掉末尾的分号
    size_t semicolon = content.find_last_of(';');
    if (semicolon != std::string::npos) content = content.substr(0, semicolon);

    // 3. 提取 messageId
    size_t spacePos = content.find(' ');
    uint32_t msgId = std::stoul(trim(content.substr(0, spacePos)));
    content = trim(content.substr(spacePos + 1));

    // 4. 提取 signalName
    spacePos = content.find(' ');
    std::string signalName = trim(content.substr(0, spacePos));
    content = trim(content.substr(spacePos + 1));

    // 5. 找到对应的message和signal
    auto msgIt = m_messages.find(msgId);
    DbcSignal* targetSignal = nullptr;
    for (auto& sig : msgIt->second.signalList) {
        if (sig.name == signalName) { targetSignal = &sig; break; }
    }

    // 6. 循环解析 value "description" 对
    while (!content.empty()) {
        spacePos = content.find(' ');
        std::string valueStr = trim(content.substr(0, spacePos));
        content = trim(content.substr(spacePos + 1));

        size_t quoteStart = content.find('"');
        size_t quoteEnd = content.find('"', quoteStart + 1);
        std::string desc = content.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        content = trim(content.substr(quoteEnd + 1));

        int64_t value = std::stoll(valueStr);
        targetSignal->valueDescriptions[value] = desc;
    }
    return true;
}
```

**解析要点：**
- `VAL_` 行通常出现在 `BO_` 和 `SG_` 之后，所以按顺序解析时信号已存在
- value和description交替出现，用引号区分描述文本
- 描述文本中可能包含空格，所以不能简单按空格分割，必须用引号定位
- `std::stoll` 解析有符号64位整数（支持负数枚举值）

### DbcMessage 便捷查询方法

```cpp
std::string getSignalValueDescription(const std::string& signalName, int64_t rawValue) const {
    const DbcSignal* sig = findSignal(signalName);
    return sig ? sig->getValueDescription(rawValue) : std::string();
}
```

### 在主解析循环中注册

```cpp
if (line.substr(0, 4) == "VAL_") {
    parseValueTableLine(line);
}
```

**设计理念：值表与信号解耦**
- `VAL_` 行是独立的行，不紧跟在 `SG_` 后面
- 解析时通过 `messageId + signalName` 二次查找，关联到已解析的信号
- 这是**两阶段解析**的典型模式：先解析结构（BO_/SG_），再补充元数据（VAL_/CM_/BA_）
