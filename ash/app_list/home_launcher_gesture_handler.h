// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_HOME_LAUNCHER_GESTURE_HANDLER_H_
#define ASH_APP_LIST_HOME_LAUNCHER_GESTURE_HANDLER_H_

#include <map>
#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/transform.h"

namespace ash {

// HomeLauncherGestureHandler makes modifications to a window's transform and
// opacity when gesture drag events are received and forwarded to it.
// Additionally hides windows which may block the home launcher. All
// modifications are either transitioned to their final state, or back to their
// initial state on release event.
class ASH_EXPORT HomeLauncherGestureHandler : aura::WindowObserver {
 public:
  HomeLauncherGestureHandler();
  ~HomeLauncherGestureHandler() override;

  // Called by owner of this object when a gesture event is received. |location|
  // should be in screen coordinates.
  void OnPressEvent();
  void OnScrollEvent(const gfx::Point& location);
  void OnReleaseEvent(const gfx::Point& location);

  // TODO(sammiequon): Investigate if it is needed to observe potential window
  // visibility changes, if they can happen.
  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  aura::Window* window() { return window_; }

 private:
  // Stores the initial and target opacities and transforms of a window.
  struct WindowValues {
    float initial_opacity;
    float target_opacity;
    gfx::Transform initial_transform;
    gfx::Transform target_transform;
  };

  // Checks if |window| can be hidden or shown with a gesture.
  bool CanHideWindow(aura::Window* window);

  // Updates the opacity and transform |window_| and its transient children base
  // on the values in |window_values_| and |transient_descendants_values_|.
  // |progress| is between 0.0 and 1.0, where 0.0 means the window will have its
  // original opacity and transform, and 1.0 means the window will be faded out
  // and transformed offscreen.
  void UpdateWindows(double progress);

  // Stop observing all windows and remove their local pointers.
  void RemoveObserversAndStopTracking();

  aura::Window* window_ = nullptr;

  // Original and target transform and opacity of |window_|.
  WindowValues window_values_;

  // Tracks the transient descendants of |window_| and their initial and target
  // opacities and transforms.
  std::map<aura::Window*, WindowValues> transient_descendants_values_;

  // Stores windows which were shown behind the mru window. They need to be
  // hidden so the home launcher is visible when swiping up.
  std::vector<aura::Window*> hidden_windows_;

  DISALLOW_COPY_AND_ASSIGN(HomeLauncherGestureHandler);
};

}  // namespace ash

#endif  // ASH_APP_LIST_HOME_LAUNCHER_GESTURE_HANDLER_H_
