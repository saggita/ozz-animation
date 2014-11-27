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

#ifndef OZZ_OZZ_ANIMATION_OFFLINE_ANIMATION_DELTA_BUILDER_H_
#define OZZ_OZZ_ANIMATION_OFFLINE_ANIMATION_DELTA_BUILDER_H_

namespace ozz {
namespace animation {
namespace offline {

// Forward declare offline animation type.
struct RawAnimation;

// Defines the class responsible of building a delta animation from an offline
// raw animation. This is used to create animations compatible with additive
// blending.
class AnimationDeltaBuilder {
 public:
  // Initializes the builder.
  AnimationDeltaBuilder();

  // Builds delta animation from _input..
  // Returns true on success and fills _output_animation with the delta
  // version of _input animation.
  // *_output must be a valid RawAnimation instance.
  // Returns false on failure and resets _output to an empty animation.
  // See RawAnimation::Validate() for more details about failure reasons.
  bool operator()(const RawAnimation& _input, RawAnimation* _output) const;
};
}  // offline
}  // animation
}  // ozz
#endif  // OZZ_OZZ_ANIMATION_OFFLINE_ANIMATION_DELTA_BUILDER_H_
