#include "utils/simd/strings.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "utils/simd/strings.cpp" // the .cpp, not the .hpp
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace luvme::HWY_NAMESPACE
{
namespace hn = hwy::HWY_NAMESPACE;

bool StringContainsImpl(string_view str, string_view substr)
{
    const size_t n = str.size();
    const size_t k = substr.size();

    if (k == 0)
        return true;
    if (k > n)
        return false;

    const auto *hay = reinterpret_cast<const uint8_t *>(str.data());
    const auto *needle = reinterpret_cast<const uint8_t *>(substr.data());

    const hn::ScalableTag<uint8_t> d;
    const size_t N = hn::Lanes(d);

    // Broadcast the first and last byte of the needle.
    const auto first = hn::Set(d, needle[0]);
    const auto last = hn::Set(d, needle[k - 1]);

    // Candidate start positions are [0, n - k].
    const size_t positions = n - k + 1;

    size_t i = 0;
    for (; i + N <= positions; i += N)
    {
        // Compare N candidate start positions at once: the byte at the
        // candidate start must equal needle[0], and the byte at
        // start + k - 1 must equal needle[k - 1].
        const auto block_first = hn::LoadU(d, hay + i);
        const auto block_last = hn::LoadU(d, hay + i + k - 1);
        auto candidates = hn::And(hn::Eq(block_first, first), hn::Eq(block_last, last));

        // Verify each surviving candidate (usually very few).
        while (!hn::AllFalse(d, candidates))
        {
            const size_t lane = hn::FindKnownFirstTrue(d, candidates);
            const size_t pos = i + lane;

            // k <= 2: first and last bytes already cover the whole needle.
            if (k <= 2 || std::memcmp(hay + pos + 1, needle + 1, k - 2) == 0)
            {
                return true;
            }
            // Clear the lane we just checked.
            candidates = hn::AndNot(hn::SetOnlyFirst(candidates), candidates);
        }
    }

    // Scalar tail: fewer than N candidate positions remain.
    return str.substr(i).find(substr) != string_view::npos;
}
} // namespace luvme::HWY_NAMESPACE
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace luvme
{
HWY_EXPORT(StringContainsImpl);

bool StringContains(string_view str, string_view substr)
{
    return HWY_DYNAMIC_DISPATCH(StringContainsImpl)(str, substr);
}
} // namespace luvme
#endif