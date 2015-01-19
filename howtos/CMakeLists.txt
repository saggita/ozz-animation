if(NOT ozz_build_howtos)
  return()
endif()

# custom_skeleton_importer

add_executable(custom_skeleton_importer
  custom_skeleton_importer.cc)
target_link_libraries(custom_skeleton_importer
  ozz_animation_offline
  ozz_animation
  ozz_base)
set_target_properties(custom_skeleton_importer
  PROPERTIES FOLDER "howtos")
add_test(NAME custom_skeleton_importer COMMAND custom_skeleton_importer)

# custom_animation_importer

add_executable(custom_animation_importer
  custom_animation_importer.cc)
target_link_libraries(custom_animation_importer
  ozz_animation_offline
  ozz_animation
  ozz_base)
set_target_properties(custom_animation_importer
  PROPERTIES FOLDER "howtos")
add_test(NAME custom_animation_importer COMMAND custom_animation_importer)
