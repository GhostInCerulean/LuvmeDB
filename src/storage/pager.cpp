#include "pager.hpp"

namespace luvme
{
awaitable<expected<Pager, SystemErr>> Pager::Open(asio::any_io_executor ex, const filesystem::path &path)
{
    auto file = asio::random_access_file(ex);
    auto err = SystemErr();
    (void)file.open(path, asio::file_base::read_write | asio::file_base::create, err);

    if (err)
    {
        co_return unexpected(err);
    }
    else
    {
        co_return Pager(std::move(file));
    }
}

awaitable<optional<SystemErr>> Pager::Init()
{
    auto error = SystemErr();

    auto size = file.size(error);
    if (error)
        co_return error;

    // brand new file
    if (size == 0)
    {
        pageCount = 1; // page 0 is meta
        co_return co_await WriteMeta();
    }

    auto buf = make_unique<PageBuf>();
    auto res = co_await ReadPage(0, buf->Span());
    if (res)
        co_return *res;

    auto magic = u32(0);
    memcpy(&magic, buf->Span().data(), sizeof(magic));
    if (magic != kPageMagic)
        co_return make_error_code(errc::invalid_argument);

    memcpy(&pageCount, buf->Span().data() + sizeof(PageID), sizeof(pageCount));

    co_return nullopt;
}

awaitable<optional<SystemErr>> Pager::ReadPage(PageID id, span<u8> out)
{
    auto err = SystemErr();
    co_await asio::async_read_at(file, Offset(id), asio::buffer(out.data(), kPageSize),
                                 asio::redirect_error(asio::use_awaitable, err));
    if (err)
    {
        co_return err;
    }
    co_return nullopt;
}

awaitable<optional<SystemErr>> Pager::WritePage(PageID id, span<const u8> in)
{
    auto err = SystemErr();
    co_await asio::async_write_at(file, Offset(id), asio::buffer(in.data(), kPageSize),
                                  asio::redirect_error(asio::use_awaitable, err));
    if (err)
    {
        co_return err;
    }
    co_return nullopt;
}

awaitable<expected<PageID, SystemErr>> Pager::AllocatePage()
{
    const auto kZero = PageBuf{};

    auto id = pageCount++;
    if (auto err = co_await WritePage(id, kZero.Buf))
        co_return unexpected(*err);
    if (auto err = co_await WriteMeta())
        co_return unexpected(*err);

    co_return id;
}

awaitable<optional<SystemErr>> Pager::Sync()
{
    auto err = SystemErr();
    file.sync_data(err);
    if (err)
    {
        co_return err;
    }
    else
    {
        co_return nullopt;
    }
}

PageID Pager::GetPageCount() const
{
    return pageCount;
}

Pager::Pager(asio::random_access_file &&file) : file(std::move(file))
{
}

awaitable<optional<SystemErr>> Pager::WriteMeta()
{
    auto buf = make_unique<PageBuf>();
    memcpy(buf->Buf.data(), &kPageMagic, sizeof(kPageMagic));
    memcpy(buf->Buf.data() + sizeof(pageCount), &pageCount, sizeof(pageCount));
    co_return co_await WritePage(0, buf->Buf);
}

usize Pager::Offset(PageID id)
{
    return usize(id) * kPageSize;
}
} // namespace luvme
