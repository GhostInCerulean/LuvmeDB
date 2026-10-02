#pragma once
#include <filesystem>

#include "pch.hpp"

namespace luvme {
    constexpr usize kPageSize = 4096;
    constexpr u32 kMetaMagic = 0x4C55564D; // "LUVM"
    using PageBuf = array<u8, kPageSize>;
    using PageId = u32;
    constexpr PageId kNoPage = 0; // page 0 is the meta page so it will never be a data page

    /// Wrapper class for physical pages, this works directly with the FS and is the lowest layer
    class PhysicalPages {
        // for some reason hardware people named 'access anywhere' as 'random access'
        asio::random_access_file file;
        PageId pageCount = 0;
        PageId root = kNoPage;
        bool metaDirty = false;
    public:
        static awaitable<PhysicalPages> Open(asio::any_io_executor executor, const filesystem::path& path);

        awaitable<void> ReadPage(PageId pageId, span<u8> out);
        awaitable<void> WritePage(PageId pageId, span<const u8> in);

        awaitable<PageId> AllocatePage();
        awaitable<void> Sync();

        PageId GetRoot() const;

        void SetRoot(PageId pageId);

        PageId GetPageCount() const;

    private:
        explicit PhysicalPages(asio::random_access_file&& f);

        awaitable<void> WriteMeta();
        static usize Offset(PageId pageId);
    };
} // luvme
