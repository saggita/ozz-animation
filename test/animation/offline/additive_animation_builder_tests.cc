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

#include "gtest/gtest.h"

#include "ozz/base/maths/math_constant.h"
#include "ozz/base/maths/gtest_math_helper.h"

#include "ozz/animation/offline/raw_animation.h"

using ozz::animation::offline::RawAnimation;
using ozz::animation::offline::AdditiveAnimationBuilder;

TEST(Error, AdditiveAnimationBuilder) {
  AdditiveAnimationBuilder builder;

  { // NULL output.
    RawAnimation input;
    EXPECT_TRUE(input.Validate());

    // Builds animation
    EXPECT_FALSE(builder(input, NULL));
  }

  { // Invalid input animation.
    RawAnimation input;
    input.duration = -1.f;
    EXPECT_FALSE(input.Validate());

    // Builds animation
    RawAnimation output;
    output.duration = -1.f;
    output.tracks.resize(1);
    EXPECT_FALSE(builder(input, &output));
    EXPECT_FLOAT_EQ(output.duration, RawAnimation().duration);
    EXPECT_EQ(output.num_tracks(), 0);
  }
}

TEST(Build, AdditiveAnimationBuilder) {
  AdditiveAnimationBuilder builder;

  RawAnimation input;
  input.duration = 1.f;
  input.tracks.resize(3);

  // First track is empty
  {
    // input.tracks[0]
  }

  // 2nd track
  // 1 key at the beginning
  {
    const RawAnimation::TranslationKey key = {
      0.f, ozz::math::Float3(2.f, 3.f, 4.f)};
    input.tracks[1].translations.push_back(key);
  }
  {
    const RawAnimation::RotationKey key = {
      0.f, ozz::math::Quaternion(.70710677f, 0.f, 0.f, .70710677f)};
    input.tracks[1].rotations.push_back(key);
  }
  {
    const RawAnimation::ScaleKey key = {
      0.f, ozz::math::Float3(5.f, 6.f, 7.f)};
    input.tracks[1].scales.push_back(key);
  }

  // 3rd track
  // 2 keys after the beginning
  {
    const RawAnimation::TranslationKey key0 = {
      .5f, ozz::math::Float3(2.f, 3.f, 4.f)};
    input.tracks[2].translations.push_back(key0);
    const RawAnimation::TranslationKey key1 = {
      .7f, ozz::math::Float3(20.f, 30.f, 40.f)};
    input.tracks[2].translations.push_back(key1);
  }
  {
    const RawAnimation::RotationKey key0 = {
      .5f, ozz::math::Quaternion(.70710677f, 0.f, 0.f, .70710677f)};
    input.tracks[2].rotations.push_back(key0);
    const RawAnimation::RotationKey key1 = {
      .7f, ozz::math::Quaternion(0.f, 0.f, 0.f, 1.f)};
    input.tracks[2].rotations.push_back(key1);
  }
  {
    const RawAnimation::ScaleKey key0 = {
      .5f, ozz::math::Float3(5.f, 6.f, 7.f)};
    input.tracks[2].scales.push_back(key0);
    const RawAnimation::ScaleKey key1 = {
      .7f, ozz::math::Float3(50.f, 60.f, 70.f)};
    input.tracks[2].scales.push_back(key1);
  }

  // Builds animation with very little tolerance.
  {
    RawAnimation output;
    ASSERT_TRUE(builder(input, &output));
    EXPECT_EQ(output.num_tracks(), 3);

    // 1st track.
    {
      EXPECT_EQ(output.tracks[0].translations.size(), 0u);
      EXPECT_EQ(output.tracks[0].rotations.size(), 0u);
      EXPECT_EQ(output.tracks[0].scales.size(), 0u);
    }

    // 2nd track.
    {
      const RawAnimation::JointTrack::Translations& translations =
        output.tracks[1].translations;
      EXPECT_EQ(translations.size(), 1u);
      EXPECT_FLOAT3_EQ(translations[0].value, 0.f, 0.f, 0.f);
      const RawAnimation::JointTrack::Rotations& rotations =
        output.tracks[1].rotations;
      EXPECT_EQ(rotations.size(), 1u);
      EXPECT_QUATERNION_EQ(rotations[0].value, 0.f, 0.f, 0.f, 1.f);
      const RawAnimation::JointTrack::Scales& scales =
        output.tracks[1].scales;
      EXPECT_EQ(scales.size(), 1u);
      EXPECT_FLOAT3_EQ(scales[0].value, 1.f, 1.f, 1.f);
    }

    // 3rd track.
    {
      const RawAnimation::JointTrack::Translations& translations =
        output.tracks[1].translations;
      EXPECT_EQ(translations.size(), 2u);
      EXPECT_FLOAT3_EQ(translations[0].value, 0.f, 0.f, 0.f);
      EXPECT_FLOAT3_EQ(translations[1].value, 18.f, 27.f, 36.f);
      const RawAnimation::JointTrack::Rotations& rotations =
        output.tracks[1].rotations;
      EXPECT_EQ(rotations.size(), 2u);
      EXPECT_QUATERNION_EQ(rotations[0].value, 0.f, 0.f, 0.f, 0.f);
      EXPECT_QUATERNION_EQ(rotations[1].value, -.70710677f, 0.f, 0.f, .70710677f);
      const RawAnimation::JointTrack::Scales& scales =
        output.tracks[1].scales;
      EXPECT_EQ(scales.size(), 2u);
      EXPECT_FLOAT3_EQ(scales[0].value, 1.f, 1.f, 1.f);
      EXPECT_FLOAT3_EQ(scales[1].value, 10.f, 10.f, 10.f);
    }
  }
}
