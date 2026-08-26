// MobileGL - MobileGL/MG_Test/Program/PZF1V1ActivationTest.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only

#include <MG_Util/ShaderTranspiler/EsslBuiltinFunctionNames.h>

#include <gtest/gtest.h>

TEST(PZF1V1ActivationTest, V1EnablesAcceptedProjectZomboidMathNames) {
    using MobileGL::MG_Util::ShaderTranspiler::IsLexicalPreemptRenameName;

    EXPECT_TRUE(IsLexicalPreemptRenameName("clamp"));
    EXPECT_TRUE(IsLexicalPreemptRenameName("max"));
    EXPECT_TRUE(IsLexicalPreemptRenameName("min"));

    // Keep the V1 expansion at the accepted three-name PZF1 scope.
    EXPECT_FALSE(IsLexicalPreemptRenameName("floor"));
    EXPECT_FALSE(IsLexicalPreemptRenameName("sqrt"));
}
