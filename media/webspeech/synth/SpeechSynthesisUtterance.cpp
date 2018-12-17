/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGkAtoms.h"

#include "mozilla/dom/SpeechSynthesisEvent.h"
#include "mozilla/dom/SpeechSynthesisUtteranceBinding.h"
#include "SpeechSynthesisUtterance.h"
#include "SpeechSynthesisVoice.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(SpeechSynthesisUtterance,
                                   DOMEventTargetHelper, mVoice);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(SpeechSynthesisUtterance)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(SpeechSynthesisUtterance, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SpeechSynthesisUtterance, DOMEventTargetHelper)

SpeechSynthesisUtterance::SpeechSynthesisUtterance(nsPIDOMWindow* aOwnerWindow,
                                                   const nsAString& text)
  : DOMEventTargetHelper(aOwnerWindow)
  , mText(text)
  , mVolume(1)
  , mRate(1)
  , mPitch(1)
  , mState(STATE_NONE)
  , mPaused(false)
{
}

SpeechSynthesisUtterance::~SpeechSynthesisUtterance() {}

JSObject*
SpeechSynthesisUtterance::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SpeechSynthesisUtteranceBinding::Wrap(aCx, this, aGivenProto);
}

nsISupports*
SpeechSynthesisUtterance::GetParentObject() const
{
  return GetOwner();
}

already_AddRefed<SpeechSynthesisUtterance>
SpeechSynthesisUtterance::Constructor(GlobalObject& aGlobal,
                                      ErrorResult& aRv)
{
  return Constructor(aGlobal, EmptyString(), aRv);
}

already_AddRefed<SpeechSynthesisUtterance>
SpeechSynthesisUtterance::Constructor(GlobalObject& aGlobal,
                                      const nsAString& aText,
                                      ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aGlobal.GetAsSupports());

  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
  }

  MOZ_ASSERT(win->IsInnerWindow());
  RefPtr<SpeechSynthesisUtterance> object =
    new SpeechSynthesisUtterance(win, aText);
  return object.forget();
}

void
SpeechSynthesisUtterance::GetText(nsString& aResult) const
{
  aResult = mText;
}

void
SpeechSynthesisUtterance::SetText(const nsAString& aText)
{
  mText = aText;
}

void
SpeechSynthesisUtterance::GetLang(nsString& aResult) const
{
  aResult = mLang;
}

void
SpeechSynthesisUtterance::SetLang(const nsAString& aLang)
{
  mLang = aLang;
}

SpeechSynthesisVoice*
SpeechSynthesisUtterance::GetVoice() const
{
  return mVoice;
}

void
SpeechSynthesisUtterance::SetVoice(SpeechSynthesisVoice* aVoice)
{
  mVoice = aVoice;
}

float
SpeechSynthesisUtterance::Volume() const
{
  return mVolume;
}

void
SpeechSynthesisUtterance::SetVolume(float aVolume)
{
  mVolume = aVolume;
}

float
SpeechSynthesisUtterance::Rate() const
{
  return mRate;
}

void
SpeechSynthesisUtterance::SetRate(float aRate)
{
  mRate = aRate;
}

float
SpeechSynthesisUtterance::Pitch() const
{
  return mPitch;
}

void
SpeechSynthesisUtterance::SetPitch(float aPitch)
{
  mPitch = aPitch;
}

void
SpeechSynthesisUtterance::GetChosenVoiceURI(nsString& aResult) const
{
  aResult = mChosenVoiceURI;
}

void
SpeechSynthesisUtterance::DispatchSpeechSynthesisEvent(const nsAString& aEventType,
                                                       uint32_t aCharIndex,
                                                       float aElapsedTime,
                                                       const nsAString& aName)
{
  SpeechSynthesisEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mUtterance = this;
  init.mCharIndex = aCharIndex;
  init.mElapsedTime = aElapsedTime;
  init.mName = aName;

  RefPtr<SpeechSynthesisEvent> event =
    SpeechSynthesisEvent::Constructor(this, aEventType, init);
  DispatchTrustedEvent(event);
}

} // namespace dom
} // namespace mozilla
