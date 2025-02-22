// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MESSAGE_CENTER_MESSAGE_CENTER_UI_DELEGATE_H_
#define ASH_MESSAGE_CENTER_MESSAGE_CENTER_UI_DELEGATE_H_

namespace ash {

// A UiDelegate class is responsible for managing the various UI surfaces
// (MessageCenter and popups) as notifications are added and updated.
// Implementations are platform specific.
class MessageCenterUiDelegate {
 public:
  virtual ~MessageCenterUiDelegate(){};

  // Called whenever a change to the visible UI has occurred.
  virtual void OnMessageCenterContentsChanged() = 0;

  // Display the popup bubbles for new notifications to the user.  Returns true
  // if popups were actually displayed to the user.
  virtual bool ShowPopups() = 0;

  // Remove the popup bubbles from the UI.
  virtual void HidePopups() = 0;

  // Display the message center containing all undismissed notifications to the
  // user. Set |show_by_click| to true if message center is shown by mouse or
  // gesture click. Returns true if the center was actually displayed to the
  // user.
  virtual bool ShowMessageCenter(bool show_by_click) = 0;

  // Remove the message center from the UI.
  virtual void HideMessageCenter() = 0;
};

}  // namespace ash

#endif  // ASH_MESSAGE_CENTER_MESSAGE_CENTER_UI_DELEGATE_H_
