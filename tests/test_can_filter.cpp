#include "can/can_filter.h"
#include <iostream>
#include <cassert>

int main()
{
    CanFilter filter;

    // 测试默认模式（黑名单，空规则全部通过）
    CanFrame frame1(0x123, {0x01, 0x02});
    assert(filter.pass(frame1) == true);
    std::cout << "[PASS] Default filter passes all frames" << std::endl;

    // 测试白名单模式
    filter.setMode(FilterMode::Whitelist);
    FilterRule rule;
    rule.id = 0x123;
    rule.mask = 0x7FF;
    filter.addRule(rule);

    CanFrame frame2(0x123, {0x01});
    CanFrame frame3(0x456, {0x01});
    assert(filter.pass(frame2) == true);
    assert(filter.pass(frame3) == false);
    std::cout << "[PASS] Whitelist filter works correctly" << std::endl;

    // 测试黑名单模式
    filter.setMode(FilterMode::Blacklist);
    assert(filter.pass(frame2) == false);
    assert(filter.pass(frame3) == true);
    std::cout << "[PASS] Blacklist filter works correctly" << std::endl;

    // 测试掩码过滤
    filter.clearRules();
    filter.setMode(FilterMode::Whitelist);
    FilterRule rule2;
    rule2.id = 0x100;
    rule2.mask = 0xF00;
    filter.addRule(rule2);

    CanFrame frame4(0x123, {0x01});
    CanFrame frame5(0x200, {0x01});
    assert(filter.pass(frame4) == true);
    assert(filter.pass(frame5) == false);
    std::cout << "[PASS] Mask-based filter works correctly" << std::endl;

    std::cout << "\nAll CAN filter tests passed!" << std::endl;
    return 0;
}
