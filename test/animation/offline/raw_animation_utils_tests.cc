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

#include "gtest/gtest.h"

#include "ozz/base/gtest_helper.h"
#include "ozz/base/maths/gtest_math_helper.h"

using ozz::animation::offline::RawAnimation;

TEST(SampleTrackInvalid, RawAnimationUtils) {
  // Create an invalid animation.
  RawAnimation raw_animation;
  raw_animation.duration = -1.f;

  ozz::math::Transform t;
  EXPECT_FALSE(ozz::animation::offline::SampleTrack(raw_animation, 3, .5f, &t));
}

TEST(SampleTrack, RawAnimationUtils) {

  RawAnimation raw_animation;
  raw_animation.duration = 1.f;
  raw_animation.tracks.resize(4);

  // Raw animation inputs.
  //     0              1
  // --------------------
  // 0 - A     B        |
  // 1 - C  D  E        |
  // 2 - F  G     H  I  J
  // 3 -                |

  RawAnimation::TranslationKey a = {0.f, ozz::math::Float3(1.f, 0.f, 0.f)};
  raw_animation.tracks[0].translations.push_back(a);
  RawAnimation::TranslationKey b = {.4f, ozz::math::Float3(3.f, 0.f, 0.f)};
  raw_animation.tracks[0].translations.push_back(b);

  RawAnimation::RotationKey c = {0.f, ozz::math::Quaternion(1.f, 0.f, 0.f, 0.f)};
  raw_animation.tracks[1].rotations.push_back(c);
  RawAnimation::RotationKey d = {0.2f, ozz::math::Quaternion(0.f, 1.f, 0.f, 0.f)};
  raw_animation.tracks[1].rotations.push_back(d);
  RawAnimation::RotationKey e = {0.4f, ozz::math::Quaternion(0.f, 0.f, 1.f, 0.f)};
  raw_animation.tracks[1].rotations.push_back(e);

  RawAnimation::ScaleKey f = {0.f, ozz::math::Float3(12.f, 0.f, 0.f)};
  raw_animation.tracks[2].scales.push_back(f);
  RawAnimation::ScaleKey g = {.2f, ozz::math::Float3(11.f, 0.f, 0.f)};
  raw_animation.tracks[2].scales.push_back(g);
  RawAnimation::ScaleKey h = {.6f, ozz::math::Float3(9.f, 0.f, 0.f)};
  raw_animation.tracks[2].scales.push_back(h);
  RawAnimation::ScaleKey i = {.8f, ozz::math::Float3(7.f, 0.f, 0.f)};
  raw_animation.tracks[2].scales.push_back(i);
  RawAnimation::ScaleKey j = {1.f, ozz::math::Float3(5.f, 0.f, 0.f)};
  raw_animation.tracks[2].scales.push_back(j);

  ozz::math::Transform t;

  // Test invalid track values.
  EXPECT_FALSE(ozz::animation::offline::SampleTrack(raw_animation, -1, -1.f, &t));
  EXPECT_FALSE(ozz::animation::offline::SampleTrack(raw_animation, 5, -1.f, &t));

  EXPECT_TRUE(ozz::animation::offline::SampleTrack(raw_animation, 0, -1.f, &t));
  EXPECT_FLOAT3_EQ(t.translation, 1.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t.rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t.scale, 1.f, 1.f, 1.f);

  EXPECT_TRUE(ozz::animation::offline::SampleTrack(raw_animation, 0, 0.f, &t));
  EXPECT_FLOAT3_EQ(t.translation, 1.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t.rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t.scale, 1.f, 1.f, 1.f);

  EXPECT_TRUE(ozz::animation::offline::SampleTrack(raw_animation, 0, .2f, &t));
  EXPECT_FLOAT3_EQ(t.translation, 2.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t.rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t.scale, 1.f, 1.f, 1.f);

  EXPECT_TRUE(ozz::animation::offline::SampleTrack(raw_animation, 0, 1.f, &t));
  EXPECT_FLOAT3_EQ(t.translation, 3.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t.rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t.scale, 1.f, 1.f, 1.f);

  EXPECT_TRUE(ozz::animation::offline::SampleTrack(raw_animation, 1, .2f, &t));
  EXPECT_FLOAT3_EQ(t.translation, 0.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t.rotation, 0.f, 1.f, 0.f, 0.f);
  EXPECT_FLOAT3_EQ(t.scale, 1.f, 1.f, 1.f);

  EXPECT_TRUE(ozz::animation::offline::SampleTrack(raw_animation, 2, .4f, &t));
  EXPECT_FLOAT3_EQ(t.translation, 0.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t.rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t.scale, 10.f, 0.f, 0.f);

  EXPECT_TRUE(ozz::animation::offline::SampleTrack(raw_animation, 3, 0.f, &t));
  EXPECT_FLOAT3_EQ(t.translation, 0.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t.rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t.scale, 1.f, 1.f, 1.f);

  EXPECT_TRUE(ozz::animation::offline::SampleTrack(raw_animation, 3, .5f, &t));
  EXPECT_FLOAT3_EQ(t.translation, 0.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t.rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t.scale, 1.f, 1.f, 1.f);
}

TEST(SampleAnimationInvalid, RawAnimationUtils) {
  // Create an invalid animation.
  RawAnimation raw_animation;
  raw_animation.duration = -1.f;

  ozz::math::Transform t[4];
  EXPECT_FALSE(ozz::animation::offline::Sample(raw_animation, .5f, t));
}

TEST(SampleAnimation, RawAnimationUtils) {

  RawAnimation raw_animation;
  raw_animation.duration = 1.f;
  raw_animation.tracks.resize(4);

  // Raw animation inputs.
  //     0              1
  // --------------------
  // 0 - A     B        |
  // 1 - C  D  E        |
  // 2 - F  G     H  I  J
  // 3 -                |

  RawAnimation::TranslationKey a = {0.f, ozz::math::Float3(1.f, 0.f, 0.f)};
  raw_animation.tracks[0].translations.push_back(a);
  RawAnimation::TranslationKey b = {.4f, ozz::math::Float3(3.f, 0.f, 0.f)};
  raw_animation.tracks[0].translations.push_back(b);

  RawAnimation::RotationKey c = {0.f, ozz::math::Quaternion(1.f, 0.f, 0.f, 0.f)};
  raw_animation.tracks[1].rotations.push_back(c);
  RawAnimation::RotationKey d = {0.2f, ozz::math::Quaternion(0.f, 1.f, 0.f, 0.f)};
  raw_animation.tracks[1].rotations.push_back(d);
  RawAnimation::RotationKey e = {0.4f, ozz::math::Quaternion(0.f, 0.f, 1.f, 0.f)};
  raw_animation.tracks[1].rotations.push_back(e);

  RawAnimation::ScaleKey f = {0.f, ozz::math::Float3(12.f, 0.f, 0.f)};
  raw_animation.tracks[2].scales.push_back(f);
  RawAnimation::ScaleKey g = {.2f, ozz::math::Float3(11.f, 0.f, 0.f)};
  raw_animation.tracks[2].scales.push_back(g);
  RawAnimation::ScaleKey h = {.6f, ozz::math::Float3(9.f, 0.f, 0.f)};
  raw_animation.tracks[2].scales.push_back(h);
  RawAnimation::ScaleKey i = {.8f, ozz::math::Float3(7.f, 0.f, 0.f)};
  raw_animation.tracks[2].scales.push_back(i);
  RawAnimation::ScaleKey j = {1.f, ozz::math::Float3(5.f, 0.f, 0.f)};
  raw_animation.tracks[2].scales.push_back(j);

  // Test invalid transform buffer sizes.

  ozz::math::Transform t1[1];
  EXPECT_FALSE(ozz::animation::offline::Sample(raw_animation, 0.f, t1));

  ozz::math::Transform t4[4];
  EXPECT_TRUE(ozz::animation::offline::Sample(raw_animation, -1.f, t4));
  EXPECT_FLOAT3_EQ(t4[0].translation, 1.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t4[0].rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[0].scale, 1.f, 1.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[1].translation, 0.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t4[1].rotation, 1.f, 0.f, 0.f, 0.f);
  EXPECT_FLOAT3_EQ(t4[1].scale, 1.f, 1.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[2].translation, 0.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t4[2].rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[2].scale, 12.f, 0.f, 0.f);

  EXPECT_TRUE(ozz::animation::offline::Sample(raw_animation, 0.f, t4));
  EXPECT_FLOAT3_EQ(t4[0].translation, 1.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t4[0].rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[0].scale, 1.f, 1.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[1].translation, 0.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t4[1].rotation, 1.f, 0.f, 0.f, 0.f);
  EXPECT_FLOAT3_EQ(t4[1].scale, 1.f, 1.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[2].translation, 0.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t4[2].rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[2].scale, 12.f, 0.f, 0.f);
  EXPECT_FLOAT3_EQ(t4[3].translation, 0.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t4[3].rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[3].scale, 1.f, 1.f, 1.f);
  
  EXPECT_TRUE(ozz::animation::offline::Sample(raw_animation, .2f, t4));
  EXPECT_FLOAT3_EQ(t4[0].translation, 2.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t4[0].rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[0].scale, 1.f, 1.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[1].translation, 0.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t4[1].rotation, 0.f, 1.f, 0.f, 0.f);
  EXPECT_FLOAT3_EQ(t4[1].scale, 1.f, 1.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[2].translation, 0.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t4[2].rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[2].scale, 11.f, 0.f, 0.f);

  EXPECT_TRUE(ozz::animation::offline::Sample(raw_animation, .4f, t4));
  EXPECT_FLOAT3_EQ(t4[1].translation, 0.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t4[1].rotation, 0.f, 0.f, 1.f, 0.f);
  EXPECT_FLOAT3_EQ(t4[1].scale, 1.f, 1.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[2].translation, 0.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t4[2].rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[2].scale, 10.f, 0.f, 0.f);
  EXPECT_FLOAT3_EQ(t4[3].translation, 0.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t4[3].rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[3].scale, 1.f, 1.f, 1.f);

  EXPECT_TRUE(ozz::animation::offline::Sample(raw_animation, 1.f, t4));
  EXPECT_FLOAT3_EQ(t4[0].translation, 3.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t4[0].rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[0].scale, 1.f, 1.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[1].translation, 0.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t4[1].rotation, 0.f, 0.f, 1.f, 0.f);
  EXPECT_FLOAT3_EQ(t4[1].scale, 1.f, 1.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[2].translation, 0.f, 0.f, 0.f);
  EXPECT_QUATERNION_EQ(t4[2].rotation, 0.f, 0.f, 0.f, 1.f);
  EXPECT_FLOAT3_EQ(t4[2].scale, 5.f, 0.f, 0.f);
}
