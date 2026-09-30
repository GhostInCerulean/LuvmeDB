// BTree.hpp
#pragma once
#include "slotted_page.hpp"

namespace luvme
{
class BTree
{
    // Keep cells small enough that any split leaves both halves fitting in a page.
    static constexpr usize kMaxCell = 1000;

    struct Step
    {
        PageID Id;
        u16 ChildIdx;
    };

    Pager &pager;

  public:
    static awaitable<expected<BTree, SystemErr>> Open(Pager &pager);

    // key/value spans must stay alive until the awaitable completes.
    awaitable<expected<optional<vector<u8>>, SystemErr>> Get(span<const u8> key);
    awaitable<optional<SystemErr>> Put(span<const u8> key, span<const u8> value);

  private:
    explicit BTree(Pager &p) : pager(p) {}
    awaitable<optional<SystemErr>> Descend(span<const u8> key, PageBuf &leaf, PageID &leafId, vector<Step> *path);
};
} // namespace luvme