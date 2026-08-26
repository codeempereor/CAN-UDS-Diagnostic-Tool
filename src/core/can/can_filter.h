#ifndef CAN_FILTER_H
#define CAN_FILTER_H

#include "can/can_frame.h"
#include <vector>
#include <cstdint>

enum class FilterMode {
    Whitelist,
    Blacklist
};

struct FilterRule {
    uint32_t id = 0;
    uint32_t mask = 0x7FF;
    bool extended = false;

    bool match(const CanFrame& frame) const {
        if (extended != frame.extended) return false;
        return (frame.id & mask) == (id & mask);
    }
};

class CanFilter {
public:
    CanFilter();

    void setMode(FilterMode mode);
    FilterMode mode() const;

    void addRule(const FilterRule& rule);
    void removeRule(size_t index);
    void clearRules();
    size_t ruleCount() const;
    const FilterRule& ruleAt(size_t index) const;

    bool pass(const CanFrame& frame) const;

private:
    FilterMode m_mode;
    std::vector<FilterRule> m_rules;
};

#endif // CAN_FILTER_H
