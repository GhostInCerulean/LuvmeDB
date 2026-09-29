#pragma once
#include <boost/asio.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/leaf.hpp>
#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace luvme {
    using namespace std;

    using i8 = int8_t;
    using u8 = uint8_t;
    using i16 = int16_t;
    using u16 = uint16_t;
    using i32 = int32_t;
    using u32 = uint32_t;
    using i64 = int64_t;
    using u64 = uint64_t;
    using f32 = float;
    using f64 = double;
    using usize = size_t;
    using isize = ptrdiff_t;
}