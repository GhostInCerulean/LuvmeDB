#pragma once
#include "pager.hpp"
#include "pch.hpp"

namespace luvme
{
class SlottedPage
{
    // header is page_id u32 | num_slots u16 | free_end u16
    // slot is   offset u16 | length u16 with offset 0 = tombstone
    static constexpr usize kHeaderSize = 8;
    static constexpr usize kSlotSize = 4;
    static constexpr usize kNumSlotsPos = 4;
    static constexpr usize kFreeEndPos = 6;

    u8 *page;

  public:
    explicit SlottedPage(u8 *page);

    void Init(PageID id);

    u16 NumSlots() const;
    u16 FreeEnd() const;

    usize ContiguousFree() const;
    usize TotalFree() const;

    optional<u16> Insert(span<const u8> rec);
    optional<span<const u8>> Get(u16 slot) const;
    bool Erase(u16 slot);

  private:
    template <class T> T Load(usize off) const
    {
        auto v = T();
        memcpy(&v, page + off, sizeof(v));
        return v;
    }

    template <class T> void Store(usize off, T v)
    {
        memcpy(page + off, &v, sizeof(v));
    }

    u16 SlotOffset(u16 i) const;
    u16 SlotLength(u16 i) const;
    void SetSlot(u16 i, u16 off, u16 len);
    void SetNumSlots(u16 n);
    void SetFreeEnd(u16 e);

    void Compact();
};
} // namespace luvme