// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/app_info_dialog.h"

#include <string>

#include "build/build_config.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/testing_profile.h"

#if defined(OS_MACOSX)
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#endif

class AppInfoDialogBrowserTest : public DialogBrowserTest {
 public:
  AppInfoDialogBrowserTest() {}

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    extension_environment_ =
        base::MakeUnique<extensions::TestExtensionEnvironment>(nullptr);
    constexpr char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    extension_ =
        extension_environment_->MakePackagedApp(kTestExtensionId, true);
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    ShowAppInfoInNativeDialog(
        web_contents, extension_environment_->profile(), extension_.get(),
        base::Bind(&AppInfoDialogBrowserTest::OnDialogClosed,
                   base::Unretained(this)));
  }

  void OnDialogClosed() { extension_environment_.reset(); }

#if defined(OS_MACOSX)
  // content::BrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableAppInfoDialogMac);
  }
#endif

 private:
  std::unique_ptr<extensions::TestExtensionEnvironment> extension_environment_;
  scoped_refptr<extensions::Extension> extension_;

  DISALLOW_COPY_AND_ASSIGN(AppInfoDialogBrowserTest);
};

// Invokes a dialog that shows details of an installed extension.
IN_PROC_BROWSER_TEST_F(AppInfoDialogBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
