#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

TEST(Logger, Can_use_spdlog) {
    spdlog::info("Welcome to spdlog!");
    spdlog::error("Some error message with arg: {}", 1);

    spdlog::warn("Easy padding in numbers like {:08d}", 12);
    spdlog::critical("Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin: {0:b}", 42);
    spdlog::info("Support for floats {:03.2f}", 1.23456);
    spdlog::info("Positional args are {1} {0}..", "too", "supported");
    spdlog::info("{:<30}", "left aligned");

    spdlog::debug("This message should _not_ be displayed..");
    spdlog::set_level(spdlog::level::debug);  // Set global log level to debug
    spdlog::debug("This message should be displayed..\nThis message is second line..");

    // change log pattern
    std::string pattern = std::string("[%H:%M:%S %z] [%n] [0, %t] [%^%l%$] ") + std::string("%v");
    spdlog::set_pattern(pattern);
}

int main(int argc, char* argv[]) {
    int result = 0;

    ::testing::InitGoogleTest(&argc, argv);
    result = RUN_ALL_TESTS();

    return result;
}