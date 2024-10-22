// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/vector_icon.h"

#include "base/memory/ptr_util.h"
#include "base/test/gtest_util.h"
#include "cc/test/test_skcanvas.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/fake_ui_element_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "components/vector_icons/vector_icons.h"

namespace vr {

namespace {

using testing::_;
using testing::InSequence;

static constexpr int kMaximumWidth = 512;

class TestVectorIcon : public VectorIcon {
 public:
  explicit TestVectorIcon(int maximum_width) : VectorIcon(maximum_width) {}
  ~TestVectorIcon() override {}

  UiTexture* GetTexture() const override { return VectorIcon::GetTexture(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestVectorIcon);
};

}  // namespace

TEST(VectorIcon, SmokeTest) {
  UiScene scene;
  auto icon = base::MakeUnique<TestVectorIcon>(kMaximumWidth);
  icon->SetInitializedForTesting();
  icon->SetIcon(vector_icons::kClose16Icon);
  UiTexture* texture = icon->GetTexture();
  scene.AddUiElement(kRoot, std::move(icon));
  base::TimeTicks start_time = MsToTicks(1);
  scene.OnBeginFrame(start_time, kStartHeadPose);

  InSequence scope;
  cc::MockCanvas canvas;

  // This is the clearing of the canvas.
  EXPECT_CALL(canvas, OnDrawPaintWithColor(0));

  // The drawing of vector icons is bookended with a scoped save layer.
  EXPECT_CALL(canvas, willSave());

  // This matrix is concatenated to apply to the vector icon.
  EXPECT_CALL(canvas, didConcat(_));

  // This is the call to draw the path comprising the vector icon.
  EXPECT_CALL(canvas, onDrawPath(_, _));

  // The drawing of vector icons is bookended with a scoped save layer.
  EXPECT_CALL(canvas, willRestore());

  texture->DrawAndLayout(&canvas,
                         texture->GetPreferredTextureSize(kMaximumWidth));
}

}  // namespace vr
