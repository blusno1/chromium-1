// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_OMNIBOX_FOCUSER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_OMNIBOX_FOCUSER_H_

#import <Foundation/Foundation.h>

// This protocol provides callbacks for focusing and blurring the omnibox.
@protocol OmniboxFocuser
// Give focus to the omnibox, if it is visible. No-op if it is not visible.
- (void)focusOmnibox;
// Cancel omnibox edit (from shield tap or cancel button tap).
- (void)cancelOmniboxEdit;
// Give focus to the omnibox, but indicate that the focus event was initiated
// from the fakebox on the Google landing page.
- (void)focusFakebox;
// Hides the toolbar when the fakebox is blurred.
- (void)onFakeboxBlur;
// Shows the toolbar when the fakebox has animated to full bleed.
- (void)onFakeboxAnimationComplete;
@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_OMNIBOX_FOCUSER_H_
