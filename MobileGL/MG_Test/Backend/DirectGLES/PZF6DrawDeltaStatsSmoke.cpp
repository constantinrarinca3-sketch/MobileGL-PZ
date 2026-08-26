#include "../../../MG_Backend/DirectGLES/PZF6DrawDeltaStats.h"

#include <cstdio>
#include <cstdlib>

using MobilePZ::PZF6::DrawDeltaStats;

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "PZF6_DRAW_DELTA_STATS_SMOKE_FAIL %s\n", message);
    std::exit(1);
}

int main() {
    const std::uint8_t before[] = {
        0, 0, 0, 0,
        1, 2, 3, 255,
        5, 0, 0, 255,
        10, 10, 10, 128,
    };
    const std::uint8_t after[] = {
        10, 20, 30, 255,
        1, 2, 3, 255,
        0, 0, 0, 0,
        11, 10, 9, 128,
    };
    DrawDeltaStats stats;
    stats.Observe(before, after, 4);
    Require(stats.pixels == 4 && stats.changed == 3 && stats.unchanged == 1,
            "pixel_buckets");
    Require(stats.rgbChanged == 3 && stats.alphaChanged == 2, "channel_change_buckets");
    Require(stats.rgbGain == 1 && stats.rgbLoss == 1, "rgb_gain_loss");
    Require(stats.alphaGain == 1 && stats.alphaLoss == 1, "alpha_gain_loss");
    Require(stats.nonTransparentGain == 1 && stats.nonTransparentLoss == 1,
            "nontransparent_gain_loss");
    Require(stats.channelSignedDelta[0] == 6 && stats.channelSignedDelta[1] == 20 &&
            stats.channelSignedDelta[2] == 29 && stats.channelSignedDelta[3] == 0,
            "signed_channel_delta");
    Require(stats.channelAbsoluteDelta[0] == 16 && stats.channelAbsoluteDelta[1] == 20 &&
            stats.channelAbsoluteDelta[2] == 31 && stats.channelAbsoluteDelta[3] == 510,
            "absolute_channel_delta");
    std::puts("PZF6_DRAW_DELTA_STATS_SMOKE stats=PASS pixels=4 changed=3 unchanged=1 "
              "rgb_changed=3 alpha_changed=2");
    return 0;
}
