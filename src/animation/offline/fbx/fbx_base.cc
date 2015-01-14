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

#include "ozz/animation/offline/fbx/fbx_base.h"

#include "ozz/base/log.h"

#include "ozz/base/memory/allocator.h"

#include "ozz/base/maths/transform.h"

namespace ozz {
namespace animation {
namespace offline {
namespace fbx {

FbxManagerInstance::FbxManagerInstance()
    : fbx_manager_(NULL) {
  // Instantiate Fbx manager, mostly a memory manager.
  fbx_manager_ = FbxManager::Create();

  // Logs SDK version.
  ozz::log::Log() << "FBX importer version " << fbx_manager_->GetVersion() <<
    "." << std::endl;
}

FbxManagerInstance::~FbxManagerInstance() {
  // Destroy the manager and all associated objects.
  fbx_manager_->Destroy();
  fbx_manager_ = NULL;
}

FbxDefaultIOSettings::FbxDefaultIOSettings(const FbxManagerInstance& _manager)
    : io_settings_(NULL) {
  io_settings_ = FbxIOSettings::Create(_manager, IOSROOT);
}

FbxDefaultIOSettings::~FbxDefaultIOSettings() {
  io_settings_->Destroy();
  io_settings_ = NULL;
}

FbxAnimationIOSettings::FbxAnimationIOSettings(const FbxManagerInstance& _manager)
    : FbxDefaultIOSettings(_manager) {
  settings()->SetBoolProp(IMP_FBX_MATERIAL, false);
  settings()->SetBoolProp(IMP_FBX_TEXTURE, false);
  settings()->SetBoolProp(IMP_FBX_MODEL, false);
  settings()->SetBoolProp(IMP_FBX_SHAPE, false);
  settings()->SetBoolProp(IMP_FBX_LINK, false);
  settings()->SetBoolProp(IMP_FBX_GOBO, false);
}

FbxSkeletonIOSettings::FbxSkeletonIOSettings(const FbxManagerInstance& _manager)
    : FbxDefaultIOSettings(_manager) {
  settings()->SetBoolProp(IMP_FBX_MATERIAL, false);
  settings()->SetBoolProp(IMP_FBX_TEXTURE, false);
  settings()->SetBoolProp(IMP_FBX_MODEL, false);
  settings()->SetBoolProp(IMP_FBX_SHAPE, false);
  settings()->SetBoolProp(IMP_FBX_LINK, false);
  settings()->SetBoolProp(IMP_FBX_GOBO, false);
  settings()->SetBoolProp(IMP_FBX_ANIMATION, false);
}

FbxSceneLoader::FbxSceneLoader(const char* _filename,
                               const char* _password,
                               const FbxManagerInstance& _manager,
                               const FbxDefaultIOSettings& _io_settings)
    : scene_(NULL),
    original_axis_system_(),
    original_system_unit_() {
  // Create an importer.
  FbxImporter* importer = FbxImporter::Create(_manager,"ozz importer");

  // Initialize the importer by providing a filename. Use all available plugins.
  const bool initialized = importer->Initialize(_filename, -1, _io_settings);

  // Get the version number of the FBX file format.
  int major, minor, revision;
  importer->GetFileVersion(major, minor, revision);

  if (!initialized)  // Problem with the file to be imported.
  {
    const FbxString error = importer->GetStatus().GetErrorString();
    ozz::log::Err() << "FbxImporter initialization failed with error: " <<
      error.Buffer() << std::endl;

    if (importer->GetStatus().GetCode() == FbxStatus::eInvalidFileVersion)
    {
      ozz::log::Err() << "FBX version number for " << _filename << " is " <<
        major << "." << minor<< "." << revision << "." << std::endl;
    }
  }

  if (initialized) {
    if ( importer->IsFBX()) {
      ozz::log::Log() << "FBX version number for " << _filename << " is " <<
        major << "." << minor<< "." << revision << "." << std::endl;
    }

    // Load the scene.
    scene_ = FbxScene::Create(_manager,"ozz scene");
    bool imported = importer->Import(scene_);

    if(!imported &&     // The import file may have a password
       importer->GetStatus().GetCode() == FbxStatus::ePasswordError)
    {
      _io_settings.settings()->SetStringProp(IMP_FBX_PASSWORD, _password);
      _io_settings.settings()->SetBoolProp(IMP_FBX_PASSWORD_ENABLE, true);

      // Retries to import the scene.
      imported = importer->Import(scene_);

      if(!imported &&
         importer->GetStatus().GetCode() == FbxStatus::ePasswordError)
      {
        ozz::log::Err() << "Incorrect password." << std::endl;

        // scene_ will be destroyed because imported is false.
      }
    }

    // Setup axis and system converter.
    if (imported) {
      FbxGlobalSettings& settings = scene_->GetGlobalSettings();
      converter_ = ozz::memory::default_allocator()->
        New<FbxSystemConverter>(settings.GetAxisSystem(),
                                settings.GetSystemUnit());
    }

    // Clear the scene if import failed.
    if (!imported) {
      scene_->Destroy();
      scene_ = NULL;
    }
  }

  // Destroy the importer
  importer->Destroy();
}

FbxSceneLoader::~FbxSceneLoader() {
  if (scene_) {
    scene_->Destroy();
    scene_ = NULL;
  }

  if (converter_) {
    ozz::memory::default_allocator()->Delete(converter_);
    converter_ = NULL;
  }
}

namespace {
ozz::math::Float4x4 BuildAxisSystemMatrix(const FbxAxisSystem& _system) {

  int sign;
  ozz::math::SimdFloat4 up = ozz::math::simd_float4::y_axis();
  ozz::math::SimdFloat4 at = ozz::math::simd_float4::z_axis();

  // The EUpVector specifies which axis has the up and down direction in the
  // system (typically this is the Y or Z axis). The sign of the EUpVector is
  // applied to represent the direction (1 is up and -1 is down relative to the
  // observer).
  const FbxAxisSystem::EUpVector eup = _system.GetUpVector(sign);
  switch (eup) {
    case FbxAxisSystem::eXAxis: {
      up = math::simd_float4::Load(1.f * sign, 0.f, 0.f, 0.f);
      // If the up axis is X, the remain two axes will be Y And Z, so the
      // ParityEven is Y, and the ParityOdd is Z
      if (_system.GetFrontVector(sign) == FbxAxisSystem::eParityEven) {
        at = math::simd_float4::Load(0.f, 1.f * sign, 0.f, 0.f);
      } else {
        at = math::simd_float4::Load(0.f, 0.f, 1.f * sign, 0.f);
      }
      break;
    }
    case FbxAxisSystem::eYAxis: {
      up = math::simd_float4::Load(0.f, 1.f * sign, 0.f, 0.f);
      // If the up axis is Y, the remain two axes will X And Z, so the
      // ParityEven is X, and the ParityOdd is Z
      if (_system.GetFrontVector(sign) == FbxAxisSystem::eParityEven) {
        at = math::simd_float4::Load(1.f * sign, 0.f, 0.f, 0.f);
      } else {
        at = math::simd_float4::Load(0.f, 0.f, 1.f * sign, 0.f);
      }
      break;
    }
    case FbxAxisSystem::eZAxis: {
      up = math::simd_float4::Load(0.f, 0.f, 1.f * sign, 0.f);
      // If the up axis is Z, the remain two axes will X And Y, so the
      // ParityEven is X, and the ParityOdd is Y
      if (_system.GetFrontVector(sign) == FbxAxisSystem::eParityEven) {
        at = math::simd_float4::Load(1.f * sign, 0.f, 0.f, 0.f);
      } else {
        at = math::simd_float4::Load(0.f, 1.f * sign, 0.f, 0.f);
      }
      break;
    }
    default: {
      assert(false && "Invalid FbxAxisSystem");
      break;
    }
  }

  // If the front axis and the up axis are determined, the third axis will be
  // automatically determined as the left one. The ECoordSystem enum is a
  // parameter to determine the direction of the third axis just as the
  // EUpVector sign. It determines if the axis system is right-handed or
  // left-handed just as the enum values.
  ozz::math::SimdFloat4 right;
  if (_system.GetCoorSystem() == FbxAxisSystem::eRightHanded) {
    right = math::Cross3(up, at);
  } else {
    right = math::Cross3(at, up);
  }

  const ozz::math::Float4x4 matrix = {{
    right, up, at, ozz::math::simd_float4::w_axis()}};

  return matrix;
}
}

FbxSystemConverter::FbxSystemConverter(const FbxAxisSystem& _from_axis,
                                       const FbxSystemUnit& _from_unit) {
  // Build axis system conversion matrix.
  const math::Float4x4 from_matrix = BuildAxisSystemMatrix(_from_axis);

  // Finds unit conversion ratio to meters, where GetScaleFactor() is in cm.
  const float to_meters =
    static_cast<float>(_from_unit.GetScaleFactor()) * .01f;

  // Builds conversion matrix.
  convert_ = Invert(from_matrix) *
             math::Float4x4::Scaling(math::simd_float4::Load1(to_meters));
}

math::Float4x4 FbxSystemConverter::ConvertMatrix(const FbxAMatrix& _m) const {
  const ozz::math::Float4x4 ret = {{
    ozz::math::simd_float4::Load(static_cast<float>(_m[0][0]),
                                 static_cast<float>(_m[0][1]),
                                 static_cast<float>(_m[0][2]),
                                 static_cast<float>(_m[0][3])),
    ozz::math::simd_float4::Load(static_cast<float>(_m[1][0]),
                                 static_cast<float>(_m[1][1]),
                                 static_cast<float>(_m[1][2]),
                                 static_cast<float>(_m[1][3])),
    ozz::math::simd_float4::Load(static_cast<float>(_m[2][0]),
                                 static_cast<float>(_m[2][1]),
                                 static_cast<float>(_m[2][2]),
                                 static_cast<float>(_m[2][3])),
    ozz::math::simd_float4::Load(static_cast<float>(_m[3][0]),
                                 static_cast<float>(_m[3][1]),
                                 static_cast<float>(_m[3][2]),
                                 static_cast<float>(_m[3][3])),
  }};
  return ConvertMatrix(ret);
}

math::Float4x4 FbxSystemConverter::ConvertMatrix(const math::Float4x4& _m) const {
  return convert_ * _m * Invert(convert_);
}

math::Float3 FbxSystemConverter::ConvertPoint(const math::Float3& _p) const {
/*
  switch (up_axis_) {
    case kXUp: return math::Float3(-_p.y * unit_, _p.x * unit_, _p.z * unit_);
    case kYUp: return math::Float3(_p.x * unit_, _p.y * unit_, _p.z * unit_);
    case kZUp: return math::Float3(_p.x * unit_, _p.z * unit_, -_p.y * unit_);
    default: {
      assert(false);
      return _p;
    }
  }
*/
  return _p;
}

math::Quaternion FbxSystemConverter::ConvertRotation(
  const math::Quaternion& _q) const {
/*
  switch (up_axis_) {
    case kXUp: return math::Quaternion(-_q.y, _q.x, _q.z, _q.w);
    case kYUp: return _q;
    case kZUp: return math::Quaternion(_q.x, _q.z, -_q.y, _q.w);
    default: {
      assert(false);
      return _q;
    }
  }
*/
  return _q;
}

math::Float3 FbxSystemConverter::ConvertScale(const math::Float3& _s) const {
/*
  switch (up_axis_) {
    case kXUp: return math::Float3(_s.y, _s.x, _s.z);
    case kYUp: return _s;
    case kZUp: return math::Float3(_s.x, _s.z, _s.y);
    default: {
      assert(false);
      return _s;
    }
  }
*/
  return _s;
}

math::Transform FbxSystemConverter::ConvertTransform(const FbxAMatrix& _m) const {
  const math::Float4x4 matrix = ConvertMatrix(_m);

  math::SimdFloat4 translation, rotation, scale;
  if (ToAffine(matrix, &translation, &rotation, &scale)) {
    ozz::math::Transform transform;
    math::Store3PtrU(translation, &transform.translation.x);
    math::StorePtrU(math::Normalize4(rotation), &transform.rotation.x);
    math::Store3PtrU(scale, &transform.scale.x);
    return transform;
  }
  return ozz::math::Transform::identity();
}

math::Transform FbxSystemConverter::ConvertTransform(
  const math::Transform& _t) const {
/*
  const math::Transform transform = {ConvertPoint(_t.translation),
                                     ConvertRotation(_t.rotation),
                                     ConvertScale(_t.scale)};
  return transform;
*/
  return _t;
}

bool FbxAMatrixToTransform(const FbxAMatrix& _matrix,
                           ozz::math::Transform* _transform) {

  const FbxVector4 translation = _matrix.GetT();
  _transform->translation = ozz::math::Float3(
    static_cast<float>(translation.mData[0]),
    static_cast<float>(translation.mData[1]),
    static_cast<float>(translation.mData[2]));

  const FbxQuaternion quaternion = _matrix.GetQ();
  _transform->rotation = ozz::math::Quaternion(
    static_cast<float>(quaternion.Buffer()[0]),
    static_cast<float>(quaternion.Buffer()[1]),
    static_cast<float>(quaternion.Buffer()[2]),
    static_cast<float>(quaternion.Buffer()[3]));

  const FbxVector4 scale = _matrix.GetS();
  _transform->scale = ozz::math::Float3(
    static_cast<float>(scale.mData[0]),
    static_cast<float>(scale.mData[1]),
    static_cast<float>(scale.mData[2]));

  return true;
}
}  // fbx
}  // ozz
}  // offline
}  // animation
