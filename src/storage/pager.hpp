#pragma once
#include <filesystem>
#include "pch.hpp"
#include <expected>

namespace luvme
{
constexpr usize kPageSize = 4096;
using PageID = u32;
constexpr PageID kNoPage = 0; // page 0 is meta, so it can never be a data page
using PageBuf = array<u8, kPageSize>;

inline SystemErr Errc(boost::system::errc::errc_t e)
{
    return boost::system::errc::make_error_code(e);
}

class Pager
{
    asio::random_access_file file;
    PageID pageCount = 0;
    PageID root = kNoPage;
    bool metaDirty = false;

  public:
    static awaitable<expected<Pager, SystemErr>> Open(asio::any_io_executor ex, const filesystem::path &path);

    awaitable<optional<SystemErr>> ReadPage(PageID id, span<u8> out);
    awaitable<optional<SystemErr>> WritePage(PageID id, span<const u8> in);
    awaitable<expected<PageID, SystemErr>> AllocatePage();
    awaitable<optional<SystemErr>> Sync();

    PageID Root() const { return root; }
    void SetRoot(PageID id) { root = id; metaDirty = true; }
    PageID PageCount() const { return pageCount; }

  private:
    explicit Pager(asio::random_access_file &&f);
    awaitable<optional<SystemErr>> WriteMeta();
    static u64 Offset(PageID id) { return u64(id) * kPageSize; }
};
} // namespace luvme