// Standalone differential harness for OPT-LAB-V3-022A/022B.
//
// Build once against the V3-021 source (SELFTEST_DIRECT=0) and once against
// V3-022 (SELFTEST_DIRECT=1), then diff stdout.  The direct build additionally
// checks every byte outside the requested destination rows remains untouched.

#include <MG_Util/Texture/PixelStoreProcessor.h>

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#ifndef SELFTEST_DIRECT
#define SELFTEST_DIRECT 0
#endif

namespace MobileGL::MG_Util::Debug {
    void Log(const char*, android_LogPriority, const char*, ...) {}
}

namespace {
    using namespace MobileGL;

    constexpr Uint8 kSentinel = 0xCD;

    struct Case {
        const char* name;
        PixelStoreParameters params;
        TextureInternalFormat internalFormat;
        TextureInputFormat inputFormat;
        TexturePixelDataType inputType;
        IntVec3 dimensions;
        Bool bitmap;
        SizeT destinationPixelSize;
        SizeT destinationRowStride;
        SizeT destinationImageStride;
        SizeT inputBytes;
    };

    std::vector<Uint8> MakeInput(SizeT size, Uint8 seed) {
        std::vector<Uint8> bytes(size);
        Uint32 state = 0x9E3779B9u ^ seed;
        for (SizeT i = 0; i < size; ++i) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            bytes[i] = static_cast<Uint8>((state + static_cast<Uint32>(i * 29u)) & 0xFFu);
        }
        return bytes;
    }

    void PrintHex(const Case& test, const Uint8* bytes, SizeT size) {
        std::cout << test.name << ':';
        const auto oldFlags = std::cout.flags();
        const auto oldFill = std::cout.fill();
        for (SizeT i = 0; i < size; ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(bytes[i]);
        }
        std::cout.flags(oldFlags);
        std::cout.fill(oldFill);
        std::cout << '\n';
    }

    [[noreturn]] void Fail(const Case& test, const std::string& message) {
        std::cerr << "SELFTEST_FAIL " << test.name << ": " << message << '\n';
        std::exit(1);
    }

    void Run(const Case& test, Uint8 seed) {
        auto input = MakeInput(test.inputBytes, seed);
        SizeT outputSize = 0;

#if SELFTEST_DIRECT
        constexpr SizeT prefix = 11;
        constexpr SizeT suffix = 13;
        const SizeT depth = static_cast<SizeT>(test.dimensions.z());
        const SizeT height = static_cast<SizeT>(test.dimensions.y());
        const SizeT width = static_cast<SizeT>(test.dimensions.x());
        const SizeT rowBytes = width * test.destinationPixelSize;
        const SizeT payloadBytes = depth * test.destinationImageStride;
        std::vector<Uint8> destination(prefix + payloadBytes + suffix, kSentinel);

        const Bool ok = MG_Util::PixelStoreProcessor::ProcessTexturePixelsDataUnpackInto(
            input.data(), test.params, test.internalFormat, test.inputFormat, test.inputType,
            test.dimensions, test.bitmap, destination.data() + prefix, test.destinationPixelSize,
            test.destinationRowStride, test.destinationImageStride, outputSize);
        if (!ok) Fail(test, "direct route rejected a valid case");
        const SizeT expectedSize = width * height * depth * test.destinationPixelSize;
        if (outputSize != expectedSize) Fail(test, "unexpected logical output size");

        std::vector<Bool> mayChange(destination.size(), false);
        std::vector<Uint8> compact;
        compact.reserve(expectedSize);
        for (SizeT z = 0; z < depth; ++z) {
            for (SizeT y = 0; y < height; ++y) {
                const SizeT rowStart = prefix + z * test.destinationImageStride + y * test.destinationRowStride;
                for (SizeT x = 0; x < rowBytes; ++x) mayChange[rowStart + x] = true;
                compact.insert(compact.end(), destination.begin() + static_cast<std::ptrdiff_t>(rowStart),
                               destination.begin() + static_cast<std::ptrdiff_t>(rowStart + rowBytes));
            }
        }
        for (SizeT i = 0; i < destination.size(); ++i) {
            if (!mayChange[i] && destination[i] != kSentinel) {
                Fail(test, "write escaped the requested rows at byte " + std::to_string(i));
            }
        }
        PrintHex(test, compact.data(), compact.size());
#else
        void* output = MG_Util::PixelStoreProcessor::ProcessTexturePixelsDataUnpack(
            input.data(), test.params, test.internalFormat, test.inputFormat, test.inputType,
            test.dimensions, test.bitmap, outputSize);
        if (!output || outputSize == 0) Fail(test, "legacy oracle rejected a valid case");
        PrintHex(test, static_cast<const Uint8*>(output), outputSize);
        std::free(output);
#endif
    }

#if SELFTEST_DIRECT
    void RunRejectionChecks() {
        Case invalid{"reject_short_stride", {}, TextureInternalFormat::RGBA8, TextureInputFormat::RGBA,
                     TexturePixelDataType::UnsignedByte, {2, 2, 1}, false, 4, 7, 32, 16};
        auto input = MakeInput(invalid.inputBytes, 0xE1);
        std::vector<Uint8> destination(64, kSentinel);
        SizeT outputSize = std::numeric_limits<SizeT>::max();
        const Bool ok = MG_Util::PixelStoreProcessor::ProcessTexturePixelsDataUnpackInto(
            input.data(), invalid.params, invalid.internalFormat, invalid.inputFormat, invalid.inputType,
            invalid.dimensions, invalid.bitmap, destination.data(), invalid.destinationPixelSize,
            invalid.destinationRowStride, invalid.destinationImageStride, outputSize);
        if (ok || outputSize != 0) Fail(invalid, "short row stride was accepted");
        for (Uint8 byte : destination) {
            if (byte != kSentinel) Fail(invalid, "rejected write modified destination");
        }

        invalid.name = "reject_wrong_bpp";
        invalid.destinationRowStride = 16;
        outputSize = std::numeric_limits<SizeT>::max();
        const Bool bppOk = MG_Util::PixelStoreProcessor::ProcessTexturePixelsDataUnpackInto(
            input.data(), invalid.params, invalid.internalFormat, invalid.inputFormat, invalid.inputType,
            invalid.dimensions, invalid.bitmap, destination.data(), 3, invalid.destinationRowStride,
            invalid.destinationImageStride, outputSize);
        if (bppOk || outputSize != 0) Fail(invalid, "wrong destination bpp was accepted");
        for (Uint8 byte : destination) {
            if (byte != kSentinel) Fail(invalid, "rejected bpp write modified destination");
        }
    }
#endif
} // namespace

int main() {
    using namespace MobileGL;

    PixelStoreParameters padded{};
    padded.RowLength = 5;
    padded.SkipPixels = 1;
    padded.SkipRows = 1;
    padded.Alignment = 8;

    PixelStoreParameters redSkips{};
    redSkips.RowLength = 7;
    redSkips.SkipPixels = 2;
    redSkips.SkipRows = 1;
    redSkips.Alignment = 4;

    PixelStoreParameters volume{};
    volume.RowLength = 4;
    volume.ImageHeight = 5;
    volume.SkipPixels = 1;
    volume.SkipRows = 1;
    volume.SkipImages = 1;
    volume.Alignment = 4;

    PixelStoreParameters swap{};
    swap.SwapBytes = true;
    swap.Alignment = 4;

    PixelStoreParameters bitmap{};
    bitmap.LSBFirst = true;
    bitmap.Alignment = 4;

    const std::vector<Case> cases = {
        {"rgba8_tight", {}, TextureInternalFormat::RGBA8, TextureInputFormat::RGBA,
         TexturePixelDataType::UnsignedByte, {3, 2, 1}, false, 4, 17, 43, 24},
        {"rgba8_rowlength_skips", padded, TextureInternalFormat::RGBA8, TextureInputFormat::RGBA,
         TexturePixelDataType::UnsignedByte, {2, 2, 1}, false, 4, 13, 37, 80},
        {"red_to_rgba8_skips", redSkips, TextureInternalFormat::RGBA8, TextureInputFormat::Red,
         TexturePixelDataType::UnsignedByte, {3, 2, 1}, false, 4, 19, 47, 48},
        {"rgb_to_rgba8_3d", volume, TextureInternalFormat::RGBA8, TextureInputFormat::RGB,
         TexturePixelDataType::UnsignedByte, {2, 2, 2}, false, 4, 15, 41, 180},
        {"r16ui_swap", swap, TextureInternalFormat::R16UI, TextureInputFormat::RInteger,
         TexturePixelDataType::UnsignedShort, {3, 2, 1}, false, 2, 11, 29, 16},
        {"bgra_8888_rev", {}, TextureInternalFormat::RGBA8, TextureInputFormat::BGRA,
         TexturePixelDataType::UnsignedInt8888Rev, {3, 2, 1}, false, 4, 17, 43, 24},
        {"depth_float_to_d24s8", {}, TextureInternalFormat::Depth24Stencil8,
         TextureInputFormat::DepthComponent, TexturePixelDataType::Float,
         {3, 2, 1}, false, 4, 17, 43, 24},
        {"bitmap_lsb", bitmap, TextureInternalFormat::R8, TextureInputFormat::Red,
         TexturePixelDataType::UnsignedByte, {16, 2, 1}, true, 1, 21, 53, 32},
    };

    Uint8 seed = 1;
    for (const auto& test : cases) Run(test, seed++);
#if SELFTEST_DIRECT
    RunRejectionChecks();
#endif
    return 0;
}
