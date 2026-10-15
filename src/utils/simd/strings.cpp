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


// Rough "how common is this byte in English-ish text"; higher = more common.
constexpr int ByteScore(uint8_t c)
{
    constexpr std::string_view kFreq = " etaoinshrdlcumwfgypbvkjxqz";
    if (c >= 'A' && c <= 'Z')
        return 60;
    if (c >= '0' && c <= '9')
        return 100;
    const size_t p = kFreq.find(static_cast<char>(c));
    if (p != std::string_view::npos)
        return 250 - static_cast<int>(p);
    if (c >= 0x20 && c < 0x7F)
        return 80; // punctuation
    return 10;     // control / non-ASCII
}

constexpr std::array<uint8_t, 256> kScore = [] {
    std::array<uint8_t, 256> t{};
    for (size_t c = 0; c < 256; ++c)
        t[c] = static_cast<uint8_t>(ByteScore(static_cast<uint8_t>(c)));
    return t;
}();

struct Probe
{
    size_t a, b;
};

// Two distinct offsets with the rarest bytes. Requires k >= 2.
inline Probe PickProbes(const uint8_t *needle, size_t k)
{
    size_t a = 0;
    for (size_t j = 1; j < k; ++j)
        if (kScore[needle[j]] < kScore[needle[a]])
            a = j;
    size_t b = (a == 0) ? 1 : 0;
    for (size_t j = 0; j < k; ++j)
        if (j != a && kScore[needle[j]] < kScore[needle[b]])
            b = j;
    return {a, b};
}

template <bool kUnroll>
HWY_INLINE bool ContainsImpl(std::string_view str, std::string_view substr)
{
    const size_t n = str.size();
    const size_t k = substr.size();
    if (k == 0)
        return true;
    if (k > n)
        return false;

    const auto *hay = reinterpret_cast<const uint8_t *>(str.data());
    const auto *needle = reinterpret_cast<const uint8_t *>(substr.data());

    if (k == 1)
        return std::memchr(hay, needle[0], n) != nullptr;

    using D = hn::ScalableTag<uint8_t>;
    using Mask = hn::Mask<D>;
    const D d;
    const size_t N = hn::Lanes(d);
    const size_t positions = n - k + 1; // valid start positions: [0, n - k]

    // Not even one full vector of candidates: let the stdlib handle it.
    if (positions < N)
        return str.find(substr) != std::string_view::npos;

    const auto [a, b] = PickProbes(needle, k);
    const auto va = hn::Set(d, needle[a]);
    const auto vb = hn::Set(d, needle[b]);

    // Candidate mask for N consecutive start positions.
    // Reads up to hay[start + N - 1 + (k - 1)] <= hay[n - 1].
    auto block = [&](size_t start) HWY_ATTR {
        return hn::And(hn::Eq(hn::LoadU(d, hay + start + a), va),
                       hn::Eq(hn::LoadU(d, hay + start + b), vb));
    };

    // Verify surviving candidates one by one.
    auto verify = [&](Mask cand, size_t base) HWY_ATTR -> bool {
        while (!hn::AllFalse(d, cand))
        {
            const size_t lane = hn::FindKnownFirstTrue(d, cand);
            if (k == 2 || std::memcmp(hay + base + lane, needle, k) == 0)
                return true;
            cand = hn::AndNot(hn::SetOnlyFirst(cand), cand);
        }
        return false;
    };

    size_t i = 0;

    if constexpr (kUnroll)
    {
        for (; i + 2 * N <= positions; i += 2 * N)
        {
            const Mask c0 = block(i);
            const Mask c1 = block(i + N);
            if (!hn::AllFalse(d, hn::Or(c0, c1)))
            {
                if (verify(c0, i) || verify(c1, i + N))
                    return true;
            }
        }
    }

    for (; i + N <= positions; i += N)
    {
        if (verify(block(i), i))
            return true;
    }

    // Leftover < N positions: one overlapping block at the very end.
    if (i < positions)
        return verify(block(positions - N), positions - N);
    return false;
}

bool ContainsLean(std::string_view s, std::string_view p) { return ContainsImpl<false>(s, p); }
bool ContainsFull(std::string_view s, std::string_view p) { return ContainsImpl<true>(s, p); }


} // namespace luvme::HWY_NAMESPACE
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace luvme
{
    HWY_EXPORT(ContainsLean);
    HWY_EXPORT(ContainsFull);

    bool StringContains(std::string_view str, std::string_view substr)
    {
        return HWY_DYNAMIC_DISPATCH(ContainsFull)(str, substr);
    }
} // namespace luvme
#endif