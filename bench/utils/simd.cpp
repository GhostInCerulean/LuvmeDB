#include <benchmark/benchmark.h>
#include <random>
#include <string>
#include "utils/simd/strings.hpp"

static std::string MakeText(size_t n)
{
    static constexpr std::string_view kAlpha = "eeeeeettttaaaooiinnsshhrrddllcuumwfgypbvk "; // no q/x/z/j
    std::mt19937 rng(1);
    std::string s(n, ' ');
    for (auto &c : s) c = kAlpha[rng() % kAlpha.size()];
    return s;
}

static std::string MakeNeedle(size_t k, bool rare)
{
    std::string p(k, 'e');
    for (size_t i = 1; i < k; i += 2) p[i] = 's';
    if (rare && k >= 3) p[k / 2] = 'z';
    return p;
}

template <auto Fn>
static void BM(benchmark::State &st)
{
    const std::string hay = MakeText(st.range(0));
    const std::string needle = MakeNeedle(st.range(1), st.range(2) == 0);
    for (auto _ : st)
    {
        bool r = Fn(hay, needle);
        benchmark::DoNotOptimize(r);
    }
    st.SetBytesProcessed(int64_t(st.iterations()) * hay.size());
}

static bool StdFind(std::string_view h, std::string_view p) { return h.find(p) != std::string_view::npos; }

#define REG(fn) \
    BENCHMARK(BM<fn>)->ArgsProduct({{64, 1024, 65536, 1 << 20}, {2, 5, 16, 64}, {0, 1}})

REG(luvme::StringContains);
