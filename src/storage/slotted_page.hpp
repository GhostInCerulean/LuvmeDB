// SlottedPage.hpp
#pragma once
#include "pager.hpp"
#include "utils/memory/bytes.hpp"

namespace luvme
{
enum class PageKind : u8
{
    Free = 0,
    Leaf = 1,
    Internal = 2,
};

// A non-owning view over one page buffer.
class SlottedPage
{
    static constexpr usize kHeader = 12;
    span<u8> buf;

  public:
    explicit SlottedPage(span<u8> b) : buf(b) {}
    void Init(PageKind kind);

    PageKind Kind() const;
    u16 Count() const;
    PageID Link() const;
    void SetLink(PageID id);

    span<u8> Cell(u16 i) const;

    // Returns false (page untouched) if it doesn't fit even after compaction.
    bool Insert(u16 i, span<const u8> payload);
    void Remove(u16 i);

  private:
    u16 CellTop() const;
    u16 Garbage() const;
    u16 SlotOff(u16 i) const;
    usize FreeSpace() const;
    void SetCount(u16 n);
    void SetCellTop(u16 t);
    void SetGarbage(u16 g);

    // Repack live cells against the end of the page, reclaiming garbage.
    void Compact();
};
} // namespace luvme