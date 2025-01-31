// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/web_view_web_client.h"

#import "ios/web/public/test/js_test_util.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/public/web_view_creation_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

class WebViewWebClientTest : public web::WebTest {
 public:
  WebViewWebClientTest() {}
  ~WebViewWebClientTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewWebClientTest);
};

// Tests that WebViewWebClient provides autofill controller script for
// WKWebView.
TEST_F(WebViewWebClientTest, WKWebViewEarlyPageScriptAutofillController) {
  // WebView scripts rely on __gCrWeb object presence.
  WKWebView* web_view = web::BuildWKWebView(CGRectZero, GetBrowserState());
  web::ExecuteJavaScript(web_view, @"__gCrWeb = {};");

  web::ScopedTestingWebClient web_client(std::make_unique<WebViewWebClient>());
  NSString* script =
      web_client.Get()->GetEarlyPageScriptForMainFrame(GetBrowserState());
  web::ExecuteJavaScript(web_view, script);
  EXPECT_NSEQ(@"object",
              web::ExecuteJavaScript(web_view, @"typeof __gCrWeb.autofill"));
}

}  // namespace ios_web_view
