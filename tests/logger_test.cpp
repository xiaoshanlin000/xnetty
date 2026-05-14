#include "xnetty/common/logger.h"

#include <gtest/gtest.h>

using namespace xnetty;

TEST(LoggerTest, ShouldLog) {
    auto &logger = Logger::instance();
    logger.setLevel(LogLevel::INFO);

    EXPECT_FALSE(logger.shouldLog(LogLevel::TRACE));
    EXPECT_FALSE(logger.shouldLog(LogLevel::DEBUG));
    EXPECT_TRUE(logger.shouldLog(LogLevel::INFO));
    EXPECT_TRUE(logger.shouldLog(LogLevel::WARN));
    EXPECT_TRUE(logger.shouldLog(LogLevel::ERROR));
}

TEST(LoggerTest, SetLevel) {
    auto &logger = Logger::instance();
    logger.setLevel(LogLevel::TRACE);
    EXPECT_TRUE(logger.shouldLog(LogLevel::TRACE));

    logger.setLevel(LogLevel::ERROR);
    EXPECT_FALSE(logger.shouldLog(LogLevel::INFO));
    EXPECT_TRUE(logger.shouldLog(LogLevel::ERROR));
}

TEST(LoggerTest, MacrosCompile) {
    XNETTY_TRACE("trace message " << 1);
    XNETTY_DEBUG("debug message " << 2.5);
    XNETTY_INFO("info message " << "string");
    XNETTY_WARN("warn message " << 42);
    XNETTY_ERROR("error message " << 0xDEAD);
}

class CollectingHandler : public LogHandler {
   public:
    std::string lastMessage;

    void log(LogLevel level, const char *file, int line, const char *func, const std::string &message) override {
        lastMessage = message;
        lastLevel = level;
    }

    LogLevel lastLevel = LogLevel::TRACE;
};

TEST(LoggerTest, CustomHandler) {
    auto handler = std::make_shared<CollectingHandler>();
    auto &logger = Logger::instance();
    auto prevHandler = handler;

    logger.setHandler(handler);
    logger.setLevel(LogLevel::DEBUG);

    XNETTY_DEBUG("test message");
    EXPECT_EQ(handler->lastMessage, "test message");
    EXPECT_EQ(handler->lastLevel, LogLevel::DEBUG);

    logger.setHandler(nullptr);
}
