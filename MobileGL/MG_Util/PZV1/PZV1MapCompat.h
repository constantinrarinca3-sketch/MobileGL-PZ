// MobileGL - MobileGL/MG_Util/PZV1/PZV1MapCompat.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once

#include <Includes.h>

namespace MobilePZ::V1 {
    // Hashes of the original Project Zomboid shader text before MobileGL
    // preprocessing.  The SDF identity preserves the already-proven label path
    // without restoring PZF16's global GL_QUADS mutation.
    inline constexpr MobileGL::Uint64 kMapVectorVertexHash = 0x7e4fe6ed7264947full;
    inline constexpr MobileGL::Uint64 kMapVectorFragmentHash = 0x3a458f7884568505ull;
    inline constexpr MobileGL::Uint64 kMapSdfQuadVertexHash = 0xfd62cd3081ecbee3ull;
    inline constexpr MobileGL::Uint64 kMapSdfQuadFragmentHash = 0x60544110691d0109ull;
    inline constexpr MobileGL::Uint64 kMapSdfTriVertexHash = 0x9213b8eca10fcc24ull;
    inline constexpr MobileGL::Uint64 kMapSdfTriFragmentHash = 0x26b3a3f6e59cc968ull;

    enum class MapProgramRole : MobileGL::Uint8 {
        None = 0,
        Vector,
        SdfQuad,
        SdfTri,
    };

    inline MobileGL::Uint64 HashShaderSource(const MobileGL::String& source) {
        MobileGL::Uint64 hash = 1469598103934665603ull;
        for (const unsigned char byte : source) {
            hash ^= static_cast<MobileGL::Uint64>(byte);
            hash *= 1099511628211ull;
        }
        return hash;
    }

    inline MapProgramRole ClassifyMapProgram(MobileGL::Uint64 vertexHash,
                                             MobileGL::Uint64 fragmentHash) {
        if (vertexHash == kMapVectorVertexHash &&
            fragmentHash == kMapVectorFragmentHash) {
            return MapProgramRole::Vector;
        }
        if (vertexHash == kMapSdfQuadVertexHash &&
            fragmentHash == kMapSdfQuadFragmentHash) {
            return MapProgramRole::SdfQuad;
        }
        if (vertexHash == kMapSdfTriVertexHash &&
            fragmentHash == kMapSdfTriFragmentHash) {
            return MapProgramRole::SdfTri;
        }
        return MapProgramRole::None;
    }

    // GL_QUADS describes one independent quad for every four indices/vertices.
    // A single triangle fan is equivalent only for one quad, so larger batches
    // must be split into four-vertex fans by the submission frontend.
    inline MobileGL::Bool IsQuadBatch(GLenum mode, GLsizei count) {
        return mode == GL_QUADS && count >= 4 && count % 4 == 0;
    }

    inline MobileGL::Bool ShouldConvertMapSdfQuadBatch(MapProgramRole role,
                                                        GLenum mode,
                                                        GLsizei count) {
        return role == MapProgramRole::SdfQuad && IsQuadBatch(mode, count);
    }

    // UIWorldMap establishes this state before rendering both the Create New
    // World preview and the in-game world map.  Requiring it keeps the repair
    // out of actor, building, inventory and generic SpriteRenderer submissions.
    inline MobileGL::Bool IsWorldMapStencilSignature(
        MobileGL::Bool stencilEnabled, MobileGL::Bool depthEnabled,
        MobileGL::Bool frontStencilIsEqual, MobileGL::Int frontStencilRef) {
        return stencilEnabled && !depthEnabled && frontStencilIsEqual &&
               frontStencilRef == 1;
    }

    inline MobileGL::Bool HasVboRendererProgramInterface(
        MobileGL::Bool hasPosition, MobileGL::Bool hasColor,
        MobileGL::Bool hasModelViewProjection, MobileGL::Bool hasUserDepth) {
        return hasPosition && hasColor && hasModelViewProjection && hasUserDepth;
    }

    inline MobileGL::Bool ShouldConvertWorldMapVboQuadBatch(
        GLenum mode, GLsizei count, MobileGL::Bool worldMapStencil,
        MobileGL::Bool vboRendererInterface) {
        return IsQuadBatch(mode, count) && worldMapStencil &&
               vboRendererInterface;
    }
} // namespace MobilePZ::V1
