#include "test.hpp"

#include <random>

namespace luvme {
    string RandomString(usize size) {
        // keyboard smash
        constexpr auto kAlphabet = "&é\"'(-è_çà)=azertyuiopqsdfghjklmù*wxcvbn,;:!"sv;
        auto result = string(size, 'a');
        auto rng = mt19937();
        auto dist = uniform_int_distribution<usize>(0, kAlphabet.size() - 1);

        for (usize i = 0; i != size; ++i) {
            result[i] = kAlphabet[dist(rng)];
        }

        return result;
    }
}
