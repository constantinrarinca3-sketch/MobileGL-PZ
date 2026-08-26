// MobilePZ PZF5 - GL-independent RGBA8 readback statistics.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace MobilePZ::PZF5 {

struct ReadbackStats {
    std::uint64_t pixels = 0;
    std::uint64_t zeroRgba = 0;
    std::uint64_t rgbNonzero = 0;
    std::uint64_t alphaZero = 0;
    std::uint64_t alphaMid = 0;
    std::uint64_t alphaFull = 0;
    std::uint64_t nonTransparentColour = 0;
    std::uint64_t hash = 1469598103934665603ull;
    std::uint64_t channelSum[4]{};
    std::uint8_t channelMin[4]{255, 255, 255, 255};
    std::uint8_t channelMax[4]{};

    void Observe(const std::uint8_t* rgba, std::size_t pixelCount) {
        if (!rgba) return;
        for (std::size_t i = 0; i < pixelCount; ++i) {
            const std::uint8_t* pixel = rgba + i * 4;
            const bool rgb = pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0;
            const bool zero = !rgb && pixel[3] == 0;
            ++pixels;
            if (zero) ++zeroRgba;
            if (rgb) ++rgbNonzero;
            if (pixel[3] == 0) ++alphaZero;
            else if (pixel[3] == 255) ++alphaFull;
            else ++alphaMid;
            if (rgb && pixel[3] != 0) ++nonTransparentColour;
            for (std::size_t channel = 0; channel < 4; ++channel) {
                channelSum[channel] += pixel[channel];
                channelMin[channel] = std::min(channelMin[channel], pixel[channel]);
                channelMax[channel] = std::max(channelMax[channel], pixel[channel]);
                hash ^= pixel[channel];
                hash *= 1099511628211ull;
            }
        }
    }
};

} // namespace MobilePZ::PZF5
