#include "../../../MG_Backend/DirectGLES/PZF5ReadbackStats.h"

#include <cstdio>
#include <cstdlib>

using MobilePZ::PZF5::ReadbackStats;

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "PZF5_READBACK_STATS_SMOKE_FAIL %s\n", message);
    std::exit(1);
}

int main() {
    const std::uint8_t pixels[] = {
        0, 0, 0, 0,
        10, 0, 0, 0,
        0, 20, 0, 128,
        1, 2, 3, 255,
    };
    ReadbackStats stats;
    stats.Observe(pixels, 4);
    Require(stats.pixels == 4, "pixel_count");
    Require(stats.zeroRgba == 1, "zero_rgba");
    Require(stats.rgbNonzero == 3, "rgb_nonzero");
    Require(stats.alphaZero == 2 && stats.alphaMid == 1 && stats.alphaFull == 1,
            "alpha_buckets");
    Require(stats.nonTransparentColour == 2, "nontransparent_colour");
    Require(stats.channelMin[0] == 0 && stats.channelMax[0] == 10, "red_range");
    Require(stats.channelMin[1] == 0 && stats.channelMax[1] == 20, "green_range");
    Require(stats.channelMin[2] == 0 && stats.channelMax[2] == 3, "blue_range");
    Require(stats.channelMin[3] == 0 && stats.channelMax[3] == 255, "alpha_range");
    Require(stats.channelSum[0] == 11 && stats.channelSum[1] == 22 &&
            stats.channelSum[2] == 3 && stats.channelSum[3] == 383, "channel_sums");
    Require(stats.hash != 1469598103934665603ull, "hash_changed");
    std::puts("PZF5_READBACK_STATS_SMOKE stats=PASS pixels=4 zero_rgba=1 rgb_nonzero=3 "
              "alpha=2/1/1 nontransparent_colour=2");
    return 0;
}
