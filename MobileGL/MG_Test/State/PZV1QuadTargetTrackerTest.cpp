// MobileGL - MobileGL/MG_Test/State/PZV1QuadTargetTrackerTest.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only

#include <MG_Util/PZV1/PZV1QuadTargetTracker.h>

#include <gtest/gtest.h>

namespace {
    using namespace MobilePZ::V1;

    class PZV1QuadTargetTrackerTest : public testing::Test {
    protected:
        void SetUp() override { Reset(); }
        void TearDown() override { Reset(); }
    };

    TEST_F(PZV1QuadTargetTrackerTest, CompletedWindowPublishesOneConsumer) {
        constexpr MobileGL::Uint fbo = 7;
        constexpr MobileGL::Uint64 lifetime = 101;
        constexpr MobileGL::Int attachment = 7;

        RecordNullAllocation(lifetime);
        ActivateFramebuffer(fbo, lifetime, attachment);
        MarkFramebufferQuadWritten(fbo);
        PublishAndDeactivateFramebuffer(fbo, lifetime, attachment);

        EXPECT_FALSE(IsActiveFramebuffer(fbo));
        EXPECT_TRUE(TryConsumeCompositeTexture(lifetime));
        EXPECT_FALSE(TryConsumeCompositeTexture(lifetime));
    }

    TEST_F(PZV1QuadTargetTrackerTest, ClearOnlyWindowDoesNotPublish) {
        constexpr MobileGL::Uint fbo = 9;
        constexpr MobileGL::Uint64 lifetime = 202;
        constexpr MobileGL::Int attachment = 7;

        RecordNullAllocation(lifetime);
        ActivateFramebuffer(fbo, lifetime, attachment);
        PublishAndDeactivateFramebuffer(fbo, lifetime, attachment);

        EXPECT_FALSE(TryConsumeCompositeTexture(lifetime));
    }

    TEST_F(PZV1QuadTargetTrackerTest, MismatchedDetachCannotPublish) {
        constexpr MobileGL::Uint fbo = 11;
        constexpr MobileGL::Uint64 lifetime = 303;
        constexpr MobileGL::Int attachment = 7;

        RecordNullAllocation(lifetime);
        ActivateFramebuffer(fbo, lifetime, attachment);
        MarkFramebufferQuadWritten(fbo);
        PublishAndDeactivateFramebuffer(fbo, lifetime, attachment + 1);

        EXPECT_TRUE(IsActiveFramebuffer(fbo));
        EXPECT_FALSE(TryConsumeCompositeTexture(lifetime));
        PublishAndDeactivateFramebuffer(fbo, lifetime, attachment);
        EXPECT_TRUE(TryConsumeCompositeTexture(lifetime));
    }

    TEST_F(PZV1QuadTargetTrackerTest, StorageRedefinitionInvalidatesHandoff) {
        constexpr MobileGL::Uint fbo = 13;
        constexpr MobileGL::Uint64 lifetime = 404;
        constexpr MobileGL::Int attachment = 7;

        RecordNullAllocation(lifetime);
        ActivateFramebuffer(fbo, lifetime, attachment);
        MarkFramebufferQuadWritten(fbo);
        PublishAndDeactivateFramebuffer(fbo, lifetime, attachment);
        RecordDefinedAllocation(lifetime);

        EXPECT_FALSE(TryConsumeCompositeTexture(lifetime));
    }

    TEST_F(PZV1QuadTargetTrackerTest, NewProducerWindowInvalidatesStaleHandoff) {
        constexpr MobileGL::Uint fbo = 14;
        constexpr MobileGL::Uint64 lifetime = 505;
        constexpr MobileGL::Int attachment = 7;

        RecordNullAllocation(lifetime);
        ActivateFramebuffer(fbo, lifetime, attachment);
        MarkFramebufferQuadWritten(fbo);
        PublishAndDeactivateFramebuffer(fbo, lifetime, attachment);
        ActivateFramebuffer(fbo, lifetime, attachment);

        EXPECT_FALSE(TryConsumeCompositeTexture(lifetime));
        EXPECT_TRUE(IsActiveFramebuffer(fbo));
    }

    TEST_F(PZV1QuadTargetTrackerTest, DirectMapCollisionIsFailClosed) {
        constexpr MobileGL::Uint64 firstLifetime = 1;
        constexpr MobileGL::Uint64 secondLifetime =
            firstLifetime + kCompositeTextureSlots;

        RecordNullAllocation(firstLifetime);
        ActivateFramebuffer(15, firstLifetime, 7);
        MarkFramebufferQuadWritten(15);
        PublishAndDeactivateFramebuffer(15, firstLifetime, 7);

        RecordNullAllocation(secondLifetime);
        ActivateFramebuffer(16, secondLifetime, 7);
        MarkFramebufferQuadWritten(16);
        PublishAndDeactivateFramebuffer(16, secondLifetime, 7);

        EXPECT_FALSE(TryConsumeCompositeTexture(firstLifetime));
        EXPECT_TRUE(TryConsumeCompositeTexture(secondLifetime));
    }
} // namespace
