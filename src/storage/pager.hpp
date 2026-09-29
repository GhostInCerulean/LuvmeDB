#pragma once
#include <expected>
#include <filesystem>

#include "pch.hpp"

namespace luvme
{
constexpr usize kPageSize = 4096;
using PageID = u32;
constexpr u32 kPageMagic = 0xC0BCACA;

struct alignas(kPageSize) PageBuf
{
    array<u8, kPageSize> Buf;
    span<u8> Span()
    {
        return Buf;
    }
};

class Pager
{
    asio::random_access_file file;
    PageID pageCount = 0;

  public:
    static awaitable<expected<Pager, SystemErr>> Open(asio::any_io_executor ex, const filesystem::path &path);
    awaitable<optional<SystemErr>> Init();

    awaitable<optional<SystemErr>> ReadPage(PageID id, span<u8> out);
    awaitable<optional<SystemErr>> WritePage(PageID id, span<const u8> in);

    awaitable<expected<PageID, SystemErr>> AllocatePage();
    awaitable<optional<SystemErr>> Sync();

    PageID GetPageCount() const;

  private:
    Pager(asio::random_access_file &&file);

    awaitable<optional<SystemErr>> WriteMeta();

    static usize Offset(PageID id);
};
} // namespace luvme
