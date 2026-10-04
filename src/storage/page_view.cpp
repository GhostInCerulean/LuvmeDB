#include "page_view.hpp"

namespace luvme {
    PageView::PageView(span<u8> data): buffer(data) {}

    void PageView::Init(PageType type) {
        ranges::fill(buffer, u8(0));
        buffer[kType] = u8(type);
        Write(kCellsStart, kPageSize);
    }

    PageType PageView::Type() const {
        return PageType(buffer[kType]);
    }

    u16 PageView::CellCount() const {
        return Read<u16>(kCellCount);
    }

    PageId PageView::Link() const {
        return Load<u32>(buffer, kLink);
    }

    void PageView::SetLink(PageId link) {
        Write(kLink, link);
    }

    usize PageView::FreeContiguous() const {
        return CellsStart() - SlotsEnd();
    }

    usize PageView::FreeTotal() const {
        return FreeContiguous() + Read<u16>(kFragmented);
    }

    bool PageView::CanFit(usize length) const {
        return FreeTotal() >= length + kSlotSize;
    }

    span<const u8> PageView::Cell(u16 i) const {
        auto [off, len] = Slot(i);
        return buffer.subspan(off, len);
    }

    bool PageView::Insert(u16 i, span<const u8> cell) {
        // cases handled as exceptions as it's the caller's responsibility to check for that
        if (i > CellCount())
            throw runtime_error("Index out of bounds");

        if (cell.size() > kMaxCell)
            throw runtime_error("Cell too large");

        if (!CanFit(cell.size()))
            return false; // if it cant fit, caller must split

        // if we must compact to fit
        if (FreeContiguous() < cell.size() + kSlotSize)
            Compact();

        auto offset = CellsStart() - u16(cell.size());
        ranges::copy(cell, buffer.begin() + offset);
        Write(kCellsStart, u16(offset));

        auto slots = buffer.data() + kHeaderSize;
        memmove(
            slots + (i + 1) * kSlotSize,
            slots + i * kSlotSize,
            (CellCount() - i) * kSlotSize
        );
        SetSlot(i, offset, cell.size());
        Write(kCellCount, CellCount() + 1);

        return false;
    }

    void PageView::Remove(u16 i) {
        auto [off, len] = Slot(i);
        auto slots = buffer.data() + kHeaderSize;
        memmove(
            slots + i * kSlotSize,
            slots + (i + 1) * kSlotSize,
            (CellCount() - i - 1) * kSlotSize
        );

        Write(kCellCount, CellCount() - 1);

        // reclaim first cell directly
        if (off == CellsStart())
            Write(kCellsStart, off + len);
        else
            Write(kFragmented, Read<u16>(kFragmented) + len);
    }

    void PageView::Compact() {
        auto tmp = PageBuf();
        auto end = kPageSize;

        for (u16 i = 0; i < CellCount(); i++) {
            auto [off, len] = Slot(i);
            end -= len;
            memcpy(tmp.data() + end, buffer.data() + off, len);
            SetSlot(i, u16(end), len);
        }

        memcpy(buffer.data() + end, tmp.data() + end, kPageSize - end);
        Write(kCellsStart, u16(end));
        Write(kFragmented, 0);
    }

    u16 PageView::CellsStart() const {
        return Read<u16>(kCellsStart);
    }

    usize PageView::SlotsEnd() const {
        return kHeaderSize + CellCount() * kSlotSize;
    }

    pair<u16, u16> PageView::Slot(u16 i) const {
        auto offside = kHeaderSize + i * kSlotSize;
        return {Read<u16>(offside), Read<u16>(offside + 2)};
    }

    void PageView::SetSlot(u16 i, u16 offset, u16 len) {
        auto offside = kHeaderSize + i * kSlotSize;
        Write<u16>(offside, offset);
        Write<u16>(offside + 2, len);
    }
} // luvme