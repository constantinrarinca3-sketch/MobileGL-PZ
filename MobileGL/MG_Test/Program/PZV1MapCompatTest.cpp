// MobileGL - MobileGL/MG_Test/Program/PZV1MapCompatTest.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only

#include <MG_Util/PZV1/PZV1MapCompat.h>

#include <gtest/gtest.h>

namespace {
    using namespace MobilePZ::V1;

    TEST(PZV1MapCompatTest, ClassifiesOnlyExactCapturedShaderPairs) {
        EXPECT_EQ(ClassifyMapProgram(kMapVectorVertexHash, kMapVectorFragmentHash),
                  MapProgramRole::Vector);
        EXPECT_EQ(ClassifyMapProgram(kMapSdfQuadVertexHash, kMapSdfQuadFragmentHash),
                  MapProgramRole::SdfQuad);
        EXPECT_EQ(ClassifyMapProgram(kMapSdfTriVertexHash, kMapSdfTriFragmentHash),
                  MapProgramRole::SdfTri);

        EXPECT_EQ(ClassifyMapProgram(kMapVectorVertexHash,
                                     kMapVectorFragmentHash ^ 1),
                  MapProgramRole::None);
        EXPECT_EQ(ClassifyMapProgram(kMapSdfQuadVertexHash ^ 1,
                                     kMapSdfQuadFragmentHash),
                  MapProgramRole::None);
    }

    TEST(PZV1MapCompatTest, AcceptsOnlyCompleteQuadBatches) {
        EXPECT_TRUE(IsQuadBatch(GL_QUADS, 4));
        EXPECT_TRUE(IsQuadBatch(GL_QUADS, 8));
        EXPECT_TRUE(IsQuadBatch(GL_QUADS, 64));
        EXPECT_FALSE(IsQuadBatch(GL_QUADS, 0));
        EXPECT_FALSE(IsQuadBatch(GL_QUADS, 3));
        EXPECT_FALSE(IsQuadBatch(GL_QUADS, 6));
        EXPECT_FALSE(IsQuadBatch(GL_TRIANGLES, 4));
    }

    TEST(PZV1MapCompatTest, ExactSdfIdentityAcceptsQuadBatchesOnly) {
        EXPECT_TRUE(ShouldConvertMapSdfQuadBatch(
            MapProgramRole::SdfQuad, GL_QUADS, 4));
        EXPECT_TRUE(ShouldConvertMapSdfQuadBatch(
            MapProgramRole::SdfQuad, GL_QUADS, 12));
        EXPECT_FALSE(ShouldConvertMapSdfQuadBatch(
            MapProgramRole::Vector, GL_QUADS, 4));
        EXPECT_FALSE(ShouldConvertMapSdfQuadBatch(
            MapProgramRole::SdfQuad, GL_TRIANGLES, 12));
        EXPECT_FALSE(ShouldConvertMapSdfQuadBatch(
            MapProgramRole::SdfQuad, GL_QUADS, 10));
    }

    TEST(PZV1MapCompatTest, MatchesOnlyUiWorldMapStencilState) {
        EXPECT_TRUE(IsWorldMapStencilSignature(true, false, true, 1));
        EXPECT_FALSE(IsWorldMapStencilSignature(false, false, true, 1));
        EXPECT_FALSE(IsWorldMapStencilSignature(true, true, true, 1));
        EXPECT_FALSE(IsWorldMapStencilSignature(true, false, false, 1));
        EXPECT_FALSE(IsWorldMapStencilSignature(true, false, true, 0));
    }

    TEST(PZV1MapCompatTest, RequiresTheCompleteVboRendererInterface) {
        EXPECT_TRUE(HasVboRendererProgramInterface(true, true, true, true));
        EXPECT_FALSE(HasVboRendererProgramInterface(false, true, true, true));
        EXPECT_FALSE(HasVboRendererProgramInterface(true, false, true, true));
        EXPECT_FALSE(HasVboRendererProgramInterface(true, true, false, true));
        EXPECT_FALSE(HasVboRendererProgramInterface(true, true, true, false));
    }

    TEST(PZV1MapCompatTest, WorldMapRouteNeedsStateInterfaceAndQuadBatch) {
        EXPECT_TRUE(ShouldConvertWorldMapVboQuadBatch(
            GL_QUADS, 4, true, true));
        EXPECT_TRUE(ShouldConvertWorldMapVboQuadBatch(
            GL_QUADS, 20, true, true));
        EXPECT_FALSE(ShouldConvertWorldMapVboQuadBatch(
            GL_QUADS, 4, false, true));
        EXPECT_FALSE(ShouldConvertWorldMapVboQuadBatch(
            GL_QUADS, 4, true, false));
        EXPECT_FALSE(ShouldConvertWorldMapVboQuadBatch(
            GL_QUADS, 6, true, true));
        EXPECT_FALSE(ShouldConvertWorldMapVboQuadBatch(
            GL_TRIANGLES, 4, true, true));
    }
} // namespace
