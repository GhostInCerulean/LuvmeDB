#pragma once
#include "pch.hpp"
#include "quill/Logger.h"

namespace luvme {
enum class LogType
{
    Console,
    File,
};

quill::Logger* NewLogger(string_view name, LogType type, quill::LogLevel logLevel);
}