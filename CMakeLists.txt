cmake_minimum_required (VERSION 3.15.0 FATAL_ERROR)

project (XDEGraph LANGUAGES CXX)

set (OpenCASCADE_DIR_TOOLTIP "OCCT root directory")

set (OpenCASCADE_DIR "" CACHE PATH "${OpenCASCADE_DIR_TOOLTIP}")
if (NOT OpenCASCADE_DIR)
  message (FATAL_ERROR "Cannot find OCCT. Specify OpenCASCADE_DIR")
endif()

set (OpenCASCADE_NOTFOUND_MSG "Cannot find OCCT in the specified directory \"${OpenCASCADE_DIR}\". Check OpenCASCADE_DIR variable.")

file (GLOB_RECURSE OpenCASCADE_CONFIG "${OpenCASCADE_DIR}/*OpenCASCADEConfig.cmake")
if (NOT EXISTS "${OpenCASCADE_CONFIG}")
  message (FATAL_ERROR "${OpenCASCADE_NOTFOUND_MSG}")
endif()

get_filename_component (OCCT_CMAKE_PATH ${OpenCASCADE_CONFIG} DIRECTORY)

list (APPEND CMAKE_PREFIX_PATH
  "${OCCT_CMAKE_PATH}"
)

find_package (OpenCASCADE REQUIRED)

if (OpenCASCADE_FOUND)
  set (OpenCASCADE_DIR "${OpenCASCADE_INSTALL_PREFIX}" CACHE PATH "${OpenCASCADE_DIR_TOOLTIP}" FORCE)

  message (STATUS "Using OpenCASCADE from \"${OpenCASCADE_INSTALL_PREFIX}\"")
else()
  message (FATAL_ERROR "${OpenCASCADE_NOTFOUND_MSG}")
endif()

add_executable (${PROJECT_NAME}
  main.cpp
)

target_include_directories (${PROJECT_NAME} PRIVATE
  ${OpenCASCADE_INCLUDE_DIR}
)

target_link_directories    (${PROJECT_NAME} PRIVATE
  ${OpenCASCADE_LIBRARY_DIR}
)

target_link_libraries      (${PROJECT_NAME} PRIVATE
  TKDraw
  TKDCAF
  TKXCAF
)

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/run.bat.in" "${CMAKE_CURRENT_BINARY_DIR}/run.bat" @ONLY)
