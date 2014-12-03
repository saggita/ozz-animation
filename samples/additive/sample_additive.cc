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
  animation,
  "Path to the lower body animation(ozz archive format).",
  "media/walk.ozz",
  false)

// Additive animation archive can be specified as an option.
OZZ_OPTIONS_DECLARE_STRING(
  additive_animation,
  "Path to the upper body additive animation (ozz archive format).",
  "media/additive.ozz",
  false)

class AdditiveBlendSampleApplication : public ozz::sample::Application {
 public:
  AdditiveBlendSampleApplication()
    : upper_body_root_(0),
      upper_body_mask_enable_(true),
      upper_body_joint_weight_setting_(1.f),
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
    ozz::animation::BlendingJob::Layer layers[1];
    for (int i = 0; i < 1; ++i) {
      layers[i].transform = samplers_[i].locals;
      layers[i].weight = samplers_[i].weight_setting;
    }

    ozz::animation::BlendingJob::Layer additive_layers[1];
    for (int i = 1; i < 2; ++i) {
      additive_layers[0].transform = samplers_[i].locals;
      additive_layers[0].weight = samplers_[i].weight_setting;

      // Set per-joint weights for the additive blended layer.
      if (upper_body_mask_enable_) {
        additive_layers[0].joint_weights = upper_body_joint_weights_;
      }
    }

    // Setups blending job.
    ozz::animation::BlendingJob blend_job;
    blend_job.threshold = threshold_;
    blend_job.layers = layers;
    blend_job.additive_layers = additive_layers;
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
      OPTIONS_animation, OPTIONS_additive_animation};
    for (int i = 0; i < kNumLayers; ++i) {
      Sampler& sampler = samplers_[i];

      if (!ozz::sample::LoadAnimation(filenames[i], &sampler.animation)) {
        return false;
      }

      // Allocates sampler runtime buffers.
      sampler.locals =
        allocator->AllocateRange<ozz::math::SoaTransform>(num_soa_joints);

      // Allocates a cache that matches animation requirements.
      sampler.cache = allocator->New<ozz::animation::SamplingCache>(num_joints);
    }

    // Default weight settings.
    samplers_[kMainAnimation].weight_setting = 1.f;

    upper_body_joint_weight_setting_ = 1.f;
    samplers_[kAdditiveAnimation].weight_setting = 1.f;

    // Allocates local space runtime buffers of blended data.
    blended_locals_ =
      allocator->AllocateRange<ozz::math::SoaTransform>(num_soa_joints);

    // Allocates model space runtime buffers of blended data.
    models_ = allocator->AllocateRange<ozz::math::Float4x4>(num_joints);

    // Allocates per-joint weights used for the partial additive animation.
    // Note that this is a Soa structure.
    upper_body_joint_weights_ =
      allocator->AllocateRange<ozz::math::SimdFloat4>(num_soa_joints);

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

    // Disables all joints: set all weights to 0.
    for (int i = 0; i < skeleton_.num_soa_joints(); ++i) {
     upper_body_joint_weights_[i] = ozz::math::simd_float4::zero();
    }

    // Extracts the list of children of the shoulder.
    ozz::animation::JointsIterator it;
    ozz::animation::IterateJointsDF(skeleton_, upper_body_root_, &it);

    // Sets the weight_setting of all the joints children of the arm to 1. Note
    // that weights are stored in SoA format.
    for (int i = 0; i < it.num_joints; ++i) {
      const int joint_id = it.joints[i];
      {  // Updates upper body animation sampler joint weights.
        ozz::math::SimdFloat4& weight_setting =
          upper_body_joint_weights_[joint_id/4];
        weight_setting = ozz::math::SetI(
          weight_setting, joint_id %4, upper_body_joint_weight_setting_);
      }
    }
  }

  virtual void OnDestroy() {
    ozz::memory::Allocator* allocator = ozz::memory::default_allocator();
    for (int i = 0; i < kNumLayers; ++i) {
      Sampler& sampler = samplers_[i];
      allocator->Deallocate(sampler.locals);
      allocator->Delete(sampler.cache);
    }
    allocator->Deallocate(upper_body_joint_weights_);
    allocator->Deallocate(blended_locals_);
    allocator->Deallocate(models_.begin);
  }

  virtual bool OnGui(ozz::sample::ImGui* _im_gui) {
    char label[64];

    // Exposes blending parameters.
    {
      static bool open = true;
      ozz::sample::ImGui::OpenClose oc(_im_gui, "Blending parameters", &open);
      if (open) {
        _im_gui->DoLabel("Main layer:");
        std::sprintf(label, "Layer weight: %.2f",
                     samplers_[kMainAnimation].weight_setting);
        _im_gui->DoSlider(label, 0.f, 1.f,
                          &samplers_[kMainAnimation].weight_setting, 1.f);
        _im_gui->DoLabel("Additive layer:");
        std::sprintf(label, "Layer weight: %.2f",
                     samplers_[kAdditiveAnimation].weight_setting);
        _im_gui->DoSlider(label, -1.f, 1.f,
                          &samplers_[kAdditiveAnimation].weight_setting, 1.f);
        _im_gui->DoLabel("Global settings:");
        std::sprintf(label, "Threshold: %.2f", threshold_);
        _im_gui->DoSlider(label, .01f, 1.f, &threshold_);
      }
    }
    // Exposes selection of the root of the partial blending hierarchy.
    {
      static bool open = true;
      ozz::sample::ImGui::OpenClose oc(_im_gui, "Upper body masking", &open);

      if (open) {
        _im_gui->DoCheckBox("Enable mask", &upper_body_mask_enable_);

        std::sprintf(label, "Joints weight: %.2f",
                     upper_body_joint_weight_setting_);
        _im_gui->DoSlider(label, 0.f, 1.f,
                          &upper_body_joint_weight_setting_, 1.f,
                          upper_body_mask_enable_);

        if (skeleton_.num_joints() != 0) {
          _im_gui->DoLabel("Root of the upper body hierarchy:",
                           ozz::sample::ImGui::kLeft, false);
          std::sprintf(label, "%s (%d)",
                       skeleton_.joint_names()[upper_body_root_],
                       upper_body_root_);
          _im_gui->DoSlider(label,
                            0, skeleton_.num_joints() - 1,
                            &upper_body_root_, 1.f, upper_body_mask_enable_);
        }
        SetupPerJointWeights();
      }
    }
    // Exposes animations runtime playback controls.
    {
      static bool oc_open = true;
      ozz::sample::ImGui::OpenClose oc(_im_gui, "Animation control", &oc_open);
      if (oc_open) {
        static bool open[kNumLayers] = {true, true};
        const char* oc_names[kNumLayers] = {
          "Main animation", "Additive animation"};
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
    kMainAnimation = 0,
    kAdditiveAnimation = 1,
    kNumLayers = 2,
  };

  // Sampler structure contains all the data required to sample a single
  // animation.
  struct Sampler {
    // Constructor, default initialization.
    Sampler()
     : weight_setting(1.f),
       cache(NULL) {
    }

    // Playback animation controller. This is a utility class that helps with
    // controlling animation playback time.
    ozz::sample::PlaybackController controller;

    // Blending weight_setting for the layer.
    float weight_setting;

    // Runtime animation.
    ozz::animation::Animation animation;

    // Sampling cache.
    ozz::animation::SamplingCache* cache;

    // Buffer of local transforms as sampled from animation_.
    ozz::Range<ozz::math::SoaTransform> locals;

  } samplers_[kNumLayers];  // kNumLayers animations to blend.

  // Index of the joint at the base of the upper body hierarchy.
  int upper_body_root_;

  // Enables upper boddy per-joint weights.
  bool upper_body_mask_enable_;

  // Blending weight_setting setting of the joints of this layer that are affected
  // by the masking.
  float upper_body_joint_weight_setting_;

  // Per-joint weights used to define the partial animation mask. Allows to
  // select which joints are considered during blending, and their individual
  // weight_setting.
  ozz::Range<ozz::math::SimdFloat4> upper_body_joint_weights_;

  // Blending job bind pose threshold.
  float threshold_;

  // Buffer of local transforms which stores the blending result.
  ozz::Range<ozz::math::SoaTransform> blended_locals_;

  // Buffer of model space matrices. These are computed by the local-to-model
  // job after the blending stage.
  ozz::Range<ozz::math::Float4x4> models_;
};

int main(int _argc, const char** _argv) {
  const char* title = "Ozz-animation sample: Additive animations blending";
  return AdditiveBlendSampleApplication().Run(_argc, _argv, "1.0", title);
}
