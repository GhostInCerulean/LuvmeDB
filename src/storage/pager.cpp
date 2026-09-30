#include "pager.hpp"

#include "utils/memory/bytes.hpp"

namespace luvme
{
namespace
{
constexpr u32 kMetaMagic = 0x4C55564D; // "LUVM"
const auto kTok = asio::as_tuple(asio::use_awaitable);
} // namespace

Pager::Pager(asio::random_access_file &&f) : file(std::move(f)) {}

awaitable<expected<Pager, SystemErr>> Pager::Open(asio::any_io_executor ex, const filesystem::path &path)
{
    asio::random_access_file f(ex);
    SystemErr ec;
    f.open(path.string(), asio::random_access_file::read_write | asio::random_access_file::create, ec);
    if (ec)
        co_return unexpected(ec);

    u64 size = f.size(ec);
    if (ec)
        co_return unexpected(ec);

    Pager p(std::move(f));
    if (size == 0)
    {
        p.pageCount = 1; // just the meta page
        if (auto e = co_await p.WriteMeta())
            co_return unexpected(*e);
        co_return std::move(p);
    }

    PageBuf m;
    if (auto e = co_await p.ReadPage(0, m))
        co_return unexpected(*e);
    if (Load<u32>(m, 0) != kMetaMagic || Load<u32>(m, 4) != kPageSize)
        co_return unexpected(Errc(boost::system::errc::bad_message));

    p.pageCount = static_cast<PageID>(size / kPageSize); // file size is the source of truth
    p.root = Load<u32>(m, 12);
    co_return std::move(p);
}

awaitable<optional<SystemErr>> Pager::ReadPage(PageID id, span<u8> out)
{
    auto [ec, n] = co_await asio::async_read_at(file, Offset(id), asio::buffer(out.first(kPageSize)), kTok);
    if (ec)
        co_return ec;
    co_return nullopt;
}

awaitable<optional<SystemErr>> Pager::WritePage(PageID id, span<const u8> in)
{
    auto [ec, n] = co_await asio::async_write_at(file, Offset(id), asio::buffer(in.first(kPageSize)), kTok);
    if (ec)
        co_return ec;
    co_return nullopt;
}

awaitable<expected<PageID, SystemErr>> Pager::AllocatePage()
{
    PageID id = pageCount++;
    PageBuf zero{};
    if (auto e = co_await WritePage(id, zero)) // extend the file now so reads never hit EOF
        co_return unexpected(*e);
    metaDirty = true;
    co_return id;
}

awaitable<optional<SystemErr>> Pager::WriteMeta()
{
    PageBuf m{};
    Store<u32>(m, 0, kMetaMagic);
    Store<u32>(m, 4, kPageSize);
    Store<u32>(m, 8, pageCount);
    Store<u32>(m, 12, root);
    if (auto e = co_await WritePage(0, m))
        co_return e;
    metaDirty = false;
    co_return nullopt;
}

awaitable<optional<SystemErr>> Pager::Sync()
{
    if (metaDirty)
        if (auto e = co_await WriteMeta())
            co_return e;
    SystemErr ec;
    file.sync_all(ec); // NOTE: asio has no async fsync; this blocks the executor thread
    if (ec)
        co_return ec;
    co_return nullopt;
}
} // namespace luvme