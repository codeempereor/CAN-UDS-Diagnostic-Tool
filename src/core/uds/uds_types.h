#ifndef UDS_TYPES_H
#define UDS_TYPES_H

#include <cstdint>
#include <vector>
#include <string>

// UDS 服务ID
enum class UdsService : uint8_t {
    DiagnosticSessionControl = 0x10,
    EcuReset = 0x11,
    SecurityAccess = 0x27,
    CommunicationControl = 0x28,
    ReadDataByIdentifier = 0x22,
    ReadMemoryByAddress = 0x23,
    WriteDataByIdentifier = 0x2E,
    WriteMemoryByAddress = 0x3D,
    RoutineControl = 0x31,
    RequestDownload = 0x34,
    RequestUpload = 0x35,
    TransferData = 0x36,
    RequestTransferExit = 0x37,
    TesterPresent = 0x3E,
    ControlDtcSettings = 0x85,
    ReadDtcInformation = 0x19,
    ClearDiagnosticInformation = 0x14,
};

// 否定响应码
enum class UdsNrc : uint8_t {
    GeneralReject = 0x10,
    ServiceNotSupported = 0x11,
    SubFunctionNotSupported = 0x12,
    IncorrectMessageLengthOrInvalidFormat = 0x13,
    ResponseTooLong = 0x14,
    BusyRepeatRequest = 0x21,
    ConditionsNotCorrect = 0x22,
    RequestSequenceError = 0x24,
    NoResponseFromSubnetComponent = 0x25,
    FailurePreventsExecutionOfRequestedAction = 0x26,
    RequestOutOfRange = 0x31,
    SecurityAccessDenied = 0x33,
    InvalidKey = 0x35,
    ExceedNumberOfAttempts = 0x36,
    RequiredTimeDelayNotExpired = 0x37,
    UploadDownloadNotAccepted = 0x70,
    TransferDataSuspended = 0x71,
    GeneralProgrammingFailure = 0x72,
    WrongBlockSequenceCounter = 0x73,
    RequestCorrectlyReceivedResponsePending = 0x78,
    SubFunctionNotSupportedInActiveSession = 0x7E,
    ServiceNotSupportedInActiveSession = 0x7F,
    VoltageTooHigh = 0x92,
    VoltageTooLow = 0x93,
};

// 会话类型
enum class UdsSessionType : uint8_t {
    DefaultSession = 0x01,
    ProgrammingSession = 0x02,
    ExtendedDiagnosticSession = 0x03,
    SafetySystemDiagnosticSession = 0x04,
};

// 例程控制类型
enum class RoutineControlType : uint8_t {
    StartRoutine = 0x01,
    StopRoutine = 0x02,
    RequestRoutineResults = 0x03,
};

struct UdsResponse {
    bool success = false;
    uint8_t serviceId = 0;
    std::vector<uint8_t> data;
    uint8_t nrc = 0;

    bool isPositive() const { return success; }
    bool isNegative() const { return !success; }

    std::string nrcDescription() const {
        switch (static_cast<UdsNrc>(nrc)) {
        case UdsNrc::GeneralReject: return "General reject";
        case UdsNrc::ServiceNotSupported: return "Service not supported";
        case UdsNrc::SubFunctionNotSupported: return "Sub-function not supported";
        case UdsNrc::IncorrectMessageLengthOrInvalidFormat: return "Incorrect message length or invalid format";
        case UdsNrc::ResponseTooLong: return "Response too long";
        case UdsNrc::BusyRepeatRequest: return "Busy, repeat request";
        case UdsNrc::ConditionsNotCorrect: return "Conditions not correct";
        case UdsNrc::RequestSequenceError: return "Request sequence error";
        case UdsNrc::RequestOutOfRange: return "Request out of range";
        case UdsNrc::SecurityAccessDenied: return "Security access denied";
        case UdsNrc::InvalidKey: return "Invalid key";
        case UdsNrc::ExceedNumberOfAttempts: return "Exceed number of attempts";
        case UdsNrc::RequiredTimeDelayNotExpired: return "Required time delay not expired";
        case UdsNrc::RequestCorrectlyReceivedResponsePending: return "Request correctly received, response pending";
        case UdsNrc::SubFunctionNotSupportedInActiveSession: return "Sub-function not supported in active session";
        case UdsNrc::ServiceNotSupportedInActiveSession: return "Service not supported in active session";
        default: return "Unknown NRC: 0x" + std::to_string(nrc);
        }
    }
};

#endif // UDS_TYPES_H
