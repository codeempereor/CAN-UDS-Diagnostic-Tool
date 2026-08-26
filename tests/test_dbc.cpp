#include "dbc/dbc_parser.h"
#include <iostream>
#include <cassert>

const char* TEST_DBC = R"(
VERSION ""

NS_ :
    NS_DESC_
    CM_
    BA_DEF_
    BA_
    VAL_
    CAT_DEF_
    CAT_
    FILTER
    BA_DEF_DEF_
    EV_DATA_
    ENVVAR_DATA_
    SGTYPE_
    SGTYPE_VAL_
    BA_DEF_SGTYPE_
    BA_SGTYPE_
    SIG_TYPE_REF_
    VAL_TABLE_
    SIG_GROUP_
    SIG_VALTYPE_
    SIGTYPE_VALTYPE_
    BO_TX_BU_
    BA_DEF_REL_
    BA_REL_
    BA_DEF_DEF_REL_
    BU_SG_REL_
    BU_EV_REL_
    BU_BO_REL_
    SG_MUL_VAL_
    DIAG_DEF_
    DIAG_ERR_
    GLOBAL_TOKEN_
    MULTI_
    MODULE_
    PDU_
    PDU_TX_BU_
    PDU_SG_
    PDU_TO_FRAME_
    FRAME_RX_
    FRAME_TX_
    SIGNAL_TYPE_
    SIGNAL_GROUP_
    SIGNAL_DEFAULT_VALUE_
    SIGNAL_TYPE_REF_
    SYSTEM_
    SYSTEM_CONSTANT_
    SYSTEM_CONSTANT_VALUE_
    ENUM_
    ENV_VAR_
    ENV_VAR_DATA_
    ENV_VAR_DEF_
    ENV_VAR_VAL_
    ENV_VAR_TARGET_
    ENV_VAR_TYPE_
    I/O_
    SIGNAL_
    SIGNAL_TYPE_
    UNIT_
    BA_DEF_
    BA_
    VAL_
    SIG_VALTYPE_
    SIGTYPE_VALTYPE_
    BO_TX_BU_
    BA_DEF_REL_
    BA_REL_
    BA_DEF_DEF_REL_
    BU_SG_REL_
    BU_EV_REL_
    BU_BO_REL_
    SG_MUL_VAL_
    DIAG_DEF_
    DIAG_ERR_
    GLOBAL_TOKEN_
    MULTI_
    MODULE_
    PDU_
    PDU_TX_BU_
    PDU_SG_
    PDU_TO_FRAME_
    FRAME_RX_
    FRAME_TX_
    SIGNAL_TYPE_
    SIGNAL_GROUP_
    SIGNAL_DEFAULT_VALUE_
    SIGNAL_TYPE_REF_
    SYSTEM_
    SYSTEM_CONSTANT_
    SYSTEM_CONSTANT_VALUE_
    ENUM_
    ENV_VAR_
    ENV_VAR_DATA_
    ENV_VAR_DEF_
    ENV_VAR_VAL_
    ENV_VAR_TARGET_
    ENV_VAR_TYPE_
    I/O_
    SIGNAL_
    SIGNAL_TYPE_
    UNIT_

BS_:

BU_: ECU1 ECU2

BO_ 256 EngineData: 8 ECU1
 SG_ EngineSpeed : 0|16@0+ (1,0) [0|8000] "rpm" ECU2
 SG_ CoolantTemp : 16|16@0+ (0.1,-40) [-40|215] "degC" ECU2
 SG_ OilPressure : 32|8@0+ (0.5,0) [0|100] "bar" ECU2

BO_ 512 VehicleSpeed: 4 ECU1
 SG_ VehicleSpeed : 0|16@1+ (0.01,0) [0|300] "km/h" ECU2
)";

void testDbcParse()
{
    DbcParser parser;
    bool result = parser.loadFromString(TEST_DBC);
    assert(result == true);
    assert(parser.messageCount() == 2);
    std::cout << "[PASS] DBC parse: " << parser.messageCount() << " messages, "
              << parser.signalCount() << " signals" << std::endl;
}

void testSignalExtraction()
{
    DbcParser parser;
    parser.loadFromString(TEST_DBC);

    const DbcMessage* msg = parser.getMessage(256);
    assert(msg != nullptr);
    assert(msg->name == "EngineData");
    assert(msg->signalList.size() == 3);
    std::cout << "[PASS] Message lookup: EngineData" << std::endl;

    // 测试信号查找
    const DbcSignal* sig = msg->findSignal("EngineSpeed");
    assert(sig != nullptr);
    assert(sig->bitLength == 16);
    assert(sig->factor == 1.0);
    assert(sig->offset == 0.0);
    std::cout << "[PASS] Signal lookup: EngineSpeed" << std::endl;
}

void testValueConversion()
{
    DbcParser parser;
    parser.loadFromString(TEST_DBC);

    const DbcMessage* msg = parser.getMessage(256);
    assert(msg != nullptr);

    // 测试原始值转物理值
    uint8_t data[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    double speed = msg->getPhysicalValue(data, 8, "EngineSpeed");
    std::cout << "EngineSpeed = " << speed << " rpm" << std::endl;

    double temp = msg->getPhysicalValue(data, 8, "CoolantTemp");
    std::cout << "CoolantTemp = " << temp << " degC" << std::endl;

    std::cout << "[PASS] Value conversion" << std::endl;
}

// 大端(Motorola)位提取专项测试DBC
const char* BIG_ENDIAN_DBC = R"(
BO_ 100 BigEndianMsg: 8 ECU1
 SG_ Sig8Bit : 7|8@0+ (1,0) [0|255] "" ECU2
 SG_ Sig4BitHigh : 7|4@0+ (1,0) [0|15] "" ECU2
 SG_ Sig16Bit : 7|16@0+ (1,0) [0|65535] "" ECU2
 SG_ Sig12Bit : 7|12@0+ (1,0) [0|4095] "" ECU2
 SG_ Sig3Byte : 7|24@0+ (1,0) [0|16777215] "" ECU2
)";

void testBigEndianExtraction()
{
    DbcParser parser;
    bool ok = parser.loadFromString(BIG_ENDIAN_DBC);
    assert(ok);
    const DbcMessage* msg = parser.getMessage(100);
    assert(msg != nullptr);
    assert(msg->signalList.size() == 5);

    // 测试1: 8位大端信号 startBit=7, bitLength=8
    // 期望: data[0]
    uint8_t data1[8] = {0xAB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint64_t raw1 = msg->extractRawValue(data1, 8, *msg->findSignal("Sig8Bit"));
    std::cout << "8-bit big-endian: raw=0x" << std::hex << raw1 << " (expect 0xAB)" << std::dec << std::endl;
    assert(raw1 == 0xAB);

    // 测试2: 4位大端信号(高4位) startBit=7, bitLength=4
    // 期望: data[0] >> 4 = 0xA
    uint8_t data2[8] = {0xAB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint64_t raw2 = msg->extractRawValue(data2, 8, *msg->findSignal("Sig4BitHigh"));
    std::cout << "4-bit high big-endian: raw=0x" << std::hex << raw2 << " (expect 0xA)" << std::dec << std::endl;
    assert(raw2 == 0xA);

    // 测试3: 16位大端信号 startBit=7, bitLength=16
    // 期望: data[0]<<8 | data[1] = 0x1234
    uint8_t data3[8] = {0x12, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint64_t raw3 = msg->extractRawValue(data3, 8, *msg->findSignal("Sig16Bit"));
    std::cout << "16-bit big-endian: raw=0x" << std::hex << raw3 << " (expect 0x1234)" << std::dec << std::endl;
    assert(raw3 == 0x1234);

    // 测试4: 12位大端信号 startBit=7, bitLength=12
    // 高8位在data[0]，低4位在data[1]的高4位
    // 期望: (data[0]<<4) | (data[1]>>4) = (0x12<<4)|(0x30>>4) = 0x123
    uint8_t data4[8] = {0x12, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint64_t raw4 = msg->extractRawValue(data4, 8, *msg->findSignal("Sig12Bit"));
    std::cout << "12-bit big-endian: raw=0x" << std::hex << raw4 << " (expect 0x123)" << std::dec << std::endl;
    assert(raw4 == 0x123);

    // 测试5: 24位大端信号(3字节) startBit=7, bitLength=24
    // 期望: data[0]<<16 | data[1]<<8 | data[2] = 0x112233
    uint8_t data5[8] = {0x11, 0x22, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint64_t raw5 = msg->extractRawValue(data5, 8, *msg->findSignal("Sig3Byte"));
    std::cout << "24-bit big-endian: raw=0x" << std::hex << raw5 << " (expect 0x112233)" << std::dec << std::endl;
    assert(raw5 == 0x112233);

    // 测试6: 全0数据
    uint8_t data0[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint64_t raw0 = msg->extractRawValue(data0, 8, *msg->findSignal("Sig16Bit"));
    assert(raw0 == 0);

    // 测试7: 全1数据
    uint8_t dataFF[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint64_t rawFF = msg->extractRawValue(dataFF, 8, *msg->findSignal("Sig16Bit"));
    assert(rawFF == 0xFFFF);

    std::cout << "[PASS] Big-endian bit extraction (7 test cases)" << std::endl;
}

// VAL_值表解析测试
const char* VAL_TABLE_DBC = R"(
BO_ 200 StatusMsg: 8 ECU1
 SG_ EngineState : 0|8@0+ (1,0) [0|255] "" ECU2
 SG_ Gear : 8|8@0+ (1,0) [0|255] "" ECU2

VAL_ 200 EngineState 0 "Off" 1 "Running" 2 "Fault" 3 "Cranking" ;
VAL_ 200 Gear 0 "P" 1 "R" 2 "N" 3 "D" 4 "S" ;
)";

void testValueTableParsing()
{
    DbcParser parser;
    bool ok = parser.loadFromString(VAL_TABLE_DBC);
    assert(ok);
    const DbcMessage* msg = parser.getMessage(200);
    assert(msg != nullptr);

    const DbcSignal* engineState = msg->findSignal("EngineState");
    assert(engineState != nullptr);
    assert(engineState->hasValueTable());
    assert(engineState->valueDescriptions.size() == 4);
    assert(engineState->getValueDescription(0) == "Off");
    assert(engineState->getValueDescription(1) == "Running");
    assert(engineState->getValueDescription(2) == "Fault");
    assert(engineState->getValueDescription(3) == "Cranking");
    assert(engineState->getValueDescription(99).empty());  // 不存在的值返回空

    const DbcSignal* gear = msg->findSignal("Gear");
    assert(gear != nullptr);
    assert(gear->hasValueTable());
    assert(gear->valueDescriptions.size() == 5);
    assert(gear->getValueDescription(0) == "P");
    assert(gear->getValueDescription(3) == "D");

    // 测试DbcMessage的便捷方法
    assert(msg->getSignalValueDescription("EngineState", 1) == "Running");
    assert(msg->getSignalValueDescription("Gear", 4) == "S");

    std::cout << "[PASS] VAL_ value table parsing" << std::endl;
}

int main()
{
    std::cout << "=== DBC Parser Tests ===" << std::endl;

    testDbcParse();
    testSignalExtraction();
    testValueConversion();
    testBigEndianExtraction();
    testValueTableParsing();

    std::cout << "\nAll DBC tests passed!" << std::endl;
    return 0;
}
