#pragma once
#include "pch.hpp"

namespace luvme
{
// Host-endian (little-endian assumed). Fine until you care about portable files.
template <typename T> T Load(span<const u8> b, usize off)
{
    T v;
    memcpy(&v, b.data() + off, sizeof(T));
    return v;
}

template <typename T> void Store(span<u8> b, usize off, T v)
{
    memcpy(b.data() + off, &v, sizeof(T));
}
} // namespace luvme