//============================================================================//
//                                                                            //
// ozz-animation, 3d skeletal animation libraries and tools.                  //
// https://code.google.com/p/ozz-animation/                                   //
//                                                                            //
//----------------------------------------------------------------------------//
//                                                                            //
// Copyright (c) 2012-2014 Guillaume Blanc                                    //
//                                                                            //
// This software is provided 'as-is', without any express or implied          //
// warranty. In no event will the authors be held liable for any damages      //
// arising from the use of this software.                                     //
//                                                                            //
// Permission is granted to anyone to use this software for any purpose,      //
// including commercial applications, and to alter it and redistribute it     //
// freely, subject to the following restrictions:                             //
//                                                                            //
// 1. The origin of this software must not be misrepresented; you must not    //
// claim that you wrote the original software. If you use this software       //
// in a product, an acknowledgment in the product documentation would be      //
// appreciated but is not required.                                           //
//                                                                            //
// 2. Altered source versions must be plainly marked as such, and must not be //
// misrepresented as being the original software.                             //
//                                                                            //
// 3. This notice may not be removed or altered from any source               //
// distribution.                                                              //
//                                                                            //
//============================================================================//

#include "ozz/animation/offline/additive_animation_builder.h"

#include <cstddef>
#include <cassert>

#include "ozz/animation/offline/raw_animation.h"

namespace ozz {
namespace animation {
namespace offline {

namespace {
template<typename _RawTrack, typename _MakeDelta>
void MakeDelta(const _RawTrack& _src,
               const _MakeDelta& _make_delta,
               _RawTrack* _dest) {
  _dest->reserve(_src.size());

  // Early out if no key.
  if (_src.empty()) {
    return;
  }

  // Stores reference value.
  typename _RawTrack::const_reference reference = _src[0];

  // Copy animation keys.
  for (size_t i = 0; i < _src.size(); ++i) {
    const typename _RawTrack::value_type delta = {
      _src[i].time,
      _make_delta(reference.value, _src[i].value)
    };
    _dest->push_back(delta);
  }
}

math::Float3 MakeDeltaTranslation(const math::Float3& _reference,
                                  const math::Float3& _value) {
  return _value - _reference;
}

math::Quaternion MakeDeltaRotation(const math::Quaternion& _reference,
                                   const math::Quaternion& _value) {
  return Conjugate(_reference) * _value;
}

math::Float3 MakeDeltaScale(const math::Float3& _reference,
                            const math::Float3& _value) {
  return _value / _reference;
}
}  // namespace

// Setup default values (favoring quality).
AdditiveAnimationBuilder::AdditiveAnimationBuilder() {
}

bool AdditiveAnimationBuilder::operator()(const RawAnimation& _input,
                                          RawAnimation* _output) const {
  if (!_output) {
    return false;
  }
  // Reset output animation to default.
  *_output = RawAnimation();

  // Validate animation.
  if (!_input.Validate()) {
    return false;
  }

  // Rebuilds output animation.
  _output->duration = _input.duration;
  _output->tracks.resize(_input.tracks.size());
  
  for (size_t i = 0; i < _input.tracks.size(); ++i) {
    MakeDelta(_input.tracks[i].translations,
              MakeDeltaTranslation,
              &_output->tracks[i].translations);
    MakeDelta(_input.tracks[i].rotations,
              MakeDeltaRotation,
              &_output->tracks[i].rotations);
    MakeDelta(_input.tracks[i].scales,
              MakeDeltaScale,
              &_output->tracks[i].scales);
  }

  // Output animation is always valid though.
  return _output->Validate();
}
}  // offline
}  // animation
}  // ozz
