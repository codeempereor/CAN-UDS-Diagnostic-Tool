#ifndef CAN_ADAPTER_BASE_H
#define CAN_ADAPTER_BASE_H

#include "can/can_frame.h"
#include <functional>
#include <string>
#include <cstdint>

enum class CanAdapterState {
    Closed,
    Opening,
    Open,
    Error
};

class CanAdapterBase {
public:
    using ReceiveCallback = std::function<void(const CanFrame& frame)>;
    using ErrorCallback = std::function<void(const std::string& error)>;
    using StateChangedCallback = std::function<void(CanAdapterState state)>;

    CanAdapterBase();
    virtual ~CanAdapterBase();

    virtual bool open(const std::string& interface, uint32_t bitrate = 500000) = 0;
    virtual void close() = 0;
    virtual bool sendFrame(const CanFrame& frame) = 0;

    virtual bool isOpen() const = 0;
    virtual std::string adapterName() const = 0;

    void setReceiveCallback(ReceiveCallback callback);
    void setErrorCallback(ErrorCallback callback);
    void setStateChangedCallback(StateChangedCallback callback);

    CanAdapterState state() const;

protected:
    void notifyReceived(const CanFrame& frame);
    void notifyError(const std::string& error);
    void setState(CanAdapterState state);

    ReceiveCallback m_receiveCallback;
    ErrorCallback m_errorCallback;
    StateChangedCallback m_stateCallback;
    CanAdapterState m_state;
};

#endif // CAN_ADAPTER_BASE_H
