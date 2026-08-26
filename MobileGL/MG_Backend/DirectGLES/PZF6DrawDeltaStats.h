// MobilePZ PZF6 - GL-independent before/after RGBA8 draw-delta statistics.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace MobilePZ::PZF6 {

struct DrawDeltaStats {
    std::uint64_t pixels = 0;
    std::uint64_t unchanged = 0;
    std::uint64_t changed = 0;
    std::uint64_t rgbChanged = 0;
    std::uint64_t alphaChanged = 0;
    std::uint64_t rgbGain = 0;
    std::uint64_t rgbLoss = 0;
    std::uint64_t alphaGain = 0;
    std::uint64_t alphaLoss = 0;
    std::uint64_t nonTransparentGain = 0;
    std::uint64_t nonTransparentLoss = 0;
    std::array<std::int64_t, 4> channelSignedDelta{};
    std::array<std::uint64_t, 4> channelAbsoluteDelta{};

    void Observe(const std::uint8_t* before, const std::uint8_t* after,
                 std::size_t pixelCount) {
        if (!before || !after) return;
        for (std::size_t i = 0; i < pixelCount; ++i) {
            const std::uint8_t* pre = before + i * 4;
            const std::uint8_t* post = after + i * 4;
            const bool anyChanged = pre[0] != post[0] || pre[1] != post[1] ||
                                    pre[2] != post[2] || pre[3] != post[3];
            const bool rgbChangedPixel = pre[0] != post[0] || pre[1] != post[1] ||
                                         pre[2] != post[2];
            const bool alphaChangedPixel = pre[3] != post[3];
            const bool preRgb = pre[0] != 0 || pre[1] != 0 || pre[2] != 0;
            const bool postRgb = post[0] != 0 || post[1] != 0 || post[2] != 0;
            const bool preAlpha = pre[3] != 0;
            const bool postAlpha = post[3] != 0;
            const bool preNonTransparent = preRgb && preAlpha;
            const bool postNonTransparent = postRgb && postAlpha;

            ++pixels;
            if (anyChanged) ++changed;
            else ++unchanged;
            if (rgbChangedPixel) ++rgbChanged;
            if (alphaChangedPixel) ++alphaChanged;
            if (!preRgb && postRgb) ++rgbGain;
            if (preRgb && !postRgb) ++rgbLoss;
            if (!preAlpha && postAlpha) ++alphaGain;
            if (preAlpha && !postAlpha) ++alphaLoss;
            if (!preNonTransparent && postNonTransparent) ++nonTransparentGain;
            if (preNonTransparent && !postNonTransparent) ++nonTransparentLoss;

            for (std::size_t channel = 0; channel < 4; ++channel) {
                const std::int64_t delta = static_cast<std::int64_t>(post[channel]) -
                                           static_cast<std::int64_t>(pre[channel]);
                channelSignedDelta[channel] += delta;
                channelAbsoluteDelta[channel] += static_cast<std::uint64_t>(
                    delta < 0 ? -delta : delta);
            }
        }
    }
};

} // namespace MobilePZ::PZF6
