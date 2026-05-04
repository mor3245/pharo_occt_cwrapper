cmake_minimum_required(VERSION 3.24)

project(my_occt_c_wrapper LANGUAGES CXX)

# Set default build type if not specified
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# NOTE: Hardcoding paths in the CMakeLists.txt is discouraged. 
# Consider passing these via command line (-DOpenCASCADE_DIR=...) or using CMakePresets.json.
set(OpenCASCADE_DIR
    "C:/dev/opencascade/occt_install/v8_0_0_rc5/cmake"
    CACHE PATH "Directory containing OpenCASCADEConfig.cmake")

set(OCCT_SOURCE_DIR
    "C:/dev/opencascade/OCCT"
    CACHE PATH "OpenCASCADE source checkout used to build the debug libraries")

find_package(OpenCASCADE REQUIRED COMPONENTS FoundationClasses ModelingData ModelingAlgorithms)

add_library(my_occt_c_wrapper SHARED)

target_sources(my_occt_c_wrapper
    PRIVATE
        src/my_c_wrapper.cpp
    PUBLIC
        FILE_SET HEADERS
        FILES
            src/my_c_wrapper.h
)

target_link_libraries(my_occt_c_wrapper PRIVATE
    TKPrim
    TKMesh
)

# FIXED: 'PRIVATE_HEADER' is an invalid scope. Changed to 'PRIVATE'.
# FIXED: Typo '_CRT_SECURE_NO_WRANINGS' to '_CRT_SECURE_NO_WARNINGS'.
target_compile_definitions(my_occt_c_wrapper PRIVATE
    _CRT_SECURE_NO_WARNINGS
)

# RESTRICTED: Wrapped in $<CONFIG:Debug> so these MSVC flags don't pollute Release builds.
target_compile_options(my_occt_c_wrapper PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Zi /Od /Ob0>
)

target_link_options(my_occt_c_wrapper PRIVATE
  $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/DEBUG:FULL>
)

# CLEANUP: Removed the empty set_target_properties block and grouped them all here.
set_target_properties(my_occt_c_wrapper PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:my_occt_c_wrapper>"
    VS_DEBUGGER_ENVIRONMENT "PATH=${OpenCASCADE_BINARY_DIR};%PATH%"
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# CAUTION: Globbing massive directories like OCCT can significantly slow down the CMake configure step.
if(EXISTS "${OCCT_SOURCE_DIR}/src")
  file(GLOB_RECURSE OCCT_DEBUG_SOURCE_HINTS
    CONFIGURE_DEPENDS
    "${OCCT_SOURCE_DIR}/src/*.cxx"
    "${OCCT_SOURCE_DIR}/src/*.hxx"
    "${OCCT_SOURCE_DIR}/src/*.lxx"
    "${OCCT_SOURCE_DIR}/src/*.ixx"
  )

  target_sources(my_occt_c_wrapper PRIVATE ${OCCT_DEBUG_SOURCE_HINTS})
  set_source_files_properties(${OCCT_DEBUG_SOURCE_HINTS} PROPERTIES HEADER_FILE_ONLY ON)
  source_group(TREE "${OCCT_SOURCE_DIR}/src" PREFIX "OpenCASCADE" FILES ${OCCT_DEBUG_SOURCE_HINTS})
endif()

add_custom_command(TARGET my_occt_c_wrapper POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_RUNTIME_DLLS:my_occt_c_wrapper>
          $<TARGET_FILE_DIR:my_occt_c_wrapper>
  COMMAND_EXPAND_LISTS
)