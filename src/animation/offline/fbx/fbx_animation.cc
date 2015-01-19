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

#define OZZ_INCLUDE_PRIVATE_HEADER  // Allows to include private headers.

#include "animation/offline/fbx/fbx_animation.h"

#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/offline/raw_animation.h"

#include "ozz/base/log.h"

#include "ozz/base/maths/transform.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/simd_math.h"

namespace ozz {
namespace animation {
namespace offline {
namespace fbx {

namespace {
bool ExtractAnimation(FbxScene* _scene,
                      FbxSystemConverter* _converter,
                      FbxAnimStack* anim_stack,
                      const Skeleton& _skeleton,
                      float _sampling_rate,
                      RawAnimation* _animation) {
  // Setup Fbx animation evaluator.
  _scene->SetCurrentAnimationStack(anim_stack);

  // Extract animation duration.
  float start, end;
  const FbxTakeInfo* take_info = _scene->GetTakeInfo(anim_stack->GetName());
  if (take_info)
  {
    start = static_cast<float>(
      take_info->mLocalTimeSpan.GetStart().GetSecondDouble());
    end = static_cast<float>(
      take_info->mLocalTimeSpan.GetStop().GetSecondDouble());
  }
  else
  {
    // Take the time line value.
    FbxTimeSpan lTimeLineTimeSpan;
    _scene->GetGlobalSettings().GetTimelineDefaultTimeSpan(lTimeLineTimeSpan);
    start = static_cast<float>(lTimeLineTimeSpan.GetStart().GetSecondDouble());
    end = static_cast<float>(lTimeLineTimeSpan.GetStop().GetSecondDouble());
  }

  // Animation duration could be 0 if it's just a pose. In this case we'll set a
  // default 1s duration.
  if (end > start) {
    _animation->duration = end - start;
  } else {
    _animation->duration = 1.f;
  }

  // Allocates all tracks with the same number of joints as the skeleton.
  // Tracks that would not be found will be set to skeleton bind-pose
  // transformation.
  _animation->tracks.resize(_skeleton.num_joints());

  // Iterate all skeleton joints and fills there track with key frames.
  FbxAnimEvaluator* evaluator = _scene->GetAnimationEvaluator();
  for (int i = 0; i < _skeleton.num_joints(); i++) {
    RawAnimation::JointTrack& track = _animation->tracks[i];

    // Find a node that matches skeleton joint.
    const char* joint_name = _skeleton.joint_names()[i];
    FbxNode* node = _scene->FindNodeByName(joint_name);

    if (!node) {
      // Empty joint track.
      ozz::log::Err() << "No animation track found for joint \"" << joint_name
        << "\". Using skeleton bind pose instead." << std::endl;

      // Get soa bind pose3
      const ozz::math::SoaTransform& soa_transform = _skeleton.bind_pose()[i / 4];

      // Build aos bind pose3
      ozz::math::SimdFloat4 translations[4];
      ozz::math::SimdFloat4 rotations[4];
      ozz::math::SimdFloat4 scales[4];

      ozz::math::Transpose3x4(&soa_transform.translation.x, translations);
      ozz::math::Transpose4x4(&soa_transform.rotation.x, rotations);
      ozz::math::Transpose3x4(&soa_transform.scale.x, scales);

      {
        RawAnimation::TranslationKey key;
        key.time = 0.f;
        ozz::math::Store3PtrU(translations[i % 4], &key.value.x);
        track.translations.push_back(key);
      }

      {
        RawAnimation::RotationKey key;
        key.time = 0.f;
        ozz::math::StorePtrU(rotations[i % 4], &key.value.x);
        track.rotations.push_back(key);
      }

      {
        RawAnimation::ScaleKey key;
        key.time = 0.f;
        ozz::math::Store3PtrU(scales[i % 4], &key.value.x);
        track.scales.push_back(key);
      }

      continue;
    }

    // Reserve keys in animation tracks (allocation strategy optimization
    // purpose).
    const float sampling_period = 1.f / _sampling_rate;
    const int max_keys =
      static_cast<int>(3.f + (end - start) / sampling_period);
    track.translations.reserve(max_keys);
    track.rotations.reserve(max_keys);
    track.scales.reserve(max_keys);

    // Evaluate joint transformation at the specified time.
    // Make sure to include "end" time, and loop once at least.
    bool loop_again = true;
    for (float t = start; loop_again; t += sampling_period) {
      if (t >= end) {
        t = end;
        loop_again = false;
      }

      // Evaluate local transform at fbx_time.
      bool root = _skeleton.joint_properties()[i].parent == Skeleton::kNoParentIndex;
      const ozz::math::Transform transform =
        _converter->ConvertTransform(
          root?evaluator->GetNodeGlobalTransform(node, FbxTimeSeconds(t)):
               evaluator->GetNodeLocalTransform(node, FbxTimeSeconds(t)));

      // Fills corresponding track.
      const float local_time = t - start;
      const RawAnimation::TranslationKey translation = {
        local_time, transform.translation};
      track.translations.push_back(translation);
      const RawAnimation::RotationKey rotation = {
        local_time, transform.rotation};
      track.rotations.push_back(rotation);
      const RawAnimation::ScaleKey scale = {
        local_time, transform.scale};
      track.scales.push_back(scale);
    }
  }

  // Output animation must be valid at that point.
  assert(_animation->Validate());

  return true;
}
}

bool ExtractAnimation(FbxScene* _scene,
                      FbxSystemConverter* _converter,
                      const Skeleton& _skeleton,
                      float _sampling_rate,
                      RawAnimation* _animation) {
  int anim_stacks_count = _scene->GetSrcObjectCount<FbxAnimStack>();

  // Early out if no animation's found.
  if(anim_stacks_count == 0) {
    ozz::log::Err() << "No animation found." << std::endl;
    return false;
  }
  
  if (anim_stacks_count > 1) {
    ozz::log::Log() << anim_stacks_count <<
      " animations found. Only the first one will be exported." << std::endl;
  }

  // Arbitrarily take the first animation of the stack.
  FbxAnimStack* anim_stack = _scene->GetSrcObject<FbxAnimStack>(0);
  ozz::log::Log() << "Extracting animation \"" << anim_stack->GetName() << "\""
    << std::endl;
  return ExtractAnimation(_scene,
                          _converter,
                          anim_stack,
                          _skeleton,
                          _sampling_rate,
                          _animation);
}
}  // fbx
}  // offline
}  // animation
}  // ozz
