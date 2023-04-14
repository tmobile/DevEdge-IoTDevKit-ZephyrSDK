# libs/CMakeLists.txt

target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dfu_gecko)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dfu_murata_1sc)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dfu_rs9116w)
