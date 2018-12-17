/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cmath>
#include <limits>
#include "BatteryManager.h"
#include "Constants.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Hal.h"
#include "mozilla/dom/BatteryManagerBinding.h"
#include "mozilla/Preferences.h"
#include "nsIDOMClassInfo.h"
#include "nsIDocument.h"

/**
 * We have to use macros here because our leak analysis tool things we are
 * leaking strings when we have |static const nsString|. Sad :(
 */
#define LEVELCHANGE_EVENT_NAME           NS_LITERAL_STRING("levelchange")
#define CHARGINGCHANGE_EVENT_NAME        NS_LITERAL_STRING("chargingchange")
#define DISCHARGINGTIMECHANGE_EVENT_NAME NS_LITERAL_STRING("dischargingtimechange")
#define CHARGINGTIMECHANGE_EVENT_NAME    NS_LITERAL_STRING("chargingtimechange")

namespace mozilla {
namespace dom {
namespace battery {

BatteryManager::BatteryManager(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mLevel(kDefaultLevel)
  , mCharging(kDefaultCharging)
  , mRemainingTime(kDefaultRemainingTime)
{
}

void
BatteryManager::Init()
{
  hal::RegisterBatteryObserver(this);

  hal::BatteryInformation batteryInfo;
  hal::GetCurrentBatteryInformation(&batteryInfo);

  UpdateFromBatteryInfo(batteryInfo);
}

void
BatteryManager::Shutdown()
{
  hal::UnregisterBatteryObserver(this);
}

JSObject*
BatteryManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return BatteryManagerBinding::Wrap(aCx, this, aGivenProto);
}

bool
BatteryManager::Charging() const
{
  MOZ_ASSERT(NS_IsMainThread());
  // For testing, unable to report the battery status information
  if (Preferences::GetBool("dom.battery.test.default", false)) {
    return true;
  }
  if (Preferences::GetBool("dom.battery.test.charging", false)) {
    return true;
  }
  if (Preferences::GetBool("dom.battery.test.discharging", false)) {
    return false;
  }

  return mCharging;
}

double
BatteryManager::DischargingTime() const
{
  MOZ_ASSERT(NS_IsMainThread());
  // For testing, unable to report the battery status information
  if (Preferences::GetBool("dom.battery.test.default", false)) {
    return std::numeric_limits<double>::infinity();
  }
  if (Preferences::GetBool("dom.battery.test.discharging", false)) {
    return 42.0;
  }

  if (Charging() || mRemainingTime == kUnknownRemainingTime) {
    return std::numeric_limits<double>::infinity();
  }

  return mRemainingTime;
}

double
BatteryManager::ChargingTime() const
{
  MOZ_ASSERT(NS_IsMainThread());
  // For testing, unable to report the battery status information
  if (Preferences::GetBool("dom.battery.test.default", false)) {
    return 0.0;
  }
  if (Preferences::GetBool("dom.battery.test.charging", false)) {
    return 42.0;
  }

  if (!Charging() || mRemainingTime == kUnknownRemainingTime) {
    return std::numeric_limits<double>::infinity();
  }

  return mRemainingTime;
}

double
BatteryManager::Level() const
{
  MOZ_ASSERT(NS_IsMainThread());
  // For testing, unable to report the battery status information
  if (Preferences::GetBool("dom.battery.test.default")) {
    return 1.0;
  }

  return mLevel;
}

void
BatteryManager::UpdateFromBatteryInfo(const hal::BatteryInformation& aBatteryInfo)
{
  mLevel = aBatteryInfo.level();

  // Round to the nearest ten percent for non-chrome and non-certified apps
  nsIDocument* doc = GetOwner()->GetDoc();
  uint16_t status = nsIPrincipal::APP_STATUS_NOT_INSTALLED;
  if (doc) {
    doc->NodePrincipal()->GetAppStatus(&status);
  }

  if (!nsContentUtils::IsChromeDoc(doc) &&
      status != nsIPrincipal::APP_STATUS_CERTIFIED)
  {
    mLevel = lround(mLevel * 10.0) / 10.0;
  }

  mCharging = aBatteryInfo.charging();
  mRemainingTime = aBatteryInfo.remainingTime();

  // Add some guards to make sure the values are coherent.
  if (mLevel == 1.0 && mCharging == true &&
      mRemainingTime != kDefaultRemainingTime) {
    mRemainingTime = kDefaultRemainingTime;
    NS_ERROR("Battery API: When charging and level at 1.0, remaining time "
             "should be 0. Please fix your backend!");
  }
}

void
BatteryManager::Notify(const hal::BatteryInformation& aBatteryInfo)
{
  double previousLevel = mLevel;
  bool previousCharging = mCharging;
  double previousRemainingTime = mRemainingTime;

  UpdateFromBatteryInfo(aBatteryInfo);

  if (previousCharging != mCharging) {
    DispatchTrustedEvent(CHARGINGCHANGE_EVENT_NAME);
  }

  if (previousLevel != mLevel) {
    DispatchTrustedEvent(LEVELCHANGE_EVENT_NAME);
  }

  /*
   * There are a few situations that could happen here:
   * 1. Charging state changed:
   *   a. Previous remaining time wasn't unkwonw, we have to fire an event for
   *      the change.
   *   b. New remaining time isn't unkwonw, we have to fire an event for it.
   * 2. Charging state didn't change but remainingTime did, we have to fire
   *    the event that correspond to the current charging state.
   */
  if (mCharging != previousCharging) {
    if (previousRemainingTime != kUnknownRemainingTime) {
      DispatchTrustedEvent(previousCharging ? CHARGINGTIMECHANGE_EVENT_NAME
                                            : DISCHARGINGTIMECHANGE_EVENT_NAME);
    }
    if (mRemainingTime != kUnknownRemainingTime) {
      DispatchTrustedEvent(mCharging ? CHARGINGTIMECHANGE_EVENT_NAME
                                     : DISCHARGINGTIMECHANGE_EVENT_NAME);
    }
  } else if (previousRemainingTime != mRemainingTime) {
    DispatchTrustedEvent(mCharging ? CHARGINGTIMECHANGE_EVENT_NAME
                                   : DISCHARGINGTIMECHANGE_EVENT_NAME);
  }
}

} // namespace battery
} // namespace dom
} // namespace mozilla
