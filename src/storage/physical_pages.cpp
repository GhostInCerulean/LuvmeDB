#include "physical_pages.hpp"

namespace luvme {
    awaitable<PhysicalPages> PhysicalPages::Open(asio::any_io_executor executor, const filesystem::path &path) {
        auto pages = PhysicalPages(asio::random_access_file(
            executor, path, asio::file_base::read_write | asio::file_base::create));

        auto size = pages.file.size();
        if (size % kPageSize != 0) {
            throw runtime_error("Corrupted pages file: size is not a multiple of the page size");
        }
        
        pages.pageCount = PageId(size / kPageSize);
        co_return std::move(pages);
    }

    usize PhysicalPages::Offset(PageId pageId) {
        return u64(pageId) * kPageSize;
    }

    awaitable<void> PhysicalPages::ReadPage(PageId pageId, span<u8> out) {
        co_await asio::async_read_at(file, Offset(pageId), asio::buffer(out.first(kPageSize)));
    }

    awaitable<void> PhysicalPages::WritePage(PageId pageId, span<const u8> in) {
        co_await asio::async_write_at(file, Offset(pageId), asio::buffer(in.first(kPageSize)));
    }

    awaitable<PageId> PhysicalPages::Extend() {
        // reserve the id before suspending so concurrent callers never get the same page
        PageId id = pageCount++;
        static const PageBuf zero{};
        co_await WritePage(id, zero);
        co_return id;
    }

    awaitable<void> PhysicalPages::Sync() {
        file.sync_all(); // blocking for now
        co_return;
    }

    PageId PhysicalPages::GetPageCount() const { return pageCount; }

    PhysicalPages::PhysicalPages(asio::random_access_file &&f) : file(std::move(f)) {
    }
} // luvme
