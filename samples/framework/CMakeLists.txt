include_directories(${CMAKE_SOURCE_DIR}/extern/glfw/include)

add_library(sample_framework
  application.h
  application.cc
  imgui.h
  image.h
  image.cc
  profile.h
  profile.cc
  renderer.h
  utils.h
  utils.cc
  mesh.h
  mesh.cc
  internal/camera.h
  internal/camera.cc
  internal/immediate.h
  internal/immediate.cc
  internal/imgui_impl.h
  internal/imgui_impl.cc
  internal/renderer_impl.h
  internal/renderer_impl.cc
  internal/shader.h
  internal/shader.cc
  internal/shooter.h
  internal/shooter.cc)

# Samples requires OpenGL package.
if(NOT EMSCRIPTEN)
  find_package(OpenGL REQUIRED)
  add_subdirectory(${CMAKE_SOURCE_DIR}/extern/glfw glfw)

  target_link_libraries(sample_framework
    glfw)
endif()

set_target_properties(sample_framework
  PROPERTIES FOLDER "samples/utils")

# Adds fbx2mesh utility target.
if(ozz_build_fbx)
  include_directories(${FBX_INCLUDE_DIRS})

  add_executable(sample_fbx2mesh
    fbx2mesh.cc
    mesh.h
    mesh.cc)
  target_link_libraries(sample_fbx2mesh
    ozz_animation_fbx
    ozz_animation_offline
    ozz_animation
    ozz_options
    ozz_base)
  set_target_properties(sample_fbx2mesh
    PROPERTIES FOLDER "samples/utils")

  add_custom_command(TARGET sample_fbx2mesh POST_BUILD
    DEPENDS
      "${ozz_media_directory}/fbx/alain/skeleton.fbx"
    COMMAND sample_fbx2mesh "--file=${ozz_media_directory}/fbx/alain/skeleton.fbx" "--skeleton=${ozz_media_directory}/bin/skeleton_v1_le.ozz" "--mesh=${ozz_media_directory}/bin/mesh.ozz")

  set(ozz_mesh_generation_dependency sample_fbx2mesh)

  add_test(NAME sample_fbx2mesh COMMAND sample_fbx2mesh "--file=${ozz_media_directory}/fbx/alain/skeleton.fbx" "--skeleton=${ozz_media_directory}/bin/skeleton_v1_le.ozz" "--mesh=${ozz_temp_directory}/mesh.ozz")

  add_test(NAME sample_fbx2mesh_invalid_file COMMAND sample_fbx2mesh "--file=${ozz_temp_directory}/dont_exist.fbx" "--skeleton=${ozz_media_directory}/bin/skeleton_v1_le.ozz" "--mesh=${ozz_temp_directory}/should_not_exist.ozz")
  set_tests_properties(sample_fbx2mesh_invalid_file PROPERTIES WILL_FAIL true)

  add_test(NAME sample_fbx2mesh_invalid_skeleton COMMAND sample_fbx2mesh "--file=${ozz_media_directory}/fbx/alain/skeleton.fbx" "--skeleton=${ozz_media_directory}/bin/animation.ozz" "--mesh=${ozz_temp_directory}/should_not_exist.ozz")
  set_tests_properties(sample_fbx2mesh_invalid_skeleton PROPERTIES WILL_FAIL true)

  # Ensures nothing was outputted. Unfortunately there's no way to ensure this test is ran last
  add_test(NAME sample_fbx2mesh_output COMMAND ${CMAKE_COMMAND} -E copy "${ozz_temp_directory}/should_not_exist.ozz" "${ozz_temp_directory}/should_not_exist_too.ozz")
  set_tests_properties(sample_fbx2mesh_output PROPERTIES WILL_FAIL true)
endif()
