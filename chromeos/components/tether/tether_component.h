// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_TETHER_COMPONENT_H_
#define CHROMEOS_COMPONENTS_TETHER_TETHER_COMPONENT_H_

#include "base/macros.h"
#include "base/observer_list.h"

namespace chromeos {

namespace tether {

// Initializes the Tether component.
class TetherComponent {
 public:
  class Observer {
   public:
    Observer() {}
    virtual ~Observer() {}

    virtual void OnShutdownComplete() = 0;
  };

  enum class Status { ACTIVE, SHUTTING_DOWN, SHUT_DOWN };

  enum class ShutdownReason {
    OTHER = 0,
    USER_LOGGED_OUT = 3,
    USER_CLOSED_LID = 4,
    PREF_DISABLED = 5,
    BLUETOOTH_DISABLED = 6,
    CELLULAR_DISABLED = 7,
    BLUETOOTH_CONTROLLER_DISAPPEARED = 8,
    MULTIDEVICE_HOST_UNVERIFIED = 9
  };

  TetherComponent();
  virtual ~TetherComponent();

  // Requests that the Tether component shuts down. If the component can be shut
  // down synchronously, this causes TetherComponent to transition to the
  // SHUT_DOWN status immediately. However, if the component requires an
  // asynchronous shutdown, the class transitions to the SHUTTING_DOWN status;
  // once an asynchronous shutdown completes, TetherComponent transitions to the
  // SHUT_DOWN status and notifies observers.
  virtual void RequestShutdown(const ShutdownReason& shutdown_reason) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  Status status() { return status_; }
  void TransitionToStatus(Status new_status);

 private:
  Status status_ = Status::ACTIVE;
  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(TetherComponent);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_TETHER_COMPONENT_H_
