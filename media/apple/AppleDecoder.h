/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __AppleDecoder_h__
#define __AppleDecoder_h__

#include "MediaDecoder.h"

namespace mozilla {

class AppleDecoder : public MediaDecoder
{
public:
  explicit AppleDecoder(MediaDecoderOwner* aOwner);

  virtual MediaDecoder* Clone(MediaDecoderOwner* aOwner) override;
  virtual MediaDecoderStateMachine* CreateStateMachine() override;

};

} // namespace mozilla

#endif // __AppleDecoder_h__
