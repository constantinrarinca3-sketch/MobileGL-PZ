// MobileGL-PZ - bounded BUG-002/BUG-003 program identities.
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once

#include <Includes.h>
#include <MG_State/GLState/ProgramState/ShaderStage.h>

namespace MobilePZ::Bug002Bug003 {
    // Program fingerprints captured by ROUTE-CENSUS-V2.  They are FNV-1a over
    // every attached shader in attachment order: the four little-endian bytes
    // of ShaderStage followed by the untouched application shader source.
    inline constexpr MobileGL::Uint64 kWeatherFxProgramFingerprint =
        0x91209697089dc186ull;
    inline constexpr MobileGL::Uint64 kHorseQuadProgramFingerprintA =
        0xb237c529153c6184ull;
    inline constexpr MobileGL::Uint64 kHorseQuadProgramFingerprintB =
        0x26de7f3b3457067dull;

    enum class ProgramRole : MobileGL::Uint8 {
        None = 0,
        WeatherFx,
        HorseQuad,
    };

    inline void HashByte(MobileGL::Uint64& hash, MobileGL::Uint8 byte) {
        hash ^= static_cast<MobileGL::Uint64>(byte);
        hash *= 1099511628211ull;
    }

    inline void HashShader(MobileGL::Uint64& hash, MobileGL::ShaderStage stage,
                           const MobileGL::String& source) {
        const MobileGL::Uint32 stageValue =
            static_cast<MobileGL::Uint32>(stage);
        HashByte(hash, static_cast<MobileGL::Uint8>(stageValue));
        HashByte(hash, static_cast<MobileGL::Uint8>(stageValue >> 8u));
        HashByte(hash, static_cast<MobileGL::Uint8>(stageValue >> 16u));
        HashByte(hash, static_cast<MobileGL::Uint8>(stageValue >> 24u));
        for (const unsigned char byte : source) {
            HashByte(hash, static_cast<MobileGL::Uint8>(byte));
        }
    }

    inline ProgramRole ClassifyProgram(MobileGL::Uint64 fingerprint) {
        if (fingerprint == kWeatherFxProgramFingerprint) {
            return ProgramRole::WeatherFx;
        }
        if (fingerprint == kHorseQuadProgramFingerprintA ||
            fingerprint == kHorseQuadProgramFingerprintB) {
            return ProgramRole::HorseQuad;
        }
        return ProgramRole::None;
    }
} // namespace MobilePZ::Bug002Bug003
