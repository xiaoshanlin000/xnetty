#include "xnetty/common/metrics.h"

#include <sstream>

namespace xnetty {

std::string Metrics::toString() const {
    std::ostringstream oss;
    oss << "requests=" << requests() << " bytes_sent=" << bytesSent() << " bytes_recv=" << bytesReceived()
        << " active_conns=" << activeConns() << " errors=" << errors();
    return oss.str();
}

}  // namespace xnetty
