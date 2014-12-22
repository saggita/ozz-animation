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

#include "framework/utils.h"

#include <limits>
#include <cassert>

#include "ozz/base/maths/box.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/memory/allocator.h"

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/local_to_model_job.h"

#include "ozz/geometry/runtime/skinning_job.h"

#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/log.h"

#include "framework/imgui.h"
#include "framework/mesh.h"

namespace ozz {
namespace sample {

PlaybackController::PlaybackController()
  : time_(0.f),
    playback_speed_(1.f),
    play_(true) {
}

void PlaybackController::Update(const animation::Animation& _animation,
                                float _dt) {
  if (!play_) {
    return;
  }
  const float new_time = time_ + _dt * playback_speed_;
  const float loops = new_time / _animation.duration();
  time_ = new_time - floorf(loops) * _animation.duration();
}

void PlaybackController::Reset() {
  time_ = 0.f;
  playback_speed_ = 1.f;
  play_ = true;
}

void PlaybackController::OnGui(const animation::Animation& _animation,
                               ImGui* _im_gui,
                               bool _enabled) {
  if (_im_gui->DoButton(play_ ? "Pause" : "Play", _enabled)) {
    play_ = !play_;
  }
  char szLabel[64];
  std::sprintf(szLabel, "Animation time: %.2f", time_);
  if (_im_gui->DoSlider(
    szLabel, 0.f, _animation.duration(), &time_, 1.f, _enabled)) {
    // Pause the time if slider as moved.
    play_ = false;
  }
  std::sprintf(szLabel, "Playback speed: %.2f", playback_speed_);
  _im_gui->DoSlider(szLabel, -5.f, 5.f, &playback_speed_, 1.f, _enabled);

  // Allow to reset speed if it is not the default value.
  if (_im_gui->DoButton(
    "Reset playback speed", playback_speed_ != 1.f && _enabled)) {
    playback_speed_ = 1.f;
  }
}

// Uses LocalToModelJob to compute skeleton model space posture, then forwards
// to ComputePostureBounds
void ComputeSkeletonBounds(const animation::Skeleton& _skeleton,
                           math::Box* _bound) {
  using ozz::math::Float4x4;

  assert(_bound);

  // Set a default box.
  *_bound = ozz::math::Box();

  const int num_joints = _skeleton.num_joints();
  if (!num_joints) {
    return;
  }

  // Allocate matrix array, out of memory is handled by the LocalToModelJob.
  memory::Allocator* allocator = memory::default_allocator();
  ozz::Range<ozz::math::Float4x4> models =
    allocator->AllocateRange<ozz::math::Float4x4>(num_joints);
  if (!models.begin) {
    return;
  }

  // Compute model space bind pose.
  ozz::animation::LocalToModelJob job;
  job.input = _skeleton.bind_pose();
  job.output = models;
  job.skeleton = &_skeleton;
  if (job.Run()) {
    // Forwards to posture function.
    ComputePostureBounds(models, _bound);
  }

  allocator->Deallocate(models);
}

// Loop through matrices and collect min and max bounds.
void ComputePostureBounds(ozz::Range<const ozz::math::Float4x4> _matrices,
                          math::Box* _bound) {
  assert(_bound);

  // Set a default box.
  *_bound = ozz::math::Box();

  if (!_matrices.begin || !_matrices.end) {
    return;
  }
  if (_matrices.begin > _matrices.end) {
    return;
  }

  math::SimdFloat4 min =
    math::simd_float4::Load1(std::numeric_limits<float>::max());
  math::SimdFloat4 max = -min;
  const ozz::math::Float4x4* current = _matrices.begin;
  while (current < _matrices.end) {
    min = math::Min(min, current->cols[3]);
    max = math::Max(max, current->cols[3]);
    ++current;
  }

  math::Store3PtrU(min, &_bound->min.x);
  math::Store3PtrU(max, &_bound->max.x);

  return;
}

bool LoadSkeleton(const char* _filename,
                  ozz::animation::Skeleton* _skeleton) {
  assert(_filename && _skeleton);
  ozz::log::Out() << "Loading skeleton archive " << _filename <<
    "." << std::endl;
  ozz::io::File file(_filename, "rb");
  if (!file.opened()) {
    ozz::log::Err() << "Failed to open skeleton file " << _filename << "."
      << std::endl;
    return false;
  }
  ozz::io::IArchive archive(&file);
  if (!archive.TestTag<ozz::animation::Skeleton>()) {
    ozz::log::Err() << "Failed to load skeleton instance from file " <<
      _filename << "." << std::endl;
    return false;
  }

  // Once the tag is validated, reading cannot fail.
  archive >> *_skeleton;

  return true;
}

bool LoadAnimation(const char* _filename,
                   ozz::animation::Animation* _animation) {
  assert(_filename && _animation);
  ozz::log::Out() << "Loading animation archive: " << _filename <<
    "." << std::endl;
  ozz::io::File file(_filename, "rb");
  if (!file.opened()) {
    ozz::log::Err() << "Failed to open animation file " << _filename <<
      "." << std::endl;
    return false;
  }
  ozz::io::IArchive archive(&file);
  if (!archive.TestTag<ozz::animation::Animation>()) {
    ozz::log::Err() << "Failed to load animation instance from file " <<
      _filename << "." << std::endl;
    return false;
  }

  // Once the tag is validated, reading cannot fail.
  archive >> *_animation;

  return true;
}

bool LoadMesh(const char* _filename,
              ozz::sample::Mesh* _mesh) {
  assert(_filename && _mesh);
  ozz::log::Out() << "Loading mesh archive: " << _filename <<
    "." << std::endl;
  ozz::io::File file(_filename, "rb");
  if (!file.opened()) {
    ozz::log::Err() << "Failed to open mesh file " << _filename <<
      "." << std::endl;
    return false;
  }
  ozz::io::IArchive archive(&file);
  if (!archive.TestTag<ozz::sample::Mesh>()) {
    ozz::log::Err() << "Failed to load mesh instance from file " <<
      _filename << "." << std::endl;
    return false;
  }

  // Once the tag is validated, reading cannot fail.
  archive >> *_mesh;

  return true;
}

SkinningUpdater::SkinningUpdater()
    : input_mesh_(NULL),
      skinned_mesh_(NULL)
{
  ozz::memory::Allocator* allocator = ozz::memory::default_allocator();
  input_mesh_ = allocator->New<Mesh>();
  skinned_mesh_ = allocator->New<Mesh>();
}

SkinningUpdater::~SkinningUpdater() {
  ozz::memory::Allocator* allocator = ozz::memory::default_allocator();
  allocator->Delete(input_mesh_);
  allocator->Delete(skinned_mesh_);
  allocator->Deallocate(inverse_bind_pose_);
  allocator->Deallocate(skinning_matrices_);
}

bool SkinningUpdater::Load(const char* _filename,
                           const animation::Skeleton& _skeleton) {
  assert(input_mesh_ && skinned_mesh_);

  // Reset current data.
  *input_mesh_ = Mesh();
  *skinned_mesh_ = Mesh();
  ozz::memory::default_allocator()->Deallocate(inverse_bind_pose_);
  ozz::memory::default_allocator()->Deallocate(skinning_matrices_);

  // Load input mesh from file.
  if (!LoadMesh(_filename, input_mesh_)) {
    return false;
  }

  if (input_mesh_->vertex_count() == 0) {
    return true;
  }

  // Setup output/skinned mesh. Rendering mesh has a single part.
  skinned_mesh_->parts.resize(1);
  int vertex_count = input_mesh_->vertex_count();
  skinned_mesh_->parts[0].positions.resize(vertex_count * 3);
  skinned_mesh_->parts[0].normals.resize(vertex_count * 3);
  skinned_mesh_->parts[0].colors.resize(vertex_count * 4);

  // Copy vertex colors at initialization, has they are not modified by skinning.
  int processed_vertex_count = 0;
  for (size_t i = 0; i < input_mesh_->parts.size(); ++i) {
    const ozz::sample::Mesh::Part& part = input_mesh_->parts[i];

    const uint8_t color[4] = {255, 255, 255, 255};
    const int part_vertex_count = part.vertex_count();

    for (int j = processed_vertex_count;
         j < processed_vertex_count + part_vertex_count;
         ++j) {
      uint8_t* output = &skinned_mesh_->parts[0].colors[j * 4];
      output[0] = color[0];
      output[1] = color[1];
      output[2] = color[2];
      output[3] = color[3];
    }

    // More vertices processed.
    processed_vertex_count += part_vertex_count;
  }

  // Setup inverse bind pose matrices.
  const int num_joints = _skeleton.num_joints();

  // Allocates skinning matrices.
  skinning_matrices_ = ozz::memory::default_allocator()->
    AllocateRange<ozz::math::Float4x4>(num_joints);

  // Build inverse bind-pose matrices, based on the input skeleton.
  inverse_bind_pose_ = ozz::memory::default_allocator()->
    AllocateRange<ozz::math::Float4x4>(num_joints);

  // Convert skeleton bind-pose in local space to model-space matrices using
  // the LocalToModelJob. Output is stored directly inside inverse_bind_pose_
  // which will then be inverted in-place.
  ozz::animation::LocalToModelJob ltm_job;
  ltm_job.skeleton = &_skeleton;
  ltm_job.input = _skeleton.bind_pose();
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

bool SkinningUpdater::Update(const Range<math::Float4x4>& _model_space) {
  assert(input_mesh_ && skinned_mesh_);

  // Ensures input matrices buffer has the correct size.
  const size_t joints_count = _model_space.Count();
  if (joints_count != skinning_matrices_.Count()) {
    return false;
  }

  // Early out if no mesh (or an empty mesh) was loaded.
  if (input_mesh_->vertex_count() == 0) {
    return true;
  }

  // Builds skinning matrices, based on the output of the animation stage.
  for (size_t i = 0; i < joints_count; ++i) {
    skinning_matrices_[i] = _model_space[i] * inverse_bind_pose_[i];
  }

  // Runs a skinning job per mesh part. Triangle indices are shared
  // across parts.
  int processed_vertex_count = 0;
  for (size_t i = 0; i < input_mesh_->parts.size(); ++i) {
    const ozz::sample::Mesh::Part& part = input_mesh_->parts[i];

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
    skinning_job.influences_count = part_influences_count;

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
      array_begin(skinned_mesh_->parts[0].positions) + processed_vertex_count * 3;
    skinning_job.out_positions.end =
      skinning_job.out_positions.begin + part_vertex_count * 3;
    skinning_job.out_positions_stride = sizeof(float) * 3;

    // Setup normals if input are provided.
    if (part.normals.size() != 0) {
      // Setup input normals, coming from the loaded mesh.
      skinning_job.in_normals = make_range(part.normals);
      skinning_job.in_normals_stride = sizeof(float) * 3;

      // Setup output normals, coming from the rendering output mesh buffers.
      // We need to offset the buffer every loop.
      skinning_job.out_normals.begin =
        array_begin(skinned_mesh_->parts[0].normals) + processed_vertex_count * 3;
      skinning_job.out_normals.end =
        skinning_job.out_normals.begin + part_vertex_count * 3;
      skinning_job.out_normals_stride = sizeof(float) * 3;
    }

    // Execute the job, which should succeed unless a parameter is invalid.
    if (!skinning_job.Run()) {
      return false;
    }

    // Some more vertices were processed.
    processed_vertex_count += part_vertex_count;
  }

  return true;
}
}  // sample
}  // ozz
