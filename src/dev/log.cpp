#include "log.hpp"

#include "../../cmake-build-debug/_deps/fmt-src/include/fmt/format.h"
#include "fmt/core.h"
#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/FileSink.h"

namespace luvme
{

quill::Logger *NewLogger(string_view name, LogType type, quill::LogLevel logLevel)
{
    auto backendOptions = quill::BackendOptions();
    quill::Backend::start(backendOptions);

    auto sink = [&] {
        switch (type)
        {
        case LogType::Console:
            return quill::Frontend::create_or_get_sink<quill::ConsoleSink>(name.data());
        case LogType::File:
            auto fileSinkCfg = quill::FileSinkConfig();
            fileSinkCfg.set_open_mode('w');
            fileSinkCfg.set_filename_append_option(quill::FilenameAppendOption::StartDateTime);
            return quill::Frontend::create_or_get_sink<quill::FileSink>(fmt::format("{}.log", name), fileSinkCfg);
        }
        unreachable();
    }();

    auto logger = quill::Frontend::create_or_get_logger(name.data(), std::move(sink));
    logger->set_log_level(logLevel);

#ifndef NDEBUG
    // In debug mode, force immediate flushing of log messages
    // This blocks the caller thread until log entry is written to disk
    // Simulates synchronous logging behavior for easier debugging
    // Note: This can impact performance and should only be used during development
    logger->set_immediate_flush(1u);
#endif

    return logger;
}
} // namespace luvme