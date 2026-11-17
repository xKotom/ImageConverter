#pragma once
// Типизированные ошибки без исключений на границе фоновых задач.
#include <string>
#include <utility>
#include <variant>

namespace uic {

enum class ErrorCode {
    InvalidInput, NotFound, AccessDenied, UnsupportedFormat, CorruptedFile,
    TooLarge, NoDecoder, NoEncoder, EncodeFailed, VerifyFailed, IoError,
    AlreadyExists, Cancelled, Internal
};

struct Error {
    ErrorCode code = ErrorCode::Internal;
    std::string message; // UTF-8, для показа пользователю
};

template <class T>
class Result {
public:
    Result(T value) : v_(std::move(value)) {}
    Result(Error error) : v_(std::move(error)) {}
    bool ok() const { return v_.index() == 0; }
    explicit operator bool() const { return ok(); }
    T& value() { return std::get<0>(v_); }
    const T& value() const { return std::get<0>(v_); }
    const Error& error() const { return std::get<1>(v_); }
private:
    std::variant<T, Error> v_;
};

struct Ok {};
using Status = Result<Ok>;

inline Error makeError(ErrorCode c, std::string m) { return Error{c, std::move(m)}; }

} // namespace uic
