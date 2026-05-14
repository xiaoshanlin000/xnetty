#include "xnetty/common/error.h"

#include <cstring>
#include <string>

namespace xnetty {

std::string Error::toString() const { return "[" + std::to_string(static_cast<int>(code_)) + "] " + message_; }

Error Error::fromErrno(int savedErrno) { return Error(ErrorCode::IO_ERROR, std::strerror(savedErrno)); }

}  // namespace xnetty
