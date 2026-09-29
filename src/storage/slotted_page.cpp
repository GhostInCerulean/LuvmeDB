#include "slotted_page.hpp"

namespace luvme
{
SlottedPage::SlottedPage(u8 *page) : page(page)
{
}

void SlottedPage::Init(PageID id)
{
    memset(page, 0, kPageSize);
    Store<PageID>(0, id);
    SetNumSlots(0);
    SetFreeEnd(kPageSize);
}

u16 SlottedPage::NumSlots() const
{
    return Load<u16>(kNumSlotsPos);
}

u16 SlottedPage::FreeEnd() const
{
    return Load<u16>(kFreeEndPos);
}

usize SlottedPage::ContiguousFree() const
{
    return FreeEnd() - (kHeaderSize + NumSlots() * kSlotSize);
}

usize SlottedPage::TotalFree() const
{
    auto live = usize(0);
    for (auto i = u16(0); i < NumSlots(); ++i)
    {
        if (SlotOffset(i))
            live += SlotLength(i);
    }
    return kPageSize - kHeaderSize - NumSlots() * kSlotSize - live;
}

optional<u16> SlottedPage::Insert(span<const u8> rec)
{
    // reuse a tombstoned slot if there is one
    auto reuse = optional<u16>();
    for (auto i = u16(0); i < NumSlots(); ++i)
    {
        if (SlotOffset(i) == 0)
        {
            reuse = i;
            break;
        }
    }

    const auto need = rec.size() + (reuse ? 0 : kSlotSize);
    if (ContiguousFree() < need)
    {
        if (TotalFree() < need)
            return nullopt; // page is full
        Compact();
    }

    const auto off = u16(FreeEnd() - rec.size());
    memcpy(page + off, rec.data(), rec.size());
    SetFreeEnd(off);

    const auto slot = reuse ? *reuse : NumSlots();
    if (!reuse)
        SetNumSlots(NumSlots() + 1);
    SetSlot(slot, off, u16(rec.size()));
    return slot;
}

optional<span<const u8>> SlottedPage::Get(u16 slot) const
{
    if (slot >= NumSlots() || SlotOffset(slot) == 0)
        return nullopt;
    return span<const u8>(page + SlotOffset(slot), SlotLength(slot));
}

bool SlottedPage::Erase(u16 slot)
{
    if (slot >= NumSlots() || SlotOffset(slot) == 0)
        return false;
    SetSlot(slot, 0, 0); // slot ids stay stable; space is reclaimed on Compact
    return true;
}

u16 SlottedPage::SlotOffset(u16 i) const
{
    return Load<u16>(kHeaderSize + i * kSlotSize);
}

u16 SlottedPage::SlotLength(u16 i) const
{
    return Load<u16>(kHeaderSize + i * kSlotSize + sizeof(u16));
}

void SlottedPage::SetSlot(u16 i, u16 off, u16 len)
{
    Store<u16>(kHeaderSize + i * kSlotSize, off);
    Store<u16>(kHeaderSize + i * kSlotSize + sizeof(u16), len);
}

void SlottedPage::SetNumSlots(u16 n)
{
    Store<u16>(kNumSlotsPos, n);
}

void SlottedPage::SetFreeEnd(u16 e)
{
    Store<u16>(kFreeEndPos, e);
}

void SlottedPage::Compact()
{
    auto tmp = array<u8, kPageSize>();
    auto end = kPageSize;
    for (auto i = u16(0); i < NumSlots(); ++i)
    {
        if (!SlotOffset(i))
            continue;
        const auto len = SlotLength(i);
        end -= len;
        memcpy(tmp.data() + end, page + SlotOffset(i), len);
        SetSlot(i, u16(end), len);
    }
    memcpy(page + end, tmp.data() + end, kPageSize - end);
    SetFreeEnd(u16(end));
}
} // namespace luvme