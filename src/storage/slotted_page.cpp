#include "slotted_page.hpp"

namespace luvme
{

void SlottedPage::Init(PageKind kind)
{
    memset(buf.data(), 0, kPageSize);
    buf[0] = static_cast<u8>(kind);
    SetCellTop(kPageSize);
}
PageKind SlottedPage::Kind() const
{
    return static_cast<PageKind>(buf[0]);
}
u16 SlottedPage::Count() const
{
    return Load<u16>(buf, 2);
}
PageID SlottedPage::Link() const
{
    return Load<u32>(buf, 8);
}
void SlottedPage::SetLink(PageID id)
{
    Store<u32>(buf, 8, id);
}
span<u8> SlottedPage::Cell(u16 i) const
{
    u16 off = SlotOff(i);
    return buf.subspan(off + 2, Load<u16>(buf, off));
}
bool SlottedPage::Insert(u16 i, span<const u8> payload)
{
    usize cellBytes = payload.size() + 2;
    usize need = cellBytes + 2; // + slot
    if (need > FreeSpace() + Garbage())
        return false;
    if (need > FreeSpace())
        Compact();

    u16 top = static_cast<u16>(CellTop() - cellBytes);
    Store<u16>(buf, top, static_cast<u16>(payload.size()));
    memcpy(buf.data() + top + 2, payload.data(), payload.size());

    u16 n = Count();
    u8 *slots = buf.data() + kHeader;
    memmove(slots + 2 * (i + 1), slots + 2 * i, 2 * (n - i));
    Store<u16>(buf, kHeader + 2 * i, top);
    SetCount(n + 1);
    SetCellTop(top);
    return true;
}
void SlottedPage::Remove(u16 i)
{
    u16 n = Count();
    SetGarbage(static_cast<u16>(Garbage() + Cell(i).size() + 2));
    u8 *slots = buf.data() + kHeader;
    memmove(slots + 2 * i, slots + 2 * (i + 1), 2 * (n - i - 1));
    SetCount(n - 1);
}
u16 SlottedPage::CellTop() const
{
    return Load<u16>(buf, 4);
}
u16 SlottedPage::Garbage() const
{
    return Load<u16>(buf, 6);
}
u16 SlottedPage::SlotOff(u16 i) const
{
    return Load<u16>(buf, kHeader + 2 * i);
}
usize SlottedPage::FreeSpace() const
{
    return CellTop() - (kHeader + 2 * Count());
}
void SlottedPage::SetCount(u16 n)
{
    Store<u16>(buf, 2, n);
}
void SlottedPage::SetCellTop(u16 t)
{
    Store<u16>(buf, 4, t);
}
void SlottedPage::SetGarbage(u16 g)
{
    Store<u16>(buf, 6, g);
}
void SlottedPage::Compact()
{
    PageBuf tmp;
    usize top = kPageSize;
    for (u16 i = 0; i < Count(); ++i)
    {
        u16 off = SlotOff(i);
        usize len = Load<u16>(buf, off) + 2;
        top -= len;
        memcpy(tmp.data() + top, buf.data() + off, len);
        Store<u16>(buf, kHeader + 2 * i, static_cast<u16>(top));
    }
    memcpy(buf.data() + top, tmp.data() + top, kPageSize - top);
    SetCellTop(static_cast<u16>(top));
    SetGarbage(0);
}
} // namespace luvme