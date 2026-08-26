#include "can/can_filter.h"

CanFilter::CanFilter()
    : m_mode(FilterMode::Blacklist)
{
}

void CanFilter::setMode(FilterMode mode)
{
    m_mode = mode;
}

FilterMode CanFilter::mode() const
{
    return m_mode;
}

void CanFilter::addRule(const FilterRule& rule)
{
    m_rules.push_back(rule);
}

void CanFilter::removeRule(size_t index)
{
    if (index < m_rules.size()) {
        m_rules.erase(m_rules.begin() + index);
    }
}

void CanFilter::clearRules()
{
    m_rules.clear();
}

size_t CanFilter::ruleCount() const
{
    return m_rules.size();
}

const FilterRule& CanFilter::ruleAt(size_t index) const
{
    return m_rules[index];
}

bool CanFilter::pass(const CanFrame& frame) const
{
    if (m_rules.empty()) {
        return true;
    }

    bool matched = false;
    for (const auto& rule : m_rules) {
        if (rule.match(frame)) {
            matched = true;
            break;
        }
    }

    if (m_mode == FilterMode::Whitelist) {
        return matched;
    } else {
        return !matched;
    }
}
