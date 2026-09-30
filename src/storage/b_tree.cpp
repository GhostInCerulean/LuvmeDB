// BTree.cpp
#include "b_tree.hpp"

#include "utils/memory/bytes.hpp"

namespace luvme
{
namespace
{
// ---- cell formats -------------------------------------------------------
vector<u8> EncodeLeaf(span<const u8> k, span<const u8> v)
{
    vector<u8> out(2 + k.size() + v.size());
    Store<u16>(out, 0, static_cast<u16>(k.size()));
    ranges::copy(k, out.begin() + 2);
    ranges::copy(v, out.begin() + 2 + k.size());
    return out;
}
span<const u8> LeafKey(span<const u8> p) { return p.subspan(2, Load<u16>(p, 0)); }
span<const u8> LeafVal(span<const u8> p) { return p.subspan(2 + Load<u16>(p, 0)); }

vector<u8> EncodeInternal(PageID child, span<const u8> k)
{
    vector<u8> out(4 + k.size());
    Store<u32>(out, 0, child);
    ranges::copy(k, out.begin() + 4);
    return out;
}
PageID InternalChild(span<const u8> p) { return Load<u32>(p, 0); }
span<const u8> InternalKey(span<const u8> p) { return p.subspan(4); }

// ---- searching ----------------------------------------------------------
strong_ordering Cmp(span<const u8> a, span<const u8> b)
{
    return lexicographical_compare_three_way(a.begin(), a.end(), b.begin(), b.end());
}

// First index whose key >= k. `found` is true on exact match.
u16 LowerBound(const SlottedPage &pg, span<const u8> k, bool &found)
{
    u16 lo = 0, hi = pg.Count();
    while (lo < hi)
    {
        u16 mid = (lo + hi) / 2;
        if (Cmp(LeafKey(pg.Cell(mid)), k) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    found = lo < pg.Count() && Cmp(LeafKey(pg.Cell(lo)), k) == 0;
    return lo;
}

// Child slot for k: first i with k < key(i), or Count() for the rightmost child.
u16 ChildIndex(const SlottedPage &pg, span<const u8> k)
{
    u16 lo = 0, hi = pg.Count();
    while (lo < hi)
    {
        u16 mid = (lo + hi) / 2;
        if (Cmp(k, InternalKey(pg.Cell(mid))) < 0)
            hi = mid;
        else
            lo = mid + 1;
    }
    return lo;
}

PageID ChildAt(const SlottedPage &pg, u16 idx)
{
    return idx < pg.Count() ? InternalChild(pg.Cell(idx)) : pg.Link();
}

void SetChild(SlottedPage &pg, u16 idx, PageID id)
{
    if (idx < pg.Count())
        Store<u32>(pg.Cell(idx), 0, id);
    else
        pg.SetLink(id);
}

// ---- split helpers ------------------------------------------------------
vector<vector<u8>> CollectCells(const SlottedPage &pg)
{
    vector<vector<u8>> out;
    for (u16 i = 0; i < pg.Count(); ++i)
    {
        auto c = pg.Cell(i);
        out.emplace_back(c.begin(), c.end());
    }
    return out;
}

void Fill(SlottedPage &pg, const vector<vector<u8>> &cells, usize from, usize to)
{
    for (usize i = from; i < to; ++i)
        pg.Insert(static_cast<u16>(i - from), cells[i]);
}

// Split roughly by bytes, always leaving both sides non-empty.
usize SplitPoint(const vector<vector<u8>> &cells)
{
    usize total = 0;
    for (auto &c : cells)
        total += c.size() + 4;
    usize acc = 0;
    for (usize i = 0; i < cells.size(); ++i)
    {
        acc += cells[i].size() + 4;
        if (acc >= total / 2)
            return clamp<usize>(i + 1, 1, cells.size() - 1);
    }
    return cells.size() / 2;
}

struct Entry
{
    PageID Child;
    vector<u8> Key;
};
} // namespace

// ---------------------------------------------------------------------------

awaitable<expected<BTree, SystemErr>> BTree::Open(Pager &pager)
{
    BTree t(pager);
    if (pager.Root() == kNoPage)
    {
        auto id = co_await pager.AllocatePage();
        if (!id)
            co_return unexpected(id.error());
        PageBuf buf;
        SlottedPage(buf).Init(PageKind::Leaf);
        if (auto e = co_await pager.WritePage(*id, buf))
            co_return unexpected(*e);
        pager.SetRoot(*id);
    }
    co_return t;
}

awaitable<optional<SystemErr>> BTree::Descend(span<const u8> key, PageBuf &buf, PageID &id, vector<Step> *path)
{
    id = pager.Root();
    for (;;)
    {
        if (auto e = co_await pager.ReadPage(id, buf))
            co_return e;
        SlottedPage pg(buf);
        if (pg.Kind() == PageKind::Leaf)
            co_return nullopt;
        if (pg.Kind() != PageKind::Internal)
            co_return Errc(boost::system::errc::bad_message);

        u16 idx = ChildIndex(pg, key);
        if (path)
            path->push_back({id, idx});
        id = ChildAt(pg, idx);
    }
}

awaitable<expected<optional<vector<u8>>, SystemErr>> BTree::Get(span<const u8> key)
{
    PageBuf buf;
    PageID id;
    if (auto e = co_await Descend(key, buf, id, nullptr))
        co_return unexpected(*e);

    SlottedPage leaf(buf);
    bool found;
    u16 i = LowerBound(leaf, key, found);
    if (!found)
        co_return optional<vector<u8>>{};

    auto v = LeafVal(leaf.Cell(i));
    co_return optional<vector<u8>>(vector<u8>(v.begin(), v.end()));
}

awaitable<optional<SystemErr>> BTree::Put(span<const u8> key, span<const u8> value)
{
    auto payload = EncodeLeaf(key, value);
    if (payload.size() > kMaxCell)
        co_return Errc(boost::system::errc::value_too_large);

    PageBuf buf;
    PageID id;
    vector<Step> path;
    if (auto e = co_await Descend(key, buf, id, &path))
        co_return e;

    SlottedPage leaf(buf);
    bool found;
    u16 pos = LowerBound(leaf, key, found);
    if (found)
        leaf.Remove(pos); // upsert: drop old version first
    if (leaf.Insert(pos, payload))
        co_return co_await pager.WritePage(id, buf);

    // ---- leaf split ------------------------------------------------------
    auto cells = CollectCells(leaf);
    cells.insert(cells.begin() + pos, std::move(payload));
    usize mid = SplitPoint(cells);

    auto rid = co_await pager.AllocatePage();
    if (!rid)
        co_return rid.error();

    PageBuf rbuf;
    SlottedPage right(rbuf);
    right.Init(PageKind::Leaf);
    right.SetLink(leaf.Link());
    Fill(right, cells, mid, cells.size());

    leaf.Init(PageKind::Leaf);
    Fill(leaf, cells, 0, mid);
    leaf.SetLink(*rid);

    if (auto e = co_await pager.WritePage(*rid, rbuf))
        co_return e;
    if (auto e = co_await pager.WritePage(id, buf))
        co_return e;

    // Separator = first key of the right sibling. Leaf keys are copied up.
    auto sk = LeafKey(cells[mid]);
    vector<u8> sep(sk.begin(), sk.end());
    PageID leftId = id, newId = *rid;

    // ---- propagate the split upward ---------------------------------------
    for (;;)
    {
        if (path.empty())
        {
            auto root = co_await pager.AllocatePage();
            if (!root)
                co_return root.error();
            PageBuf rb;
            SlottedPage rp(rb);
            rp.Init(PageKind::Internal);
            rp.Insert(0, EncodeInternal(leftId, sep));
            rp.SetLink(newId);
            if (auto e = co_await pager.WritePage(*root, rb))
                co_return e;
            pager.SetRoot(*root);
            co_return nullopt;
        }

        Step st = path.back();
        path.pop_back();

        PageBuf pbuf;
        if (auto e = co_await pager.ReadPage(st.Id, pbuf))
            co_return e;
        SlottedPage parent(pbuf);

        // Cell at ChildIdx was (L, K). It becomes (L, sep) followed by (R, K).
        if (parent.Insert(st.ChildIdx, EncodeInternal(leftId, sep)))
        {
            SetChild(parent, st.ChildIdx + 1, newId);
            co_return co_await pager.WritePage(st.Id, pbuf);
        }

        // ---- internal split (parent full) ---------------------------------
        vector<Entry> es;
        for (u16 i = 0; i < parent.Count(); ++i)
        {
            auto c = parent.Cell(i);
            auto k = InternalKey(c);
            es.push_back({InternalChild(c), vector<u8>(k.begin(), k.end())});
        }
        PageID link = parent.Link();

        es.insert(es.begin() + st.ChildIdx, Entry{leftId, sep});
        if (st.ChildIdx + 1u < es.size())
            es[st.ChildIdx + 1].Child = newId;
        else
            link = newId;

        usize m = es.size() / 2; // es[m].Key moves UP, it is not kept in either half

        auto nid = co_await pager.AllocatePage();
        if (!nid)
            co_return nid.error();

        PageBuf nbuf;
        SlottedPage np(nbuf);
        np.Init(PageKind::Internal);
        for (usize i = m + 1; i < es.size(); ++i)
            np.Insert(static_cast<u16>(i - m - 1), EncodeInternal(es[i].Child, es[i].Key));
        np.SetLink(link);

        parent.Init(PageKind::Internal);
        for (usize i = 0; i < m; ++i)
            parent.Insert(static_cast<u16>(i), EncodeInternal(es[i].Child, es[i].Key));
        parent.SetLink(es[m].Child);

        if (auto e = co_await pager.WritePage(*nid, nbuf))
            co_return e;
        if (auto e = co_await pager.WritePage(st.Id, pbuf))
            co_return e;

        sep = std::move(es[m].Key);
        leftId = st.Id;
        newId = *nid;
    }
}
} // namespace luvme