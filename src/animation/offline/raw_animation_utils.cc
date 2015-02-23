//============================================================================//
//                                                                            //
// ozz-animation, 3d skeletal animation libraries and tools.                  //
// https://code.google.com/p/ozz-animation/                                   //
//                                                                            //
//----------------------------------------------------------------------------//
//                                                                            //
// Copyright (c) 2012-2015 Guillaume Blanc                                    //
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

#include "ozz/animation/offline/raw_animation_utils.h"

#include <algorithm>

namespace ozz {
namespace animation {
namespace offline {

namespace {

// Translation interpolation method.
// This must be the same Lerp as the one used by the sampling job.
math::Float3 LerpTranslation(const math::Float3& _a,
                             const math::Float3& _b,
                             float _alpha) {
  return math::Lerp(_a, _b, _alpha);
}

// Rotation interpolation method.
// This must be the same Lerp as the one used by the sampling job.
math::Quaternion LerpRotation(const math::Quaternion& _a,
                              const math::Quaternion& _b,
                              float _alpha) {
  // Finds the shortest path. This is done by the AnimationBuilder for runtime
  // animations.
  const float dot = _a.x * _b.x + _a.y * _b.y + _a.z * _b.z + _a.w * _b.w;
  return math::NLerp(_a, dot < 0.f ? -_b : _b, _alpha); // _b an -_b are the
                                                        // same rotation.
}

// Scale interpolation method.
// This must be the same Lerp as the one used by the sampling job.
math::Float3 LerpScale(const math::Float3& _a,
                       const math::Float3& _b,
                       float _alpha) {
  return math::Lerp(_a, _b, _alpha);
}

template<typename _Key>
bool Less(const _Key& _left,const _Key& _right) {
  return _left.time < _right.time;
}

template<typename _Track, typename _Lerp>
typename _Track::value_type::Value SampleComponent(const _Track& _track,
                                                   const _Lerp& _lerp,
                                                   float _time) {
  if (_track.size() == 0) {
    // Return identity if there's no key for this track.
    return _Track::value_type::identity();
  } else if (_time <= _track.front().time) {
    return _track.front().value;
  } else if (_time >= _track.back().time) {
    return _track.back().value;
  } else {
    assert(_track.size() >= 2);
    // Needs to interpolate, so first find the keys.
    const typename _Track::value_type cmp = {
      _time, _Track::value_type::identity()};
    typename _Track::const_pointer it = std::lower_bound(
      array_begin(_track), array_end(_track), cmp,
      Less<typename _Track::value_type>);
    assert(it > array_begin(_track) && it < array_end(_track));

    // Then interpolate.
    const typename _Track::const_reference right = it[0];
    const typename _Track::const_reference left = it[-1];
    const float alpha = (_time - left.time) / (right.time - left.time);
    return _lerp(left.value, right.value, alpha);
  }
}
}  // namespace

bool SampleTrack(const RawAnimation& animation,
                 int _track,
                 float _time,
                 math::Transform* _transform) {

  // Early out if animation isn't valid.
  if (!animation.Validate()) {
    return false;
  }

  // Invalid parameter.
  if (_track < 0 || _track > animation.num_tracks()) {
    return false;
  }

  // Samples each track's component.
  const animation::offline::RawAnimation::JointTrack& track = animation.tracks[_track];
  _transform->translation = SampleComponent(track.translations, LerpTranslation, _time);
  _transform->rotation = SampleComponent(track.rotations, LerpRotation, _time);
  _transform->scale = SampleComponent(track.scales, LerpScale, _time);

  return true;
}

bool Sample(const RawAnimation& animation,
            float _time,
            Range<math::Transform> _transforms) {

  // Early out if animation isn't valid.
  if (!animation.Validate()) {
    return false;
  }

  // Invalid parameter.
  if (_transforms.Count() < animation.tracks.size()) {
    return false;
  }

  for (size_t i = 0; i < animation.tracks.size(); ++i) {
    // Samples each track's component.
    const animation::offline::RawAnimation::JointTrack& track = animation.tracks[i];
    math::Transform& transform = _transforms[i];
    transform.translation = SampleComponent(track.translations, LerpTranslation, _time);
    transform.rotation = SampleComponent(track.rotations, LerpRotation, _time);
    transform.scale = SampleComponent(track.scales, LerpScale, _time);
  }
  return true;
}
}  // offline
}  // animation
}  // ozz
