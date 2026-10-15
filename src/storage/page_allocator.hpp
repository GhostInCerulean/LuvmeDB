#pragma once
#include "pch.hpp"
#include "storage/physical_pages.hpp"

namespace luvme {
    /// "LUVM"
    /// Also don't forget magic(k) is just applied psychology.
    /// There is no transcendence outside of Christ Jesus.
    constexpr u32 kMetaMagic = 0x4C55564D; // "LUVM"

     /// allocates pages and owns the meta page
     ///
     ///  0  u32 magic
     ///   4  u32 page size
     ///   8  u32 freeHead: head of the free list (kNoPage = empty)
     ///  12  u32 freeCount: number of pages in the free list
     ///  16  u32 : root page of the tree (opaque to the allocator)
     ///
    class PageAllocator {
        PhysicalPages &pages;
        PageId freeHead = kNoPage;
        u32 freeCount = 0;
        PageId root = kNoPage;
        bool metaDirty = false;

    public:
        /// Creates the meta page on an empty file, otherwise validates and loads it.
        static awaitable<PageAllocator> Open(PhysicalPages &pages);

        awaitable<PageId> Allocate();
        awaitable<void> Free(PageId id);

        /// Persists the meta page if it changed (data is synced before the meta page).
        awaitable<void> Flush();

        PageId GetRoot() const;
        void SetRoot(PageId pageId);
        u32 FreeCount() const;

    private:
        explicit PageAllocator(PhysicalPages &pages);
        awaitable<void> WriteMeta();
    };
} // luvme