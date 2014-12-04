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
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"

#include "ozz/geometry/runtime/skinning_job.h"

#include "ozz/base/log.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/io/archive.h"

#include "ozz/base/memory/allocator.h"

#include "ozz/base/maths/vec_float.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/base/maths/box.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/soa_float4x4.h"

#include "ozz/options/options.h"

#include "framework/application.h"
#include "framework/renderer.h"
#include "framework/imgui.h"
#include "framework/utils.h"

<<<<<<< HEAD
#include "framework/mesh.h"
=======
#include "framework/skin_mesh.h"
>>>>>>> Move skin mesh and tools to sample framework.

// Skeleton archive can be specified as an option.
OZZ_OPTIONS_DECLARE_STRING(
  skeleton,
  "Path to the skeleton (ozz archive format).",
  "media/skeleton.ozz",
  false)

// Animation archive can be specified as an option.
OZZ_OPTIONS_DECLARE_STRING(
  animation,
  "Path to the animation (ozz archive format).",
  "media/animation.ozz",
  false)

// SkinMesh archive can be specified as an option.
OZZ_OPTIONS_DECLARE_STRING(
  mesh,
  "Path to the mesh (ozz archive format).",
  "media/mesh.ozz",
  false)

class SkinSampleApplication : public ozz::sample::Application {
 public:
  SkinSampleApplication()
    : show_influences_count_(false),
      limit_influences_count_(0),
      cache_(NULL) {
  }

 protected:
  // Updates current animation time.
  virtual bool OnUpdate(float _dt) {
    // Updates current animation time.
    controller_.Update(animation_, _dt);

    // Samples optimized animation at t = animation_time_.
    ozz::animation::SamplingJob sampling_job;
    sampling_job.animation = &animation_;
    sampling_job.cache = cache_;
    sampling_job.time = controller_.time();
    sampling_job.output = locals_;
    if (!sampling_job.Run()) {
      return false;
    }

    // Converts from local space to model space matrices.
    ozz::animation::LocalToModelJob ltm_job;
    ltm_job.skeleton = &skeleton_;
    ltm_job.input = locals_;
    ltm_job.output = models_;
    if (!ltm_job.Run()) {
      return false;
    }

    return true;
  }

  // Build skinning matrices, transform mesh vertices using the SkinningJob and
  // renders.
  virtual bool OnDisplay(ozz::sample::Renderer* _renderer) {

    // Builds skinning matrices, based on the output of the animation stage.
    for (int i = 0; i < skeleton_.num_joints(); ++i) {
      skinning_matrices_[i] = models_[i] * inverse_bind_pose_[i];
    }

    // Runs a skinning job per mesh part. Triangle indices are shared
    // across parts.
    int processed_vertex_count = 0;
    for (size_t i = 0; i < input_mesh_.parts.size(); ++i) {
      const ozz::sample::Mesh::Part& part = input_mesh_.parts[i];

      // Setup vertex and influence counts.
      const int part_vertex_count = part.vertex_count();

      // Skip this iteration if no vertex.
      if (part_vertex_count == 0) {
        continue;
      }

      // Fills the job.
      ozz::geometry::SkinningJob skinning_job;
      skinning_job.vertex_count = part_vertex_count;
      const int part_influences_count = part.influences_count();

      // Clamps joints influence count according to the option.
      skinning_job.influences_count =
        ozz::math::Min(limit_influences_count_, part_influences_count);

      // Setup skinning matrices, that came from the animation stage before being
      // multiplied by inverse model-space bind-pose.
      skinning_job.joint_matrices = skinning_matrices_;

      // Setup joint's indices.
      skinning_job.joint_indices = make_range(part.joint_indices);
      skinning_job.joint_indices_stride = sizeof(uint16_t) * part_influences_count;

      // Setup joint's weights.
      if (part_influences_count > 1) {
        skinning_job.joint_weights = make_range(part.joint_weights);
        skinning_job.joint_weights_stride = sizeof(float) * (part_influences_count - 1);
      }

      // Setup input positions, coming from the loaded mesh.
      skinning_job.in_positions = make_range(part.positions);
      skinning_job.in_positions_stride = sizeof(float) * 3;

      // Setup output positions, coming from the rendering output mesh buffers.
      // We need to offset the buffer every loop.
      skinning_job.out_positions.begin =
        array_begin(rendering_mesh_.parts[0].positions) + processed_vertex_count * 3;
      skinning_job.out_positions.end =
        skinning_job.out_positions.begin + part_vertex_count * 3;
      skinning_job.out_positions_stride = sizeof(float) * 3;

      // Setup input normals, coming from the loaded mesh.
      skinning_job.in_normals = make_range(part.normals);
      skinning_job.in_normals_stride = sizeof(float) * 3;

      // Setup output normals, coming from the rendering output mesh buffers.
      // We need to offset the buffer every loop.
      skinning_job.out_normals.begin =
        array_begin(rendering_mesh_.parts[0].normals) + processed_vertex_count * 3;
      skinning_job.out_normals.end =
        skinning_job.out_normals.begin + part_vertex_count * 3;
      skinning_job.out_normals_stride = sizeof(float) * 3;

      // Execute the job, which should succeed unless a parameter is invalid.
      if (!skinning_job.Run()) {
        return false;
      }

      // Some more vertices were processed.
      processed_vertex_count += part_vertex_count;
    }

    _renderer->DrawMesh(rendering_mesh_, ozz::math::Float4x4::identity());

    return true;
  }

  virtual bool OnInitialize() {
    ozz::memory::Allocator* allocator = ozz::memory::default_allocator();

    // Reading skeleton.
    if (!ozz::sample::LoadSkeleton(OPTIONS_skeleton, &skeleton_)) {
        return false;
    }

    // Reading animation.
    if (!ozz::sample::LoadAnimation(OPTIONS_animation, &animation_)) {
        return false;
    }

    // Allocates runtime buffers.
    const int num_joints = skeleton_.num_joints();
    const int num_soa_joints = skeleton_.num_soa_joints();
    locals_ = allocator->AllocateRange<ozz::math::SoaTransform>(num_soa_joints);
    models_ = allocator->AllocateRange<ozz::math::Float4x4>(num_joints);
    skinning_matrices_ = allocator->AllocateRange<ozz::math::Float4x4>(num_joints);

    // Allocates a cache that matches animation requirements.
    cache_ = allocator->New<ozz::animation::SamplingCache>(num_joints);

    // Reading mesh.
    if (!ozz::sample::LoadMesh(OPTIONS_mesh, &input_mesh_)) {
      return false;
    }

    // Init default value for influences count limitation option.
    limit_influences_count_ = input_mesh_.max_influences_count();

    if (!BuildInverseBindPose()) {
      return false;
    }

    if (!PrepareRenderingMesh()) {
      return false;
    }

    return true;
  }

  bool BuildInverseBindPose() {
    const int num_joints = skeleton_.num_joints();

    // Build inverse bind-pose matrices, based on the input skeleton.
    inverse_bind_pose_ = ozz::memory::default_allocator()->
      AllocateRange<ozz::math::Float4x4>(num_joints);

    // Convert skeleton bind-pose in local space to model-space matrices using
    // the LocalToModelJob. Output is stored directly inside inverse_bind_pose_
    // which will then be inverted in-place.
    ozz::animation::LocalToModelJob ltm_job;
    ltm_job.skeleton = &skeleton_;
    ltm_job.input = skeleton_.bind_pose();
    ltm_job.output = inverse_bind_pose_;
    if (!ltm_job.Run()) {
      return false;
    }

    // Invert matrices in-place.
    for (int i = 0; i < num_joints; ++i) {
      inverse_bind_pose_[i] = Invert(inverse_bind_pose_[i]);
    }

    return true;
  }

  // Prepares rendering mesh, which allocates the buffers that are filled as
  // output of the skinning job.
  // The sample uses sample framework mesh for rendering, but a real-world
  // rendering engine would expose dynamic vertex buffers.
  bool PrepareRenderingMesh() {

    // Setup vertex buffers
    const int vertex_count = input_mesh_.vertex_count();
    const int index_count = input_mesh_.triangle_index_count();
    rendering_mesh_.parts.resize(1); // One single part with all vertices.
    rendering_mesh_.parts[0].positions.resize(vertex_count * 3);  // 3 floats per position.
    rendering_mesh_.parts[0].normals.resize(vertex_count * 3);  // 3 floats per normal.

    // Copy indices.
    rendering_mesh_.triangle_indices.resize(index_count);
    for (int i = 0; i < index_count; ++i) {
      rendering_mesh_.triangle_indices[i] = input_mesh_.triangle_indices[i];
    }

    return SetupVertexColors();
  }

  bool SetupVertexColors() {
    // Makes sure color rendering buffer is big enough (4 uint8_t per vertex).
    rendering_mesh_.parts[0].colors.resize(input_mesh_.vertex_count() * 4);

    // Iterates parts and fills output color buffer.
    int processed_vertex_count = 0;
    for (size_t i = 0; i < input_mesh_.parts.size(); ++i) {
      const ozz::sample::Mesh::Part& part = input_mesh_.parts[i];

      uint8_t color[4] = {255, 255, 255, 255};
      // Computes vertex color based on the number of influencing vertices.
      if (show_influences_count_) {
        const int max_influences_count = ozz::math::Clamp(
          1,
          input_mesh_.max_influences_count() - 1,
          limit_influences_count_ - 1);
        const int part_influences_count = ozz::math::Min(
          limit_influences_count_, part.influences_count()) - 1;

        color[0] = static_cast<uint8_t>(
          part_influences_count * 255 / max_influences_count);
        color[1] = 255 - color[0];
        color[2] = 0;
      }
      const int part_vertex_count = part.vertex_count();
      for (int j = processed_vertex_count;
           j < processed_vertex_count + part_vertex_count;
           ++j) {
        uint8_t* output = &rendering_mesh_.parts[0].colors[j * 4];
        output[0] = color[0];
        output[1] = color[1];
        output[2] = color[2];
        output[3] = color[3];
      }

      // More vertices processed.
      processed_vertex_count += part_vertex_count;
    }

    return true;
  }

  virtual void OnDestroy() {
    ozz::memory::Allocator* allocator = ozz::memory::default_allocator();
    allocator->Deallocate(locals_);
    allocator->Deallocate(models_);
    allocator->Deallocate(skinning_matrices_);
    allocator->Deallocate(inverse_bind_pose_);
    allocator->Delete(cache_);
  }

  virtual bool OnGui(ozz::sample::ImGui* _im_gui) {
    { // Exposes animation runtime playback controls.
      static bool open = true;
      ozz::sample::ImGui::OpenClose oc(_im_gui, "Animation control", &open);
      if (open) {
        controller_.OnGui(animation_, _im_gui);
      }
    }

    { // Display sample options.
      static bool open = true;
      ozz::sample::ImGui::OpenClose oc(_im_gui, "Skinning options", &open);
      if (open) {
        bool colors_rebuild = false;
        char label[32];
        sprintf(label, "Limit influences: %d", limit_influences_count_);
        colors_rebuild |= _im_gui->DoSlider(
          label,
          1, input_mesh_.max_influences_count(), &limit_influences_count_);
        colors_rebuild |= _im_gui->DoCheckBox("Show influences", &show_influences_count_);

        // Rebuild vertex colors if an option has changed.
        if (colors_rebuild) {
          SetupVertexColors();
        }
      }
    }

    return true;
  }

  virtual void GetSceneBounds(ozz::math::Box* _bound) const {
    ozz::sample::ComputePostureBounds(models_, _bound);
  }

 private:

  // Option that allows to color the mesh according to the number of influences
  // per vertex.
  bool show_influences_count_;

  // Option that limits the number of influences.
  int limit_influences_count_;

  // Playback animation controller. This is a utility class that helps with
  // controlling animation playback time.
  ozz::sample::PlaybackController controller_;

  // Runtime skeleton.
  ozz::animation::Skeleton skeleton_;

  // Runtime animation.
  ozz::animation::Animation animation_;

  // Sampling cache.
  ozz::animation::SamplingCache* cache_;

  // Buffer of local transforms as sampled from animation_.
  ozz::Range<ozz::math::SoaTransform> locals_;

  // Buffer of model space matrices.
  ozz::Range<ozz::math::Float4x4> models_;

  // Buffer of skinning matrices.
  ozz::Range<ozz::math::Float4x4> skinning_matrices_;

  // Buffer of inverse bind-pose matrices. They are used during skinning to
  // convert vertices to joints local-space.
  ozz::Range<ozz::math::Float4x4> inverse_bind_pose_;

  // The input mesh containing skinning information (joint indices, weights...).
  // This mesh is loaded from a file.
  ozz::sample::Mesh input_mesh_;

  // The rendering mesh, filled with skinned vertices outputted from the
  // SkinningJob stage.
  // In a proper rendering engine (not a sample), this would be implemented with
  // dynamic vertex buffers.
  ozz::sample::Mesh rendering_mesh_;
};

int main(int _argc, const char** _argv) {
  const char* title =
    "Ozz-animation sample: Attachment to animated skeleton joints";
  return SkinSampleApplication().Run(_argc, _argv, "1.0", title);
}
