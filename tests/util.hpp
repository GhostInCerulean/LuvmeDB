#pragma once
#include <future>
#include <optional>
#include <type_traits>

#include "pch.hpp"

namespace luvme::test {
    /// One io_context (hence one io_uring ring) for the whole test binary.
    inline asio::io_context &SharedIo() {
        static asio::io_context io;
        return io;
    }

    /// Runs a coroutine to completion on `io` and returns its result (rethrows its exception).
    /// T does not need to be default-constructible.
    template<class T>
    T RunSync(asio::io_context &io, awaitable<T> task) {
        if constexpr (std::is_void_v<T>) {
            auto fut = asio::co_spawn(io, std::move(task), asio::use_future);
            io.run();
            io.restart();
            fut.get();
        } else {
            std::optional<T> result;
            auto fut = asio::co_spawn(
                io,
                [&]() -> awaitable<void> { result.emplace(co_await std::move(task)); },
                asio::use_future);
            io.run();
            io.restart();
            fut.get();
            return std::move(*result);
        }
    }
} // luvme::test