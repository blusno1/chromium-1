// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_VIEW_CONTROLLER_PROTECTED_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_VIEW_CONTROLLER_PROTECTED_H_

@protocol BookmarkHomePrimaryView;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

@class ActionSheetCoordinator;
@class BookmarkCollectionView;
@class BookmarkContextBar;
@class BookmarkEditingBar;
@class BookmarkEditViewController;
@class BookmarkFolderEditorViewController;
@class BookmarkFolderViewController;
@class BookmarkHomeWaitingView;
@class BookmarkMenuItem;
@class BookmarkMenuView;
@class BookmarkNavigationBar;
@class BookmarkPanelView;
@class BookmarkPromoController;
@class BookmarkTableView;
@class MDCAppBar;

typedef NS_ENUM(NSInteger, BookmarksContextBarState) {
  BookmarksContextBarNone,            // No state.
  BookmarksContextBarDefault,         // No selection is possible in this state.
  BookmarksContextBarBeginSelection,  // This is the clean start state,
                                      // selection is possible, but nothing is
                                      // selected yet.
  BookmarksContextBarSingleURLSelection,       // Single URL selected state.
  BookmarksContextBarMultipleURLSelection,     // Multiple URLs selected state.
  BookmarksContextBarSingleFolderSelection,    // Single folder selected.
  BookmarksContextBarMultipleFolderSelection,  // Multiple folders selected.
  BookmarksContextBarMixedSelection,  // Multiple URL / Folders selected.
};

// BookmarkHomeViewController class extension for protected read/write
// properties and methods for subclasses.
@interface BookmarkHomeViewController ()

// The bookmark model used.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarks;

// The user's browser state model used.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

// The main view showing all the bookmarks.
// TODO(crbug.com/753599): Remove this property when clean up old bookmarks.
@property(nonatomic, strong) BookmarkCollectionView* folderView;

// The main view showing all the bookmarks.
@property(nonatomic, strong) BookmarkTableView* bookmarksTableView;

// The view controller used to pick a folder in which to move the selected
// bookmarks.
@property(nonatomic, strong) BookmarkFolderViewController* folderSelector;

// Object to load URLs.
@property(nonatomic, weak) id<UrlLoader> loader;

// The menu with all the folders.
// TODO(crbug.com/753599): Remove this property when clean up old bookmarks.
@property(nonatomic, strong) BookmarkMenuView* menuView;

// The navigation bar sits on top of the main content.
// TODO(crbug.com/753599): Remove this property when clean up old bookmarks.
@property(nonatomic, strong) BookmarkNavigationBar* navigationBar;

// The app bar for the bookmarks.
@property(nonatomic, strong) MDCAppBar* appBar;

// The context bar at the bottom of the bookmarks.
@property(nonatomic, strong) BookmarkContextBar* contextBar;

// At any point in time, there is exactly one collection view whose view is part
// of the view hierarchy. This property determines what data is visible in the
// collection view.
// TODO(crbug.com/753599): Remove this property when clean up old bookmarks.
@property(nonatomic, strong) BookmarkMenuItem* primaryMenuItem;

// This view holds a content view, and a menu view.
// TODO(crbug.com/753599): Remove this property when clean up old bookmarks.
@property(nonatomic, strong) BookmarkPanelView* panelView;

// Either the menu or the primaryView can scrollToTop.
// TODO(crbug.com/753599): Remove this property when clean up old bookmarks.
@property(nonatomic, assign) BOOL scrollToTop;

// This view is created and used if the model is not fully loaded yet by the
// time this controller starts.
@property(nonatomic, strong) BookmarkHomeWaitingView* waitForModelView;

// The view controller used to view and edit a single bookmark.
@property(nonatomic, strong) BookmarkEditViewController* editViewController;

// Whether the view controller is in editing mode.
@property(nonatomic, assign) BOOL editing;

// The set of selected index paths for edition.
@property(nonatomic, strong) NSMutableArray* editIndexPaths;

// The layout code in this class relies on the assumption that the editingBar
// has the same frame as the navigationBar.
// TODO(crbug.com/753599): Remove this property when clean up old bookmarks.
@property(nonatomic, strong) BookmarkEditingBar* editingBar;

// The view controller to present when editing the current folder.
@property(nonatomic, strong) BookmarkFolderEditorViewController* folderEditor;

// The controller managing the display of the promo cell and the promo view
// controller.
@property(nonatomic, strong) BookmarkPromoController* bookmarkPromoController;

// Whether the panel view can be brought into view and hidden by swipe gesture.
// TODO(crbug.com/753599): Remove this property when clean up old bookmarks.
@property(nonatomic, assign) BOOL sideSwipingPossible;

// The action sheet coordinator used when trying to edit a single bookmark.
// TODO(crbug.com/753599): Remove this property when clean up old bookmarks.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

// The current state of the context bar UI.
@property(nonatomic, assign) BookmarksContextBarState contextBarState;

// When the view is first shown on the screen, this property represents the
// cached value of the y of the content offset of the table view. This
// property is set to nil after it is used.
@property(nonatomic, strong) NSNumber* cachedContentPosition;

// This method should be called at most once in the life-cycle of the class.
// It should be called at the soonest possible time after the view has been
// loaded, and the bookmark model is loaded.
- (void)loadBookmarkViews;

// Returns the width of the menu.
// TODO(crbug.com/753599): Remove this property when clean up old bookmarks.
- (CGFloat)menuWidth;

// This method is called if the view needs to be loaded and the model is not
// ready yet.
- (void)loadWaitingView;

// Updates the property 'primaryMenuItem'.
// Updates the UI to reflect the new state of 'primaryMenuItem'.
// TODO(crbug.com/753599): Remove this property when clean up old bookmarks.
- (void)updatePrimaryMenuItem:(BookmarkMenuItem*)menuItem
                     animated:(BOOL)animated;

// Caches the position in the collection view.
// TODO(crbug.com/753599): Remove this method when clean up old bookmarks.
- (void)cachePosition;

// Whether the back button on the navigation bar should be shown.
// TODO(crbug.com/753599): Remove this method when clean up old bookmarks.
- (BOOL)shouldShowBackButtonOnNavigationBar;

#pragma mark - Subclass overrides

// Creates and returns actionSheetCoordinator. MUST
// be overridden by subclass.
// TODO(crbug.com/753599): Remove this method when clean up old bookmarks.
- (ActionSheetCoordinator*)createActionSheetCoordinatorOnView:(UIView*)view;

// Shows the editing bar, this method MUST be overridden by subclass to
// tailor the behaviour according to device.
// TODO(crbug.com/753599): Remove this method when clean up old bookmarks.
- (void)showEditingBarAnimated:(BOOL)animated;

// Hides the editing bar, this method MUST be overridden by subclass to
// tailor the behaviour according to device.
// TODO(crbug.com/753599): Remove this method when clean up old bookmarks.
- (void)hideEditingBarAnimated:(BOOL)animated;

// Returns the frame for editingBar, MUST be overridden by subclass.
// TODO(crbug.com/753599): Remove this method when clean up old bookmarks.
- (CGRect)editingBarFrame;

#pragma mark - Navigation bar.

// Callback for when navigation bar is cancelled.
- (void)navigationBarCancel:(id)sender;

// Updates the UI of the navigation bar with the primaryMenuItem.
// This method should be called anytime:
//  (1)The primary view changes.
//  (2)The primary view has type folder, and the relevant folder has changed.
//  (3)The interface orientation changes.
//  (4)viewWillAppear, as the interface orientation may have changed.
// TODO(crbug.com/753599): Remove this method when clean up old bookmarks.
- (void)updateNavigationBarAnimated:(BOOL)animated
                        orientation:(UIInterfaceOrientation)orientation;

#pragma mark - Edit

// This method updates the property, and resets the edit nodes.
// TODO(crbug.com/753599): Remove this method when clean up old bookmarks.
- (void)setEditing:(BOOL)editing animated:(BOOL)animated;

// Instaneously updates the shadow of the edit bar.
// This method should be called anytime:
//  (1)|editing| property changes.
//  (2)The primary view changes.
//  (3)The primary view's collection view is scrolled.
// (2) is not necessary right now, as it is only possible to switch primary
// views when |editing| is NO. When |editing| is NO, the shadow is never shown.
// TODO(crbug.com/753599): Remove this method when clean up old bookmarks.
- (void)updateEditBarShadow;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_VIEW_CONTROLLER_PROTECTED_H_
