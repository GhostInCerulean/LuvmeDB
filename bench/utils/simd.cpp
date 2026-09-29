#include <benchmark/benchmark.h>

#include <cstring>
#include <random>
#include <string>
#include <string_view>

#include "utils/simd/strings.hpp"

namespace {

// ---- Implementations under test (all take the same signature) -------------
bool StdFind(std::string_view h, std::string_view n) {
    return h.find(n) != std::string_view::npos;
}

bool Memmem(std::string_view h, std::string_view n) {
    return memmem(h.data(), h.size(), n.data(), n.size()) != nullptr;
}

bool Luvme(std::string_view h, std::string_view n) {
    return luvme::StringContains(h, n);
}

// ---- Input generation ------------------------------------------------------
// The haystack only contains 'a'..'z'. The needle has '#' in its middle, so:
//  - a "miss" is guaranteed (no '#' in the haystack),
//  - a "hit" happens exactly where we plant the needle.
// Random letters at the first/last needle bytes still produce false
// candidates for SIMD/memchr-style searches, so the verification path is exercised.
enum Mode : int { kMiss = 0, kHitStart = 1, kHitMiddle = 2, kHitEnd = 3 };

struct Input {
    std::string hay;
    std::string needle;
    size_t scanned;  // bytes the search has to cover before it can stop
    bool expected;
};

Input MakeInput(size_t hay_n, size_t k, int mode) {
    std::mt19937_64 rng(42);  // fixed seed: same data for every implementation
    std::uniform_int_distribution<int> letter('a', 'z');

    Input in;
    in.hay.resize(hay_n);
    for (auto& c : in.hay) c = static_cast<char>(letter(rng));
    in.needle.resize(k);
    for (auto& c : in.needle) c = static_cast<char>(letter(rng));
    in.needle[k / 2] = '#';

    switch (mode) {
        case kMiss:
            in.scanned = hay_n;
            in.expected = false;
            return in;
        case kHitStart:  break;
        case kHitMiddle: break;
        case kHitEnd:    break;
    }
    size_t pos = mode == kHitStart ? 0
               : mode == kHitMiddle ? (hay_n - k) / 2
                                    : hay_n - k;
    std::memcpy(in.hay.data() + pos, in.needle.data(), k);
    in.scanned = pos + k;
    in.expected = true;
    return in;
}

// ---- One benchmark body, instantiated per implementation -------------------
template <bool (*Fn)(std::string_view, std::string_view)>
void BM_Contains(benchmark::State& state) {
    const size_t hay_n = state.range(0);
    const size_t k = state.range(1);
    const int mode = state.range(2);

    if (k > hay_n) {
        state.SkipWithError("needle longer than haystack");
        return;
    }

    const Input in = MakeInput(hay_n, k, mode);
    const std::string_view hay = in.hay;
    const std::string_view needle = in.needle;

    // Sanity check: a fast wrong answer is worthless.
    if (Fn(hay, needle) != in.expected) {
        state.SkipWithError("wrong result");
        return;
    }

    for (auto _ : state) {
        // Make the inputs "escape" each iteration so the compiler cannot
        // hoist the call out of the loop or constant-fold it.
        benchmark::DoNotOptimize(hay.data());
        benchmark::DoNotOptimize(needle.data());
        bool r = Fn(hay, needle);
        benchmark::DoNotOptimize(r);
    }

    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(in.scanned));
    state.counters["mode"] = mode;  // 0=miss 1=start 2=middle 3=end
}

void Params(benchmark::internal::Benchmark* b) {
    b->ArgNames({"hay", "needle", "mode"})
     ->ArgsProduct({
         {64, 1 << 10, 1 << 16, 1 << 20},  // haystack size: L1, L1/L2, L2/L3, beyond
         {2, 8, 32},                        // needle length
         {kMiss, kHitStart, kHitMiddle, kHitEnd},
     });
}

}  // namespace

BENCHMARK(BM_Contains<Luvme>)->Name("Luvme")->Apply(Params);
BENCHMARK(BM_Contains<StdFind>)->Name("StdFind")->Apply(Params);
BENCHMARK(BM_Contains<Memmem>)->Name("Memmem")->Apply(Params);