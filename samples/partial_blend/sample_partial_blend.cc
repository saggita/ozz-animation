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

#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/skeleton_utils.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/blending_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"

#include "ozz/base/log.h"

#include "ozz/base/containers/vector.h"

#include "ozz/base/maths/vec_float.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"

#include "ozz/base/memory/allocator.h"

#include "ozz/options/options.h"

#include "framework/application.h"
#include "framework/renderer.h"
#include "framework/imgui.h"
#include "framework/utils.h"

#include <cstring>

// Skeleton archive can be specified as an option.
OZZ_OPTIONS_DECLARE_STRING(
  skeleton,
  "Path to the skeleton (ozz archive format).",
  "media/skeleton.ozz",
  false)

// Lower body animation archive can be specified as an option.
OZZ_OPTIONS_DECLARE_STRING(
  lower_body_animation,
  "Path to the lower body animation(ozz archive format).",
  "media/walk.ozz",
  false)

// Upper body animation archive can be specified as an option.
OZZ_OPTIONS_DECLARE_STRING(
  upper_body_animation,
  "Path to the upper body animation (ozz archive format).",
  "media/crossarms.ozz",
  false)

class PartialBlendSampleApplication : public ozz::sample::Application {
 public:
  PartialBlendSampleApplication()
    : upper_body_root_(0),
      threshold_(ozz::animation::BlendingJob().threshold) {
  }

 protected:
  // Updates current animation time.
  virtual bool OnUpdate(float _dt) {
    // Updates and samples both animations to their respective local space
    // transform buffers.
    for (int i = 0; i < kNumLayers; ++i) {
      Sampler& sampler = samplers_[i];

      // Updates animations time.
      sampler.controller.Update(sampler.animation, _dt);

      // Setup sampling job.
      ozz::animation::SamplingJob sampling_job;
      sampling_job.animation = &sampler.animation;
      sampling_job.cache = sampler.cache;
      sampling_job.time = sampler.controller.time();
      sampling_job.output = sampler.locals;

      // Samples animation.
      if (!sampling_job.Run()) {
        return false;
      }
    }

    // Blends animations.
    // Blends the local spaces transforms computed by sampling all animations
    // (1st stage just above), and outputs the result to the local space
    // transform buffer blended_locals_

    // Prepares blending layers.
    ozz::animation::BlendingJob::Layer layers[kNumLayers];
    for (int i = 0; i < kNumLayers; ++i) {
      layers[i].transform = samplers_[i].locals;
      layers[i].weight = samplers_[i].weight_setting;

      // Set per-joint weights for the partially blended layer.
      layers[i].joint_weights = samplers_[i].joint_weights;
    }

    // Setups blending job.
    ozz::animation::BlendingJob blend_job;
    blend_job.threshold = threshold_; 
    blend_job.layers.begin = layers;
    blend_job.layers.end = layers + kNumLayers;
    blend_job.bind_pose = skeleton_.bind_pose();
    blend_job.output = blended_locals_;

    // Blends.
    if (!blend_job.Run()) {
      return false;
    }

    // Converts from local space to model space matrices.
    // Gets the ouput of the blending stage, and converts it to model space.

    // Setup local-to-model conversion job.
    ozz::animation::LocalToModelJob ltm_job;
    ltm_job.skeleton = &skeleton_;
    ltm_job.input = blended_locals_;
    ltm_job.output = models_;

    // Run ltm job.
    if (!ltm_job.Run()) {
      return false;
    }

    return true;
  }

  // Samples animation, transforms to model space and renders.
  virtual bool OnDisplay(ozz::sample::Renderer* _renderer) {
    return _renderer->DrawPosture(skeleton_,
                                  models_,
                                  ozz::math::Float4x4::identity());
  }

  virtual bool OnInitialize() {
    ozz::memory::Allocator* allocator = ozz::memory::default_allocator();

    // Reading skeleton.
    if (!ozz::sample::LoadSkeleton(OPTIONS_skeleton, &skeleton_)) {
        return false;
    }
    const int num_joints = skeleton_.num_joints();
    const int num_soa_joints = skeleton_.num_soa_joints();

    // Reading animations.
    const char* filenames[] = {
      OPTIONS_lower_body_animation, OPTIONS_upper_body_animation};
    for (int i = 0; i < kNumLayers; ++i) {
      Sampler& sampler = samplers_[i];

      if (!ozz::sample::LoadAnimation(filenames[i], &sampler.animation)) {
        return false;
      }

      // Allocates sampler runtime buffers.
      sampler.locals =
        allocator->AllocateRange<ozz::math::SoaTransform>(num_soa_joints);

      // Allocates per-joint weights used for the partial animation. Note that
      // this is a Soa structure.
      sampler.joint_weights =
        allocator->AllocateRange<ozz::math::SimdFloat4>(num_soa_joints);

      // Allocates a cache that matches animation requirements.
      sampler.cache = allocator->New<ozz::animation::SamplingCache>(num_joints);
    }

    // Default weight settings.
    Sampler& lower_body_sampler = samplers_[kLowerBody];
    lower_body_sampler.weight_setting = 1.f;
    lower_body_sampler.joint_weight_setting= 0.f;

    Sampler& upper_body_sampler = samplers_[kUpperBody];
    upper_body_sampler.weight_setting = 1.f;
    upper_body_sampler.joint_weight_setting = 1.f;

    // Allocates local space runtime buffers of blended data.
    blended_locals_ =
      allocator->AllocateRange<ozz::math::SoaTransform>(num_soa_joints);

    // Allocates model space runtime buffers of blended data.
    models_ = allocator->AllocateRange<ozz::math::Float4x4>(num_joints);

    // Finds the "Spine1" joint in the joint hierarchy.
    for (int i = 0; i < num_joints; ++i) {
      if (std::strstr(skeleton_.joint_names()[i], "Spine1")) {
        upper_body_root_ = i;
        break;
      }
    }
    SetupPerJointWeights();

    return true;
  }

  void SetupPerJointWeights() {
    // Setup partial animation mask. This mask is defined by a weight_setting
    // assigned to each joint of the hierarchy. Joint to disable are set to a
    // weight_setting of 0.f, and enabled joints are set to 1.f.
    // Per-joint weights of lower and upper body layers have opposed values
    // (weight_setting and 1 - weight_setting) in order for a layer to select
    // joints that are rejected by the other layer.
    Sampler& lower_body_sampler = samplers_[kLowerBody];
    Sampler& upper_body_sampler = samplers_[kUpperBody];

    // Disables all joints: set all weights to 0.
    for (int i = 0; i < skeleton_.num_soa_joints(); ++i) {
      lower_body_sampler.joint_weights[i] = ozz::math::simd_float4::one();
      upper_body_sampler.joint_weights[i] = ozz::math::simd_float4::zero();
    }

    // Extracts the list of children of the shoulder.
    ozz::animation::JointsIterator it;
    ozz::animation::IterateJointsDF(skeleton_, upper_body_root_, &it);

    // Sets the weight_setting of all the joints children of the arm to 1. Note
    // that weights are stored in SoA format.
    for (int i = 0; i < it.num_joints; ++i) {
      const int joint_id = it.joints[i];
      {  // Updates lower body animation sampler joint weights.
        ozz::math::SimdFloat4& weight_setting =
          lower_body_sampler.joint_weights[joint_id/4];
        weight_setting = ozz::math::SetI(
          weight_setting, joint_id %4, lower_body_sampler.joint_weight_setting);
      }
      {  // Updates upper body animation sampler joint weights.
        ozz::math::SimdFloat4& weight_setting =
          upper_body_sampler.joint_weights[joint_id/4];
        weight_setting = ozz::math::SetI(
          weight_setting, joint_id %4, upper_body_sampler.joint_weight_setting);
      }
    }
  }

  virtual void OnDestroy() {
    ozz::memory::Allocator* allocator = ozz::memory::default_allocator();
    for (int i = 0; i < kNumLayers; ++i) {
      Sampler& sampler = samplers_[i];
      allocator->Deallocate(sampler.locals);
      allocator->Deallocate(sampler.joint_weights);
      allocator->Delete(sampler.cache);
    }
    allocator->Deallocate(blended_locals_);
    allocator->Deallocate(models_.begin);
  }

  virtual bool OnGui(ozz::sample::ImGui* _im_gui) {
    // Exposes blending parameters.
    {
      static bool open = true;
      ozz::sample::ImGui::OpenClose oc(_im_gui, "Blending parameters", &open);
      if (open) {
        char label[64];

        static bool automatic = true;
        _im_gui->DoCheckBox("Use automatic blending settings", &automatic);

        static float coeff = 1.f;  // All power to the partial animation.
        std::sprintf(label, "Upper body weight: %.2f", coeff);
        _im_gui->DoSlider(label, 0.f, 1.f, &coeff, 1.f, automatic);

        Sampler& lower_body_sampler = samplers_[kLowerBody];
        Sampler& upper_body_sampler = samplers_[kUpperBody];

        if (automatic) {
          // Blending values are forced when "automatic" mode is selected.
          lower_body_sampler.weight_setting = 1.f;
          lower_body_sampler.joint_weight_setting = 1.f - coeff;
          upper_body_sampler.weight_setting = 1.f;
          upper_body_sampler.joint_weight_setting = coeff;
        }

        _im_gui->DoLabel("Manual settings:");
        _im_gui->DoLabel("Lower body layer:");
        std::sprintf(label, "Layer weight: %.2f",
          lower_body_sampler.weight_setting);
        _im_gui->DoSlider(label, 0.f, 1.f,
          &lower_body_sampler.weight_setting, 1.f, !automatic);
        std::sprintf(label, "Joints weight: %.2f",
          lower_body_sampler.joint_weight_setting);
        _im_gui->DoSlider(label, 0.f, 1.f,
          &lower_body_sampler.joint_weight_setting, 1.f, !automatic);
        _im_gui->DoLabel("Upper body layer:");
        std::sprintf(label, "Layer weight: %.2f",
          upper_body_sampler.weight_setting);
        _im_gui->DoSlider(label, 0.f, 1.f,
          &upper_body_sampler.weight_setting, 1.f, !automatic);
        std::sprintf(label, "Joints weight: %.2f",
          upper_body_sampler.joint_weight_setting);
        _im_gui->DoSlider(label, 0.f, 1.f,
          &upper_body_sampler.joint_weight_setting, 1.f, !automatic);

        std::sprintf(label, "Threshold: %.2f", threshold_);
        _im_gui->DoSlider(label, .01f, 1.f, &threshold_);

        SetupPerJointWeights();
      }
    }
    // Exposes selection of the root of the partial blending hierarchy.
    {
      static bool open = true;
      ozz::sample::ImGui::OpenClose oc(_im_gui, "Root", &open);
      if (open && skeleton_.num_joints() != 0) {
        _im_gui->DoLabel("Root of the upper body hierarchy:",
          ozz::sample::ImGui::kLeft, false);
        char label[64];
        std::sprintf(label, "%s (%d)",
          skeleton_.joint_names()[upper_body_root_],
          upper_body_root_);
        if (_im_gui->DoSlider(label,
          0, skeleton_.num_joints() - 1,
          &upper_body_root_)) {
            SetupPerJointWeights();
        }
      }
    }
    // Exposes animations runtime playback controls.
    {
      static bool oc_open = true;
      ozz::sample::ImGui::OpenClose oc(_im_gui, "Animation control", &oc_open);
      if (oc_open) {
        static bool open[kNumLayers] = {true, true};
        const char* oc_names[kNumLayers] = {
          "Lower body animation", "Upper body animation"};
        for (int i = 0; i < kNumLayers; ++i) {
          Sampler& sampler = samplers_[i];
          ozz::sample::ImGui::OpenClose oc(_im_gui, oc_names[i], NULL);
          if (open[i]) {
            sampler.controller.OnGui(sampler.animation, _im_gui);
          }
        }
      }
    }

    return true;
  }

  virtual void GetSceneBounds(ozz::math::Box* _bound) const {
    ozz::sample::ComputePostureBounds(models_, _bound);
  }

 private:

  // Runtime skeleton.
  ozz::animation::Skeleton skeleton_;

  // The number of layers to blend.
  enum {
    kLowerBody = 0,
    kUpperBody = 1,
    kNumLayers = 2,
  };

  // Sampler structure contains all the data required to sample a single
  // animation.
  struct Sampler {
    // Constructor, default initialization.
    Sampler()
     : weight_setting(1.f),
       joint_weight_setting(1.f),
       cache(NULL) {
    }

    // Playback animation controller. This is a utility class that helps with
    // controlling animation playback time.
    ozz::sample::PlaybackController controller;

    // Blending weight_setting for the layer.
    float weight_setting;

    // Blending weight_setting setting of the joints of this layer that are affected
    // by the masking.
    float joint_weight_setting;

    // Runtime animation.
    ozz::animation::Animation animation;

    // Sampling cache.
    ozz::animation::SamplingCache* cache;

    // Buffer of local transforms as sampled from animation_.
    ozz::Range<ozz::math::SoaTransform> locals;

    // Per-joint weights used to define the partial animation mask. Allows to
    // select which joints are considered during blending, and their individual
    // weight_setting.
    ozz::Range<ozz::math::SimdFloat4> joint_weights;
  } samplers_[kNumLayers];  // kNumLayers animations to blend.

  // Index of the joint at the base of the upper body hierarchy.
  int upper_body_root_;

  // Blending job bind pose threshold.
  float threshold_;

  // Buffer of local transforms which stores the blending result.
  ozz::Range<ozz::math::SoaTransform> blended_locals_;

  // Buffer of model space matrices. These are computed by the local-to-model
  // job after the blending stage.
  ozz::Range<ozz::math::Float4x4> models_;
};

int main(int _argc, const char** _argv) {
  const char* title = "Ozz-animation sample: Partial animations blending";
  return PartialBlendSampleApplication().Run(_argc, _argv, "1.0", title);
}
