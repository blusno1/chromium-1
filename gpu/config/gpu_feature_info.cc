// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_feature_info.h"

#include <algorithm>

#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "ui/gl/gl_context.h"

namespace gpu {

GpuFeatureInfo::GpuFeatureInfo() {
  for (auto& status : status_values)
    status = kGpuFeatureStatusUndefined;
}

GpuFeatureInfo::GpuFeatureInfo(const GpuFeatureInfo&) = default;

GpuFeatureInfo::GpuFeatureInfo(GpuFeatureInfo&&) = default;

GpuFeatureInfo::~GpuFeatureInfo() = default;

GpuFeatureInfo& GpuFeatureInfo::operator=(const GpuFeatureInfo&) = default;

GpuFeatureInfo& GpuFeatureInfo::operator=(GpuFeatureInfo&&) = default;

void GpuFeatureInfo::ApplyToGLContext(gl::GLContext* gl_context) const {
  DCHECK(gl_context);
  gl::GLWorkarounds gl_workarounds;
  if (IsWorkaroundEnabled(gpu::CLEAR_TO_ZERO_OR_ONE_BROKEN)) {
    gl_workarounds.clear_to_zero_or_one_broken = true;
  }
  if (IsWorkaroundEnabled(RESET_TEXIMAGE2D_BASE_LEVEL)) {
    gl_workarounds.reset_teximage2d_base_level = true;
  }
  gl_context->SetGLWorkarounds(gl_workarounds);
  gl_context->SetDisabledGLExtensions(this->disabled_extensions);
}

bool GpuFeatureInfo::IsWorkaroundEnabled(int32_t workaround) const {
  return std::find(this->enabled_gpu_driver_bug_workarounds.begin(),
                   this->enabled_gpu_driver_bug_workarounds.end(),
                   workaround) !=
         this->enabled_gpu_driver_bug_workarounds.end();
}

bool GpuFeatureInfo::IsValid() const {
  // Check if any feature status is undefined.
  return status_values[GPU_FEATURE_TYPE_GPU_COMPOSITING] !=
         kGpuFeatureStatusUndefined;
}

}  // namespace gpu
