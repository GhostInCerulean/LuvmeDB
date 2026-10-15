#include "page_allocator.hpp"

#include "storage/page_view.hpp"
#include "utils/memory/bytes.hpp"

namespace luvme {
    namespace {
        constexpr usize kMagicOff = 0;
        constexpr usize kPageSizeOff = 4;
        constexpr usize kFreeHeadOff = 8;
        constexpr usize kFreeCountOff = 12;
        constexpr usize kRootOff = 16;
    }

    PageAllocator::PageAllocator(PhysicalPages &pages) : pages(pages) {}

    awaitable<PageAllocator> PageAllocator::Open(PhysicalPages &pages) {
        auto alloc = PageAllocator(pages);

        if (pages.GetPageCount() == 0) {
            // brand new file: page 0 becomes the meta page
            co_await pages.Extend();
            co_await alloc.WriteMeta();
            co_return std::move(alloc);
        }

        PageBuf meta{};
        co_await pages.ReadPage(0, meta);
        if (Load<u32>(meta, kMagicOff) != kMetaMagic
            || Load<u32>(meta, kPageSizeOff) != kPageSize) {
            throw runtime_error("Corrupted pages file");
        }
        alloc.freeHead = Load<u32>(meta, kFreeHeadOff);
        alloc.freeCount = Load<u32>(meta, kFreeCountOff);
        alloc.root = Load<u32>(meta, kRootOff);
        co_return std::move(alloc);
    }

    awaitable<PageId> PageAllocator::Allocate() {
        if (freeHead == kNoPage) {
            co_return co_await pages.Extend();
        }

        PageId id = freeHead;
        PageBuf buf{};
        co_await pages.ReadPage(id, buf);

        PageView view{span<u8>(buf)};
        if (view.Type() != PageType::Free) {
            throw runtime_error("Corrupted free list");
        }

        freeHead = view.Link();
        freeCount--;
        metaDirty = true;
        co_return id;
    }

    awaitable<void> PageAllocator::Free(PageId id) {
        if (id == kNoPage || id >= pages.GetPageCount()) {
            throw invalid_argument("Free: invalid page id");
        }

        PageBuf buf{};
        PageView view{span<u8>(buf)};
        view.Init(PageType::Free);
        view.SetLink(freeHead);
        co_await pages.WritePage(id, buf);

        // only touch the in-memory state once the page is actually written
        freeHead = id;
        freeCount++;
        metaDirty = true;
    }

    awaitable<void> PageAllocator::Flush() {
        if (!metaDirty) co_return;
        co_await pages.Sync();   // free pages first
        co_await WriteMeta();    // then what points at them
        co_await pages.Sync();
    }

    awaitable<void> PageAllocator::WriteMeta() {
        PageBuf meta{};
        Store(meta, kMagicOff, kMetaMagic);
        Store(meta, kPageSizeOff, u32(kPageSize));
        Store(meta, kFreeHeadOff, u32(freeHead));
        Store(meta, kFreeCountOff, u32(freeCount));
        Store(meta, kRootOff, u32(root));
        co_await pages.WritePage(0, meta);
        metaDirty = false;
    }

    PageId PageAllocator::GetRoot() const { return root; }
    void PageAllocator::SetRoot(PageId pageId) { root = pageId; metaDirty = true; }
    u32 PageAllocator::FreeCount() const { return freeCount; }
} // luvme