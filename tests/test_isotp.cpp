#include "isotp/isotp_client.h"
#include <iostream>
#include <cassert>
#include <vector>

static std::vector<CanFrame> g_sentFrames;
static std::vector<uint8_t> g_receivedData;

void testSingleFrame()
{
    g_sentFrames.clear();
    g_receivedData.clear();

    IsoTpClient client;
    client.setTxId(0x7E0);
    client.setRxId(0x7E8);

    client.setSendFrameCallback([](const CanFrame& frame) {
        g_sentFrames.push_back(frame);
    });

    client.setReceiveCallback([](const std::vector<uint8_t>& data) {
        g_receivedData = data;
    });

    // 发送单帧
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
    bool result = client.sendData(data);
    assert(result == true);
    assert(g_sentFrames.size() == 1);
    assert(g_sentFrames[0].data[0] == 0x05); // SF + 长度
    assert(g_sentFrames[0].data[1] == 0x01);
    std::cout << "[PASS] Single frame send" << std::endl;

    // 接收单帧
    CanFrame rxFrame;
    rxFrame.id = 0x7E8;
    rxFrame.dlc = 3;
    rxFrame.data = {0x02, 0x50, 0x01};
    client.handleReceivedFrame(rxFrame);

    assert(g_receivedData.size() == 2);
    assert(g_receivedData[0] == 0x50);
    assert(g_receivedData[1] == 0x01);
    std::cout << "[PASS] Single frame receive" << std::endl;
}

void testMultiFrame()
{
    g_sentFrames.clear();
    g_receivedData.clear();

    IsoTpClient txClient;
    txClient.setTxId(0x7E0);
    txClient.setRxId(0x7E8);

    IsoTpClient rxClient;
    rxClient.setTxId(0x7E8);
    rxClient.setRxId(0x7E0);

    txClient.setSendFrameCallback([&rxClient](const CanFrame& frame) {
        rxClient.handleReceivedFrame(frame);
    });

    rxClient.setSendFrameCallback([&txClient](const CanFrame& frame) {
        txClient.handleReceivedFrame(frame);
    });

    rxClient.setReceiveCallback([](const std::vector<uint8_t>& data) {
        g_receivedData = data;
    });

    // 发送20字节数据（需要多帧）
    std::vector<uint8_t> data;
    for (uint8_t i = 0; i < 20; i++) {
        data.push_back(i);
    }

    bool result = txClient.sendData(data);
    assert(result == true);

    // 验证接收数据
    assert(g_receivedData.size() == 20);
    for (int i = 0; i < 20; i++) {
        assert(g_receivedData[i] == i);
    }
    std::cout << "[PASS] Multi-frame transfer (20 bytes)" << std::endl;
}

int main()
{
    std::cout << "=== ISO-TP Protocol Tests ===" << std::endl;

    testSingleFrame();
    testMultiFrame();

    std::cout << "\nAll ISO-TP tests passed!" << std::endl;
    return 0;
}
