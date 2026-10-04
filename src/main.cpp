#include "dev/log.hpp"
#include "quill/LogMacros.h"

int main(int argc, char *argv[])
{
    auto log1 = luvme::NewLogger("Storage", luvme::LogType::Console, quill::LogLevel::Info);
    auto log2 = luvme::NewLogger("Network", luvme::LogType::Console, quill::LogLevel::Info);

    LOG_INFO(log1, "Initialized");
    LOG_INFO(log2, "Initialized");
    sleep(1);
    LOG_WARNING(log1, "Warning: {}", rand());
    LOG_WARNING(log2, "Warning: {}", rand());
    sleep(1);
}
