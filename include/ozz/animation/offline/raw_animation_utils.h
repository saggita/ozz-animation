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

#ifndef OZZ_OZZ_ANIMATION_OFFLINE_RAW_ANIMATION_UTILS_H_
#define OZZ_OZZ_ANIMATION_OFFLINE_RAW_ANIMATION_UTILS_H_

#include "ozz/animation/offline/raw_animation.h"

#include "ozz/base/maths/transform.h"

namespace ozz {
namespace animation {
namespace offline {

// Translation interpolation method.
math::Float3 LerpTranslation(const math::Float3& _a,
                             const math::Float3& _b,
                             float _alpha);

// Rotation interpolation method.
math::Quaternion LerpRotation(const math::Quaternion& _a,
                              const math::Quaternion& _b,
                              float _alpha);

// Scale interpolation method.
math::Float3 LerpScale(const math::Float3& _a,
                       const math::Float3& _b,
                       float _alpha);

// Utility function that samples one animation track at t = _time.
// This function is not intended to be used at runtime, but for rather as a
// helper for offline tools. Use ozz::animation::SamplingJob to sample
// runtime animations instead.
// Return false if animation is invalid, or if track is out of range.
bool SampleTrack(const RawAnimation& animation,
                 int _track,
                 float _time,
                 math::Transform* _transform);

// Utility function that samples all animation tracks at t = _time.
// This function is not intended to be used at runtime, but for rather as a
// helper for offline tools. Use ozz::animation::SamplingJob to sample
// runtime animations instead.
// Return false if animation is invalid, or if _transforms size is smaller that
// the number of tracks.
bool Sample(const RawAnimation& animation,
            float _time,
            Range<math::Transform> _transforms);
}  // offline
}  // animation
}  // ozz
#endif  // OZZ_OZZ_ANIMATION_OFFLINE_RAW_ANIMATION_UTILS_H_
