// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LAYOUT_CONSTANTS_H_
#define CHROME_BROWSER_UI_LAYOUT_CONSTANTS_H_

#include "build/build_config.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

enum LayoutConstant {
  // The minimum height of Bookmarks Bar, when attached to the toolbar. The
  // height of the toolbar may grow to more than this value if the embedded
  // views need more space, for example, when the font is larger than normal.
  BOOKMARK_BAR_HEIGHT,

#if defined(OS_MACOSX)
  // This is a little smaller than the bookmarkbar height because of the visual
  // overlap with the main toolbar. This height should not be used when
  // computing the height of the toolbar.
  BOOKMARK_BAR_HEIGHT_NO_OVERLAP,
#endif

  // The height of Bookmarks Bar, when visible in "New Tab Page" mode.
  BOOKMARK_BAR_NTP_HEIGHT,

#if defined(OS_MACOSX)
  // The amount of space between the inner bookmark bar and the outer toolbar on
  // new tab pages.
  BOOKMARK_BAR_NTP_PADDING,
#endif

  // The size of the app menu button in a hosted app browser window.
  HOSTED_APP_MENU_BUTTON_SIZE,

  // The size of page action icons in a hosted app title bar.
  HOSTED_APP_PAGE_ACTION_ICON_SIZE,

  // The vertical padding (additional to TOOLBAR_ELEMENT_PADDING) above and
  // below location bar bubbles.
  LOCATION_BAR_BUBBLE_VERTICAL_PADDING,

  // The vertical padding between the edge of a location bar bubble and its
  // contained text.
  LOCATION_BAR_BUBBLE_FONT_VERTICAL_PADDING,

  // The corner radius used for the location bar bubble.
  LOCATION_BAR_BUBBLE_CORNER_RADIUS,

  // The vertical inset to apply to the bounds of a location bar bubble's anchor
  // view, to bring the bubble closer to the anchor.  This compensates for the
  // space between the bottoms of most such views and the visible bottoms of the
  // images inside.
  LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET,

  // The vertical and horizontal padding inside the border.
  LOCATION_BAR_ELEMENT_PADDING,

  // The horizontal padding between location bar decorations.
  LOCATION_BAR_BETWEEN_ELEMENTS_PADDING,

  // The height to be occupied by the LocationBar.
  LOCATION_BAR_HEIGHT,

  // The size of the icons used inside the LocationBar.
  LOCATION_BAR_ICON_SIZE,

  // Padding after the tab title.
  TAB_AFTER_TITLE_PADDING,

  // Width of the alert indicator shown for a tab using media capture.
  TAB_ALERT_INDICATOR_CAPTURE_ICON_WIDTH,

  // Width of the alert indicator icon displayed in the tab. The same width is
  // used for all 3 states of normal, hovered and pressed.
  TAB_ALERT_INDICATOR_ICON_WIDTH,

  // The height of a tab, including outer strokes.  In non-100% scales this is
  // slightly larger than the apparent height of the tab, as the top stroke is
  // drawn as a 1-px line flush with the bottom of the tab's topmost DIP.
  TAB_HEIGHT,

  // Padding before the tab title.
  TAB_PRE_TITLE_PADDING,

  // The distance between the edge of one tab to the corresponding edge or the
  // subsequent tab when tabs are stacked.
  TAB_STACK_DISTANCE,

  // In refresh, tabs are drawn with an extension into the toolbar's
  // space to prevent a gap from appearing between the toolbar and the
  // bottom of tabs on some non-integral scales.
  // TODO(tbergquist): Remove this after pixel canvas or any deeper fix to
  // non-pixel-aligned drawing goes in.  See https://crbug.com/765723.
  TABSTRIP_TOOLBAR_OVERLAP,

  // Additional horizontal padding between the elements in the toolbar.
  TOOLBAR_ELEMENT_PADDING,

  // The horizontal space between most items in the toolbar.
  TOOLBAR_STANDARD_SPACING,
};

enum LayoutInset {
  // The padding used around the icon inside the LocationBar. The full width of
  // the icon would be LOCATION_BAR_ICON_SIZE + 2 * inset.width(). The full
  // height of the icon would be LOCATION_BAR_ICON_SIZE + 2 * inset.height().
  // Icons may additionally be spaced horizontally by
  // LOCATION_BAR_ELEMENT_PADDING, but this region is not part of the icon view
  // (e.g. does not highlight on hover).
  LOCATION_BAR_ICON_INTERIOR_PADDING,

  // The padding inside the border of a toolbar button (around the image).
  TOOLBAR_BUTTON,

  // The padding inside the border of a toolbar action view button.
  TOOLBAR_ACTION_VIEW,
};

int GetLayoutConstant(LayoutConstant constant);
#if defined(OS_MACOSX)
// Use this function instead of GetLayoutConstant() for Cocoa browser.
// This will handle Cocoa specific layout constants. For non Cocoa specific
// constants, it will call GetLayoutConstant() anyway.
int GetCocoaLayoutConstant(LayoutConstant constant);
#endif

gfx::Insets GetLayoutInsets(LayoutInset inset);

#endif  // CHROME_BROWSER_UI_LAYOUT_CONSTANTS_H_
