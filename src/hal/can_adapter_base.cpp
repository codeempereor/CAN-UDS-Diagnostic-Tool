#include "hal/can_adapter_base.h"

CanAdapterBase::CanAdapterBase()
    : m_state(CanAdapterState::Closed)
{
}

CanAdapterBase::~CanAdapterBase()
{
}

void CanAdapterBase::setReceiveCallback(ReceiveCallback callback)
{
    m_receiveCallback = callback;
}

void CanAdapterBase::setErrorCallback(ErrorCallback callback)
{
    m_errorCallback = callback;
}

void CanAdapterBase::setStateChangedCallback(StateChangedCallback callback)
{
    m_stateCallback = callback;
}

CanAdapterState CanAdapterBase::state() const
{
    return m_state;
}

void CanAdapterBase::notifyReceived(const CanFrame& frame)
{
    if (m_receiveCallback) {
        m_receiveCallback(frame);
    }
}

void CanAdapterBase::notifyError(const std::string& error)
{
    if (m_errorCallback) {
        m_errorCallback(error);
    }
}

void CanAdapterBase::setState(CanAdapterState state)
{
    if (m_state != state) {
        m_state = state;
        if (m_stateCallback) {
            m_stateCallback(state);
        }
    }
}
