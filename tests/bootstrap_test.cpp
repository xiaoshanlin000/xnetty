#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "xnetty/bootstrap/server_bootstrap.h"

using namespace xnetty;

TEST(BootstrapTest, CreateAndShutdown) {
    ServerBootstrap server;
    server.port(19999);
    server.workerThreads(2);

    server.start();
    EXPECT_TRUE(server.isRunning());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    server.shutdownGracefully();
    EXPECT_FALSE(server.isRunning());
}

TEST(BootstrapTest, DefaultConfig) {
    ServerBootstrap server;
    EXPECT_FALSE(server.isRunning());
}
