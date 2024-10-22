// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive/primary_toolbar_view.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrimaryToolbarView ()
// Container for the location bar.
@property(nonatomic, strong) UIView* locationBarContainer;

// StackView containing the leading buttons (relative to the location bar). It
// should only contain ToolbarButtons.
@property(nonatomic, strong) UIStackView* leadingStackView;
// Buttons from the leading stack view.
@property(nonatomic, strong) NSArray<ToolbarButton*>* leadingStackViewButtons;
// StackView containing the trailing buttons (relative to the location bar). It
// should only contain ToolbarButtons.
@property(nonatomic, strong) UIStackView* trailingStackView;
// Buttons from the trailing stack view.
@property(nonatomic, strong) NSArray<ToolbarButton*>* trailingStackViewButtons;

#pragma mark** Buttons in the leading stack view. **
// Button to navigate back.
@property(nonatomic, strong) ToolbarButton* backButton;
// Button to navigate forward, leading position.
@property(nonatomic, strong) ToolbarButton* forwardLeadingButton;
// Button to display the TabGrid.
@property(nonatomic, strong) ToolbarButton* tabGridButton;
// Button to stop the loading of the page.
@property(nonatomic, strong) ToolbarButton* stopButton;
// Button to reload the page.
@property(nonatomic, strong) ToolbarButton* reloadButton;

#pragma mark** Buttons in the trailing stack view. **
// Button to navigate forward, trailing position.
@property(nonatomic, strong) ToolbarButton* forwardTrailingButton;
// Button to display the share menu.
@property(nonatomic, strong) ToolbarButton* shareButton;
// Button to manage the bookmarks of this page.
@property(nonatomic, strong) ToolbarButton* bookmarkButton;
// Button to display the tools menu, redefined as readwrite.
@property(nonatomic, strong) ToolbarToolsMenuButton* toolsMenuButton;

@end

@implementation PrimaryToolbarView

@synthesize locationBarView = _locationBarView;
@synthesize topSafeAnchor = _topSafeAnchor;
@synthesize buttonFactory = _buttonFactory;
@synthesize allButtons = _allButtons;
@synthesize leadingStackView = _leadingStackView;
@synthesize leadingStackViewButtons = _leadingStackViewButtons;
@synthesize backButton = _backButton;
@synthesize forwardLeadingButton = _forwardLeadingButton;
@synthesize tabGridButton = _tabGridButton;
@synthesize stopButton = _stopButton;
@synthesize reloadButton = _reloadButton;
@synthesize locationBarContainer = _locationBarContainer;
@synthesize trailingStackView = _trailingStackView;
@synthesize trailingStackViewButtons = _trailingStackViewButtons;
@synthesize forwardTrailingButton = _forwardTrailingButton;
@synthesize shareButton = _shareButton;
@synthesize bookmarkButton = _bookmarkButton;
@synthesize toolsMenuButton = _toolsMenuButton;

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [self setUp];
  [super willMoveToSuperview:newSuperview];
}

#pragma mark - Setup

// Sets all the subviews and constraints of this view.
- (void)setUp {
  if (self.subviews.count > 0) {
    // Setup the view only once.
    return;
  }
  DCHECK(self.buttonFactory);

  self.translatesAutoresizingMaskIntoConstraints = NO;

  [self setUpLocationBar];
  [self setUpLeadingStackView];
  [self setUpTrailingStackView];

  [self setUpConstraints];
}

// Sets the location bar container and its view if present.
- (void)setUpLocationBar {
  self.locationBarContainer = [[UIView alloc] init];
  self.locationBarContainer.backgroundColor = [UIColor whiteColor];
  [self.locationBarContainer
      setContentHuggingPriority:UILayoutPriorityDefaultLow
                        forAxis:UILayoutConstraintAxisHorizontal];
  self.locationBarContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:self.locationBarContainer];

  if (self.locationBarView) {
    [self.locationBarContainer addSubview:self.locationBarView];
  }
}

// Sets the leading stack view.
- (void)setUpLeadingStackView {
  self.backButton = [self.buttonFactory backButton];
  self.forwardLeadingButton = [self.buttonFactory leadingForwardButton];
  self.tabGridButton = [self.buttonFactory tabSwitcherStripButton];
  self.stopButton = [self.buttonFactory stopButton];
  self.stopButton.hiddenInCurrentState = YES;
  self.reloadButton = [self.buttonFactory reloadButton];

  self.leadingStackViewButtons = @[
    self.backButton, self.forwardLeadingButton, self.tabGridButton,
    self.stopButton, self.reloadButton
  ];
  self.leadingStackView = [[UIStackView alloc]
      initWithArrangedSubviews:self.leadingStackViewButtons];
  self.leadingStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:self.leadingStackView];
}

// Sets the trailing stack view.
- (void)setUpTrailingStackView {
  self.forwardTrailingButton = [self.buttonFactory trailingForwardButton];
  self.shareButton = [self.buttonFactory shareButton];
  self.bookmarkButton = [self.buttonFactory bookmarkButton];
  self.toolsMenuButton = [self.buttonFactory toolsMenuButton];

  self.trailingStackViewButtons = @[
    self.forwardTrailingButton, self.shareButton, self.bookmarkButton,
    self.toolsMenuButton
  ];
  self.trailingStackView = [[UIStackView alloc]
      initWithArrangedSubviews:self.trailingStackViewButtons];
  self.trailingStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:self.trailingStackView];
}

// Sets the constraints up.
- (void)setUpConstraints {
  id<LayoutGuideProvider> safeArea = SafeAreaLayoutGuideForView(self);

  // Leading StackView constraints
  [NSLayoutConstraint activateConstraints:@[
    [self.leadingStackView.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor],
    [self.leadingStackView.bottomAnchor
        constraintEqualToAnchor:safeArea.bottomAnchor],
    [self.leadingStackView.topAnchor
        constraintEqualToAnchor:self.topSafeAnchor],
  ]];

  // LocationBar constraints.
  [NSLayoutConstraint activateConstraints:@[
    [self.locationBarContainer.leadingAnchor
        constraintEqualToAnchor:self.leadingStackView.trailingAnchor],
    [self.locationBarContainer.trailingAnchor
        constraintEqualToAnchor:self.trailingStackView.leadingAnchor],
    [self.locationBarContainer.bottomAnchor
        constraintEqualToAnchor:safeArea.bottomAnchor],
    [self.locationBarContainer.topAnchor
        constraintEqualToAnchor:self.topSafeAnchor],
    [self.locationBarContainer.heightAnchor
        constraintEqualToConstant:kToolbarHeight],
  ]];

  // Trailing StackView constraints.
  [NSLayoutConstraint activateConstraints:@[
    [self.trailingStackView.trailingAnchor
        constraintEqualToAnchor:safeArea.trailingAnchor],
    [self.trailingStackView.bottomAnchor
        constraintEqualToAnchor:safeArea.bottomAnchor],
    [self.trailingStackView.topAnchor
        constraintEqualToAnchor:self.topSafeAnchor],
  ]];

  // locationBarView constraints, if present.
  if (self.locationBarView) {
    AddSameConstraints(self.locationBarContainer, self.locationBarView);
  }
}

#pragma mark - Property accessors

- (void)setLocationBarView:(UIView*)locationBarView {
  if (_locationBarView == locationBarView) {
    return;
  }
  [_locationBarView removeFromSuperview];

  _locationBarView = locationBarView;
  locationBarView.translatesAutoresizingMaskIntoConstraints = NO;
  [locationBarView setContentHuggingPriority:UILayoutPriorityDefaultLow
                                     forAxis:UILayoutConstraintAxisHorizontal];

  if (!self.locationBarContainer || !locationBarView)
    return;

  [self.locationBarContainer addSubview:locationBarView];
  AddSameConstraints(self.locationBarContainer, locationBarView);
}

- (NSArray<ToolbarButton*>*)allButtons {
  if (!_allButtons) {
    _allButtons = [self.leadingStackViewButtons
        arrayByAddingObjectsFromArray:self.trailingStackViewButtons];
  }
  return _allButtons;
}

@end
