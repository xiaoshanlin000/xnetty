// MIT License
//
// Copyright (c) 2026 xiaoshanlin000
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once

// Bootstrap
#include "xnetty/bootstrap/server_bootstrap.h"

// Channel + Pipeline
#include "xnetty/channel/channel_pipeline.h"
#include "xnetty/channel/context.h"
#include "xnetty/channel/handler.h"

// HTTP
#include "xnetty/http/compressor_handler.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_request.h"
#include "xnetty/http/http_response.h"
#include "xnetty/http/http_server_handler.h"
#include "xnetty/http/http_status.h"
#include "xnetty/http/router.h"

// Buffer
#include "xnetty/buffer/byte_buf.h"

// Common
#include "xnetty/common/logger.h"

// SSL
#include "xnetty/ssl/ssl_handler.h"

// Util
#include "xnetty/util/gzip.h"

// WebSocket
#include "xnetty/websocket/topic_tree.h"
#include "xnetty/websocket/web_socket.h"
#include "xnetty/websocket/websocket_codec.h"
#include "xnetty/websocket/websocket_handler.h"
#include "xnetty/websocket/ws_upgrade_handler.h"
