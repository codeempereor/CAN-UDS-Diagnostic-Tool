#include "uds/uds_client.h"
#include <iostream>
#include <cassert>
#include <vector>

static std::vector<CanFrame> g_sentFrames;
static UdsResponse g_lastResponse;

void testUdsRequestResponse()
{
    g_sentFrames.clear();
    g_lastResponse = UdsResponse();

    UdsClient client;
    client.setTxId(0x7E0);
    client.setRxId(0x7E8);

    client.setSendFrameCallback([](const CanFrame& frame) {
        g_sentFrames.push_back(frame);
    });

    client.setResponseCallback([](const UdsResponse& resp) {
        g_lastResponse = resp;
    });

    // 测试会话控制请求
    bool result = client.diagnosticSessionControl(UdsSessionType::ExtendedDiagnosticSession);
    assert(result == true);
    assert(g_sentFrames.size() == 1);
    assert(g_sentFrames[0].data[1] == 0x10); // 服务ID
    assert(g_sentFrames[0].data[2] == 0x03); // 扩展会话
    std::cout << "[PASS] UDS DiagnosticSessionControl request" << std::endl;

    // 模拟肯定响应
    CanFrame response;
    response.id = 0x7E8;
    response.dlc = 3;
    response.data = {0x02, 0x50, 0x03}; // SF, 0x10+0x40=0x50, session
    client.handleReceivedFrame(response);

    assert(g_lastResponse.success == true);
    assert(g_lastResponse.serviceId == 0x10);
    assert(g_lastResponse.data.size() == 1);
    assert(g_lastResponse.data[0] == 0x03);
    assert(client.currentSession() == UdsSessionType::ExtendedDiagnosticSession);
    std::cout << "[PASS] UDS positive response parsing" << std::endl;
}

void testUdsNegativeResponse()
{
    g_sentFrames.clear();
    g_lastResponse = UdsResponse();

    UdsClient client;
    client.setTxId(0x7E0);
    client.setRxId(0x7E8);

    client.setResponseCallback([](const UdsResponse& resp) {
        g_lastResponse = resp;
    });

    // 模拟否定响应
    CanFrame response;
    response.id = 0x7E8;
    response.dlc = 4;
    response.data = {0x03, 0x7F, 0x10, 0x12}; // SF, NRC, serviceId, subFunctionNotSupported
    client.handleReceivedFrame(response);

    assert(g_lastResponse.success == false);
    assert(g_lastResponse.serviceId == 0x10);
    assert(g_lastResponse.nrc == 0x12);
    std::cout << "[PASS] UDS negative response parsing" << std::endl;
}

void testReadDid()
{
    g_sentFrames.clear();

    UdsClient client;
    client.setTxId(0x7E0);
    client.setRxId(0x7E8);

    client.setSendFrameCallback([](const CanFrame& frame) {
        g_sentFrames.push_back(frame);
    });

    bool result = client.readDataByIdentifier(0xF190);
    assert(result == true);
    assert(g_sentFrames.size() == 1);
    assert(g_sentFrames[0].data[1] == 0x22); // 服务ID
    assert(g_sentFrames[0].data[2] == 0xF1); // DID高字节
    assert(g_sentFrames[0].data[3] == 0x90); // DID低字节
    std::cout << "[PASS] UDS ReadDataByIdentifier request" << std::endl;
}

int main()
{
    std::cout << "=== UDS Protocol Tests ===" << std::endl;

    testUdsRequestResponse();
    testUdsNegativeResponse();
    testReadDid();

    std::cout << "\nAll UDS tests passed!" << std::endl;
    return 0;
}
