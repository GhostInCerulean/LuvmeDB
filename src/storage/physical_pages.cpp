#include "physical_pages.hpp"

#include "utils/memory/bytes.hpp"


namespace luvme {
    awaitable<PhysicalPages> PhysicalPages::Open(asio::any_io_executor executor, const filesystem::path &path) {
        auto pages = PhysicalPages(asio::random_access_file(executor, path, asio::file_base::read_write | asio::file_base::create));

        // if the file is empty, write the meta page
        auto size = pages.file.size();
        if (size == 0) {
            pages.pageCount = 1;
            co_await pages.WriteMeta();
            co_return std::move(pages);
        }

        auto metaBuffer = PageBuf();
        co_await pages.ReadPage(0, metaBuffer);

        if (Load<u32>(metaBuffer, 0) != kMetaMagic
            || Load<u32>(metaBuffer, 4) != kPageSize) {
            throw runtime_error("Corrupted pages file");
        }

        pages.pageCount = PageId(size / kPageSize);
        pages.root = Load<u32>(metaBuffer, 12);
        co_return std::move(pages);
    }


    awaitable<void> PhysicalPages::WriteMeta() {
        // TODO make and use an util function like MakeBuf<SIZE>(a, b, c, d)
        auto metaBuffer = PageBuf();
        Store(metaBuffer, 0, kMetaMagic);
        Store(metaBuffer, 4, kPageSize);
        Store(metaBuffer, 8, pageCount);
        Store(metaBuffer, 12, root);

        co_await WritePage(0, metaBuffer);
        metaDirty = false;
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

    awaitable<PageId> PhysicalPages::AllocatePage() {
    }

    awaitable<void> PhysicalPages::Sync() {
    }

    PageId PhysicalPages::GetRoot() const { return root; }

    void PhysicalPages::SetRoot(PageId pageId) { root = pageId; metaDirty = true; }

    PageId PhysicalPages::GetPageCount() const { return pageCount; }

    PhysicalPages::PhysicalPages(asio::random_access_file &&f): file(std::move(f)) {}
} // luvme