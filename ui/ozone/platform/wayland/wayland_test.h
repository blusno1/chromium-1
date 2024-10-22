// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_TEST_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_TEST_H_

#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_features.h"
#include "ui/ozone/platform/wayland/fake_server.h"
#include "ui/ozone/platform/wayland/mock_platform_window_delegate.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"
#endif

namespace ui {

const uint32_t kXdgShellV5 = 5;
const uint32_t kXdgShellV6 = 6;

// WaylandTest is a base class that sets up a display, window, and fake server,
// and allows easy synchronization between them.
class WaylandTest : public ::testing::TestWithParam<uint32_t> {
 public:
  WaylandTest();
  ~WaylandTest() override;

  void SetUp() override;
  void TearDown() override;

  void Sync();

 private:
  bool initialized = false;
  base::MessageLoopForUI message_loop;

 protected:
  wl::FakeServer server;
  wl::MockSurface* surface;

  MockPlatformWindowDelegate delegate;
  std::unique_ptr<WaylandConnection> connection;
  std::unique_ptr<WaylandWindow> window;
  gfx::AcceleratedWidget widget = gfx::kNullAcceleratedWidget;

 private:
#if BUILDFLAG(USE_XKBCOMMON)
  XkbEvdevCodes xkb_evdev_code_converter_;
#endif

  DISALLOW_COPY_AND_ASSIGN(WaylandTest);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_TEST_H_
