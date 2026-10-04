#pragma once
#include "pch.hpp"
#include "physical_pages.hpp"
#include "utils/memory/bytes.hpp"

namespace luvme {
    enum class PageType : u8 {
        Free = 0,
        Leaf = 1,
        Internal = 2,
        Overflow = 3
    };

    /// Non owning view of a physical page
    /// Layout (4096 bytes):
    ///
    /// [16 bytes of header][ slot0, slot1 ... -> free space <- ... cell1, cell0]
    ///
    /// header:
    ///     0 u32 checksum
    ///     4 u8  type (PageType)
    ///     5 u8  flags
    ///     6 u16 cellCount
    ///     8 u16 cellsStart: offset of lowest cell; cells grow downward
    ///    10 u16 fragmented: dead bytes between cells, reclaimed by compaction
    ///    12 u32 link (leaf: next sibling, internal: rightmost child, overflow: next overflow page)
    class PageView {
        static constexpr usize kChecksum = 0;
        static constexpr usize kType = 4;
        static constexpr usize kFlags = 5;
        static constexpr usize kCellCount = 6;
        static constexpr usize kCellsStart = 8;
        static constexpr usize kFragmented = 10;
        static constexpr usize kLink = 12;
        static constexpr usize kHeaderSize = 16;
        static constexpr usize kSlotSize = 4;
        // we keep >= 4 cells per page so B-tree split always works
        static constexpr usize kMaxCell = (kPageSize - kHeaderSize) / 4 - kSlotSize;

        span<u8> buffer;
    public:
        explicit PageView(span<u8> data);
        void Init(PageType type);

        PageType Type() const;
        u16 CellCount() const;
        PageId Link() const;
        void SetLink(PageId link);

        usize FreeContiguous() const;
        usize FreeTotal() const;

        bool CanFit(usize length) const;

        span<const u8> Cell(u16 i) const;

        bool Insert(u16 i, span<const u8> cell);
        void Remove(u16 i);
        void Compact();
    private:
        u16 CellsStart() const;
        usize SlotsEnd() const;

        pair<u16, u16> Slot(u16 i) const;
        void SetSlot(u16 i, u16 offset, u16 len);

        template<typename T> T Read(usize offset) const;
        template<typename T> void Write(usize offset, T value);
    };

    template<typename T>
    T PageView::Read(usize offset) const {
        return Load<T>(buffer, offset);
    }

    template<typename T>
    void PageView::Write(usize offset, T value) {
        Store<T>(buffer, offset, value);
    }
} // luvme
