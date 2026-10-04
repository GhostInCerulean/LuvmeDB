#include <gtest/gtest.h>

#include <fstream>
#include <string>

#include "storage/page_allocator.hpp"
#include "storage/page_view.hpp"
#include "../util.hpp"

using namespace luvme;
using luvme::test::RunSync;
using luvme::test::SharedIo;

namespace {
    class PageAllocatorTest : public ::testing::Test {
    protected:
        asio::io_context &io = SharedIo();
        std::filesystem::path path;

        void SetUp() override {
            auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
            path = std::filesystem::temp_directory_path() / (std::string("luvme_alloc_") + info->name() + ".db");
            std::filesystem::remove(path);
        }

        void TearDown() override { std::filesystem::remove(path); }

        PhysicalPages OpenPages() { return RunSync(io, PhysicalPages::Open(io.get_executor(), path)); }
        PageAllocator OpenAlloc(PhysicalPages &p) { return RunSync(io, PageAllocator::Open(p)); }
        PageId Alloc(PageAllocator &a) { return RunSync(io, a.Allocate()); }
        void Free(PageAllocator &a, PageId id) { RunSync(io, a.Free(id)); }
        void Flush(PageAllocator &a) { RunSync(io, a.Flush()); }

        PageBuf Read(PhysicalPages &p, PageId id) {
            PageBuf out{};
            RunSync(io, p.ReadPage(id, out));
            return out;
        }

        void Write(PhysicalPages &p, PageId id, const PageBuf &data) {
            RunSync(io, p.WritePage(id, data));
        }

        void WriteRawFile(const PageBuf &firstPage) {
            std::ofstream f(path, std::ios::binary | std::ios::trunc);
            f.write(reinterpret_cast<const char *>(firstPage.data()), firstPage.size());
        }
    };
}

TEST_F(PageAllocatorTest, NewFileGetsAMetaPage) {
    auto pages = OpenPages();
    auto alloc = OpenAlloc(pages);
    EXPECT_EQ(pages.GetPageCount(), 1u);
    EXPECT_EQ(alloc.GetRoot(), kNoPage);
    EXPECT_EQ(alloc.FreeCount(), 0u);

    auto meta = Read(pages, 0);
    EXPECT_EQ(Load<u32>(meta, 0), kMetaMagic);
    EXPECT_EQ(Load<u32>(meta, 4), kPageSize);
}

TEST_F(PageAllocatorTest, AllocateReturnsSequentialIdsStartingAtOne) {
    auto pages = OpenPages();
    auto alloc = OpenAlloc(pages);
    EXPECT_EQ(Alloc(alloc), 1u);
    EXPECT_EQ(Alloc(alloc), 2u);
    EXPECT_EQ(Alloc(alloc), 3u);
    EXPECT_EQ(pages.GetPageCount(), 4u);
}

TEST_F(PageAllocatorTest, FreshPagesAreZeroed) {
    auto pages = OpenPages();
    auto alloc = OpenAlloc(pages);
    auto id = Alloc(alloc);
    EXPECT_EQ(Read(pages, id), PageBuf{});
}

TEST_F(PageAllocatorTest, FreedPageIsReusedBeforeTheFileGrows) {
    auto pages = OpenPages();
    auto alloc = OpenAlloc(pages);
    auto a = Alloc(alloc);
    auto b = Alloc(alloc);
    (void) b;

    Free(alloc, a);
    EXPECT_EQ(alloc.FreeCount(), 1u);

    auto count = pages.GetPageCount();
    EXPECT_EQ(Alloc(alloc), a);
    EXPECT_EQ(alloc.FreeCount(), 0u);
    EXPECT_EQ(pages.GetPageCount(), count); // no growth
}

TEST_F(PageAllocatorTest, FreeListIsLifo) {
    auto pages = OpenPages();
    auto alloc = OpenAlloc(pages);
    auto a = Alloc(alloc), b = Alloc(alloc), c = Alloc(alloc);

    Free(alloc, a);
    Free(alloc, b);
    Free(alloc, c);
    EXPECT_EQ(alloc.FreeCount(), 3u);

    EXPECT_EQ(Alloc(alloc), c);
    EXPECT_EQ(Alloc(alloc), b);
    EXPECT_EQ(Alloc(alloc), a);
    EXPECT_EQ(Alloc(alloc), 4u); // list empty -> file grows again
}

TEST_F(PageAllocatorTest, FreedPageIsMarkedFreeAndLinksToPreviousHead) {
    auto pages = OpenPages();
    auto alloc = OpenAlloc(pages);
    auto a = Alloc(alloc), b = Alloc(alloc);

    Free(alloc, a);
    Free(alloc, b);

    auto bufB = Read(pages, b);
    PageView vb{span<u8>(bufB)};
    EXPECT_EQ(vb.Type(), PageType::Free);
    EXPECT_EQ(vb.Link(), a);

    auto bufA = Read(pages, a);
    PageView va{span<u8>(bufA)};
    EXPECT_EQ(va.Type(), PageType::Free);
    EXPECT_EQ(va.Link(), kNoPage);
}

TEST_F(PageAllocatorTest, FreeRejectsInvalidIds) {
    auto pages = OpenPages();
    auto alloc = OpenAlloc(pages);
    Alloc(alloc);
    EXPECT_THROW(Free(alloc, kNoPage), std::invalid_argument); // meta page
    EXPECT_THROW(Free(alloc, 999), std::invalid_argument);     // past the end
    EXPECT_EQ(alloc.FreeCount(), 0u);
}

TEST_F(PageAllocatorTest, AllocateDetectsCorruptedFreeList) {
    auto pages = OpenPages();
    auto alloc = OpenAlloc(pages);
    auto a = Alloc(alloc);
    Free(alloc, a);

    PageBuf leaf{};
    PageView(span<u8>(leaf)).Init(PageType::Leaf);
    Write(pages, a, leaf); // someone overwrote a page that is still on the free list

    EXPECT_THROW(Alloc(alloc), std::runtime_error);
}

TEST_F(PageAllocatorTest, SetRootUpdatesGetter) {
    auto pages = OpenPages();
    auto alloc = OpenAlloc(pages);
    alloc.SetRoot(5);
    EXPECT_EQ(alloc.GetRoot(), 5u);
}

TEST_F(PageAllocatorTest, FlushPersistsRootAndFreeList) {
    PageId a, b;
    {
        auto pages = OpenPages();
        auto alloc = OpenAlloc(pages);
        a = Alloc(alloc);
        b = Alloc(alloc);
        Free(alloc, a);
        alloc.SetRoot(b);
        Flush(alloc);
    }

    auto pages = OpenPages();
    auto alloc = OpenAlloc(pages);
    EXPECT_EQ(alloc.GetRoot(), b);
    EXPECT_EQ(alloc.FreeCount(), 1u);
    EXPECT_EQ(pages.GetPageCount(), 3u);
    EXPECT_EQ(Alloc(alloc), a); // the free list survived the reopen
}

TEST_F(PageAllocatorTest, FlushWithoutChangesLeavesTheMetaPageAlone) {
    auto pages = OpenPages();
    auto alloc = OpenAlloc(pages);
    auto before = Read(pages, 0);
    Flush(alloc);
    EXPECT_EQ(Read(pages, 0), before);
}

TEST_F(PageAllocatorTest, OpenRejectsWrongMagic) {
    WriteRawFile(PageBuf{}); // 4096 zero bytes
    auto pages = OpenPages();
    EXPECT_THROW(OpenAlloc(pages), std::runtime_error);
}

TEST_F(PageAllocatorTest, OpenRejectsWrongPageSize) {
    PageBuf meta{};
    Store(meta, 0, kMetaMagic);
    Store(meta, 4, u32(8192));
    WriteRawFile(meta);
    auto pages = OpenPages();
    EXPECT_THROW(OpenAlloc(pages), std::runtime_error);
}