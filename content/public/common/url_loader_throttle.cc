// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/url_loader_throttle.h"

#include "base/logging.h"

namespace content {

void URLLoaderThrottle::Delegate::SetPriority(net::RequestPriority priority) {}
void URLLoaderThrottle::Delegate::PauseReadingBodyFromNet() {}
void URLLoaderThrottle::Delegate::ResumeReadingBodyFromNet() {}

URLLoaderThrottle::Delegate::~Delegate() {}

URLLoaderThrottle::~URLLoaderThrottle() {}

void URLLoaderThrottle::DetachFromCurrentSequence() {
  NOTREACHED();
}

void URLLoaderThrottle::WillStartRequest(ResourceRequest* request,
                                         bool* defer) {}

void URLLoaderThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head,
    bool* defer) {}

void URLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    const ResourceResponseHead& response_head,
    bool* defer) {}

URLLoaderThrottle::URLLoaderThrottle() {}

}  // namespace content
