#pragma once
#include <filesystem>

#include "pch.hpp"

namespace luvme {
    constexpr usize kPageSize = 4096;
    using PageBuf = array<u8, kPageSize>;
    using PageId = u32;
    constexpr PageId kNoPage = 0; // page 0 is the meta page so it will never be a data page

    class PhysicalPages {
        // for some reason hardware people named 'access anywhere' as 'random access'
        asio::random_access_file file;
        PageId pageCount = 0;

    public:
        static awaitable<PhysicalPages> Open(asio::any_io_executor executor, const filesystem::path &path);

        awaitable<void> ReadPage(PageId pageId, span<u8> out);
        awaitable<void> WritePage(PageId pageId, span<const u8> in);

        awaitable<PageId> Extend();
        awaitable<void> Sync();

        PageId GetPageCount() const;

    private:
        explicit PhysicalPages(asio::random_access_file &&f);

        static usize Offset(PageId pageId);
    };
} // luvme