cmake_minimum_required(VERSION 3.13)

project(LBANN CXX)

# Prevent in-source builds
if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR
    "In-source build attempted; please clean the CMake cache and then "
    "switch to an out-of-source build, e.g.,\n"
    "rm -rf CMakeCache.txt CMakeFiles/\nmkdir build && "
    "cd build && cmake <options> ..\n")
endif ()

# Add CMake modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)
include(LBANNCMakeUtilities)

#FIXME
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if (CMAKE_BUILD_TYPE MATCHES Debug)
  set(LBANN_DEBUG TRUE)
endif ()

if (NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS ON)
endif ()

# Build with at least C++11 standard; allow newer standards.
if (NOT CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD EQUAL 98)
  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
endif ()

# Convenience defines
string(TOUPPER "${PROJECT_NAME}" UPPER_PROJECT_NAME)
string(TOLOWER "${PROJECT_NAME}" LOWER_PROJECT_NAME)

# This will help define some targets later
if (CMAKE_VERSION VERSION_LESS 3.9)
  set(LBANN_PUBLIC_LINK_FLAG)
else ()
  set(LBANN_PUBLIC_LINK_FLAG "PUBLIC")
endif ()

#
# Version setup
#

set(LBANN_VERSION_MAJOR 0)
set(LBANN_VERSION_MINOR 99)
set(LBANN_VERSION_PATCH 0)

set(LBANN_VERSION "${LBANN_VERSION_MAJOR}.${LBANN_VERSION_MINOR}.${LBANN_VERSION_PATCH}")

# Check to see if we are in a git repo
find_program(__GIT_EXECUTABLE git)
mark_as_advanced(__GIT_EXECUTABLE)
if (__GIT_EXECUTABLE)

  execute_process(
    COMMAND ${__GIT_EXECUTABLE} rev-parse --is-inside-work-tree
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE __BUILDING_FROM_GIT_SOURCES
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  if (__BUILDING_FROM_GIT_SOURCES)
    # Get the git version so that we can embed it into the executable
    execute_process(
      COMMAND ${__GIT_EXECUTABLE} rev-parse --show-toplevel
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE __GIT_TOPLEVEL_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(
      COMMAND ${__GIT_EXECUTABLE} rev-parse --git-dir
      WORKING_DIRECTORY "${__GIT_TOPLEVEL_DIR}"
      OUTPUT_VARIABLE __GIT_GIT_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(
      COMMAND ${__GIT_EXECUTABLE} --git-dir "${__GIT_GIT_DIR}" describe
      --abbrev=7 --always --dirty --tags
      WORKING_DIRECTORY "${__GIT_TOPLEVEL_DIR}"
      OUTPUT_VARIABLE __GIT_DESCRIBE_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE)

    set(LBANN_GIT_VERSION "${__GIT_DESCRIBE_VERSION}"
      CACHE STRING "LBANN's version string as told by git.")
  endif (__BUILDING_FROM_GIT_SOURCES)
endif (__GIT_EXECUTABLE)

if (CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
  set(LBANN_GNU_LINUX TRUE)
endif ()

#
# Options
#

option(LBANN_WITH_ALUMINUM "Enable Aluminum all-reduce library" OFF)

option(LBANN_WITH_CNPY "Include cnpy" ON)

option(LBANN_WITH_CONDUIT "Enable Conduit library" ON)

option(LBANN_WITH_CUDNN "Include Nvidia cuDNN" ON)

option(LBANN_WITH_HWLOC
  "Enable topology-aware optimizations" ON)

option(LBANN_WITH_NVPROF
  "Enable NVTX-based instrumentation for nvprof" OFF)

option(LBANN_WITH_PYTHON
  "Install Python frontend and enable embedded Python" ON)

option(LBANN_WITH_TBINF "Include Tensorboard interface" ON)

option(LBANN_WITH_VTUNE
  "Link the Intel VTune profiling library" OFF)

option(LBANN_WITH_UNIT_TESTING
  "Enable the unit testing framework (requires Catch2)" OFF)

# Enable parallel random matrix generation, if possible
option(LBANN_DETERMINISTIC
  "Use deterministic algorithms as much as possible." OFF)

option(LBANN_SEQUENTIAL_INITIALIZATION
  "Sequentially consistent initialization" OFF)

option(LBANN_DEBUG_PRINT_SUBTARGETS
  "Turn on debugging output of internal target properties." OFF)
mark_as_advanced(LBANN_DEBUG_PRINT_SUBTARGETS)

# This option is off by default because non-developers should not use
# this option under normal circumstances.
option(LBANN_WARNINGS_AS_ERRORS
  "Build with warnings promoted to errors." OFF)
mark_as_advanced(LBANN_WARNINGS_AS_ERRORS)

#
# The datatype option is not binary
#

option(LBANN_WITH_DISTCONV
  "Enable Distconv distributed convolution" OFF)

set(LBANN_DISTCONV_NUM_DIMS 4 CACHE STRING "The number of tensor dimensions")

option(LBANN_WITH_P2P
  "Enable P2P library" OFF)

option(LBANN_DISTCONV_COSMOFLOW_KEEP_INT16
  "Keep Cosmoflow input samples in int16 as long as possible" OFF)

# FIXME what if "fp16"?
set(LBANN_DATATYPE "float"
  CACHE STRING "The datatype to use in LBANN")

option(LBANN_IO_DISABLE_ZEROS "Disable zero-clearing of matrices at data loading" ON)

#
# Initialize build
#

# Get installation directories -- these get used in various places;
# best to just make them available
include(GNUInstallDirs)

include(SetupCXX)

################################################################
# Initialize dependencies
################################################################

# Required dependencies
find_package(Threads REQUIRED)

find_package(CEREAL NO_MODULE
  HINTS ${CEREAL_DIR} $ENV{CEREAL_DIR}
  PATH_SUFFIXES share/cmake/cereal
  NO_DEFAULT_PATH)
if (NOT CEREAL_FOUND)
  find_package(CEREAL NO_MODULE REQUIRED)
endif ()
set(LBANN_HAS_CEREAL ${CEREAL_FOUND})
# The imported target is just called "cereal". Super.

# Setup the linear algebra library
find_package(Hydrogen 1.2.0 NO_MODULE QUIET
  HINTS ${Hydrogen_DIR} ${HYDROGEN_DIR} $ENV{Hydrogen_DIR} $ENV{HYDROGEN_DIR}
  PATH_SUFFIXES lib/cmake/hydrogen
  NO_DEFAULT_PATH)
if (NOT Hydrogen_FOUND)
  find_package(Hydrogen 1.2.0 NO_MODULE QUIET REQUIRED)
endif ()
message(STATUS "Found Hydrogen: ${Hydrogen_DIR}")
set(LBANN_HAS_HYDROGEN ${Hydrogen_FOUND})

include(SetupOpenMP)
include(SetupMPI)
include(SetupProtobuf)

# OpenCV installs a CMake configure file we can exploit
find_package(OpenCV NO_MODULE QUIET
  HINTS ${OpenCV_DIR} ${OPENCV_DIR} $ENV{OpenCV_DIR} $ENV{OPENCV_DIR}
  PATH_SUFFIXES share/OpenCV
  NO_DEFAULT_PATH)
if (NOT OpenCV_FOUND)
  find_package(OpenCV NO_MODULE QUIET REQUIRED)
endif ()
message(STATUS "Found OpenCV: ${OpenCV_DIR}")
set(LBANN_HAS_OPENCV ${OpenCV_FOUND})

# CUDA-ness of LBANN is 1:1 with Hydrogen. Iff Hydrogen has CUDA, LBANN gets CUDA.
set(LBANN_HAS_CUDA ${_HYDROGEN_HAVE_CUDA})
set(LBANN_WITH_CUDA ${LBANN_HAS_CUDA})

if (LBANN_HAS_CUDA)
  enable_language(CUDA)

  if (NOT CMAKE_CUDA_STANDARD OR CMAKE_CUDA_STANDARD EQUAL 98)
    set(CMAKE_CUDA_STANDARD 11)
  endif ()

  set(CMAKE_CUDA_STANDARD_REQUIRED TRUE)
endif ()

if (LBANN_WITH_ALUMINUM)
  # Aluminum may have already been found by Hydrogen
  if (NOT Aluminum_FOUND)
    find_package(Aluminum 0.2.0 NO_MODULE QUIET
      HINTS ${Aluminum_DIR} ${ALUMINUM_DIR} ${AL_DIR}
      $ENV{Aluminum_DIR} $ENV{ALUMINUM_DIR} $ENV{AL_DIR}
      PATH_SUFFIXES lib64/cmake/aluminum lib/cmake/aluminum
      NO_DEFAULT_PATH)
    if (NOT Aluminum_FOUND)
      find_package(Aluminum 0.2.0 NO_MODULE QUIET)
    endif ()
  endif ()
  set(LBANN_HAS_ALUMINUM ${Aluminum_FOUND})

  # Report failure.
  if (NOT LBANN_HAS_ALUMINUM)
    message(WARNING
      "Requested LBANN_WITH_ALUMINUM but Aluminum not found. "
      "Aluminum is now disabled. "
      "Try specifying ALUMINUM_DIR as the root of an ALUMINUM install. "
      "Alternatively, build with LBANN_WITH_ALUMINUM=OFF to suppress "
      "this warning.")
    set(LBANN_WITH_ALUMINUM OFF)

  else ()

    message(STATUS "Found Aluminum: ${Aluminum_DIR}")

    if (AL_HAS_CUDA AND NOT LBANN_HAS_CUDA)
      message(WARNING
        "Aluminum has CUDA support but CUDA support has not been found.")
    endif ()

    option(LBANN_BUILT_WITH_SPECTRUM "LBANN was built with Spectrum MPI" OFF)
    if (LBANN_BUILT_WITH_SPECTRUM)
      set(LBANN_ALUMINUM_MPI_PASSTHROUGH ON)
    endif (LBANN_BUILT_WITH_SPECTRUM)

  endif (NOT LBANN_HAS_ALUMINUM)
endif (LBANN_WITH_ALUMINUM)

# Setup some additional CUDA-y things
if (LBANN_HAS_CUDA)
  if (NOT LBANN_WITH_CUDNN)
    message(WARNING
      "Despite the fact that it looks optional, cuDNN is currently required "
      "when building with CUDA support. You have tried LBANN_WITH_CUDNN=OFF. "
      "It will be searched for anyway.")
    set(LBANN_WITH_CUDNN ON)
  endif ()

  include(SetupCUDAToolkit)

  set(LBANN_HAS_CUDNN ${CUDNN_FOUND})

  if (LBANN_HAS_ALUMINUM AND AL_HAS_NCCL)
    set(LBANN_HAS_NCCL2 TRUE)
  else ()
    set(LBANN_HAS_NCCL2 FALSE)
  endif ()
endif (LBANN_HAS_CUDA)

# This shouldn't be here, but is ok for now. This will occasionally be
# part of another TPL's libraries (e.g., MKL), but it's no
# guarantee. There's no harm including it multiple times.
if (NOT DL_LIBRARY)
  find_library(DL_LIBRARY dl DOC "The dynamic loader library.")
  if (DL_LIBRARY)
    message(STATUS "Found dl: ${DL_LIBRARY}")
  else ()
    message(FATAL_ERROR
      "dl library not found! This is a required library.\n"
      "Please add the path to libdl to CMAKE_LIBRARY_PATH.")
  endif (DL_LIBRARY)
endif ()

# Other optional dependencies
if (LBANN_WITH_TBINF)
  add_subdirectory(external/TBinf)
endif ()

# Find Python
# Note: This uses the Python module in cmake/modules, not the module
# that comes included with CMake. See the file for a discussion of the
# differences.
if (LBANN_WITH_PYTHON)
  find_package(Python REQUIRED)
  set(LBANN_HAS_PYTHON "${Python_FOUND}")
  if (NOT Python_VERSION_MAJOR EQUAL 3)
    set(LBANN_HAS_PYTHON FALSE)
    message(FATAL_ERROR "Python 2 is not supported.")
  endif ()

  # Setup the installation stuff
  set(PYTHON_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}"
    CACHE PATH "The prefix for the python installation")

  set(CMAKE_INSTALL_PYTHONDIR
    "lib/python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}/site-packages"
    CACHE PATH
    "Relative path from PYTHON_INSTALL_PREFIX to the python package install")

endif (LBANN_WITH_PYTHON)

if (LBANN_WITH_VTUNE)
  find_package(VTune MODULE)

  if (VTune_FOUND)
    set(LBANN_VTUNE TRUE)
    set(LBANN_HAS_VTUNE TRUE)
  else ()
    set(LBANN_VTUNE FALSE)
    set(LBANN_HAS_VTUNE FALSE)
    set(LBANN_WITH_VTUNE OFF)
    message(WARNING
      "Requested LBANN_WITH_VTUNE=ON, but VTUNE support not detected. "
      "Support NOT enabled. "
      "Try setting VTUNE_DIR to point to the VTune install prefix "
      "and reconfigure.")
  endif (VTune_FOUND)
endif (LBANN_WITH_VTUNE)

if (LBANN_WITH_CUDA AND LBANN_WITH_NVPROF)
  set(LBANN_NVPROF TRUE)
endif ()

if (LBANN_WITH_CNPY)
  find_package(CNPY REQUIRED)
  set(LBANN_HAS_CNPY ${CNPY_FOUND})
endif (LBANN_WITH_CNPY)

if (LBANN_WITH_HWLOC)
  find_package(HWLOC REQUIRED)
  set(LBANN_TOPO_AWARE ${HWLOC_FOUND})
endif (LBANN_WITH_HWLOC)

if (LBANN_WITH_CONDUIT)
  # Apparently we have to find HDF5, too.
  find_package(HDF5 CONFIG QUIET
    HINTS ${HDF5_DIR} $ENV{HDF5_DIR}
    PATH_SUFFIXES share/cmake/hdf5
    NO_DEFAULT_PATH)
  if (NOT HDF5_FOUND)
    find_package(HDF5 CONFIG QUIET)
  endif ()
  if (NOT HDF5_FOUND)
    enable_language(C) # WHY??????????????
    find_package(HDF5 REQUIRED)
    set(HDF5_FOUND_WITH_MODULE TRUE)
  else ()
    message(STATUS "Found HDF5: ${HDF5_DIR}")
  endif ()

  find_package(Conduit CONFIG QUIET
    HINTS ${Conduit_DIR} $ENV{Conduit_DIR} ${CONDUIT_DIR} $ENV{CONDUIT_DIR}
    PATH_SUFFIXES lib64/cmake lib/cmake
    NO_DEFAULT_PATH)
  if (NOT Conduit_FOUND)
    find_package(Conduit CONFIG QUIET REQUIRED
      PATH_SUFFIXES lib64/cmake lib/cmake)
  endif ()
  message(STATUS "Found CONDUIT: ${Conduit_DIR}")

  # Ugh. I don't like that this requires intimate knowledge of
  # specific targets that CONDUIT exports. It should support
  # components.
  if (NOT TARGET conduit_relay_mpi)
    message(FATAL_ERROR "CONDUIT does not have proper MPI support.")
  endif ()

  if (NOT TARGET conduit OR NOT TARGET conduit_relay
      OR NOT TARGET conduit_blueprint)
    message(FATAL_ERROR "Missing some CONDUIT required library.")
  endif ()

  if (NOT TARGET conduit::conduit)
    add_library(conduit::conduit INTERFACE IMPORTED)
  endif ()

  set(_conduit_interface_link_libs
    "conduit;conduit_relay;conduit_relay_mpi;conduit_blueprint")

  # Remove -pthread from linkage, if found
  foreach (_lib IN LISTS _conduit_interface_link_libs)
    if (TARGET ${_lib})
      get_property(_tmp_interface_link_libs TARGET ${_lib}
        PROPERTY INTERFACE_LINK_LIBRARIES)

      list(FIND _tmp_interface_link_libs "-pthread" _pthread_idx)
      if (_pthread_idx GREATER_EQUAL 0)
        list(REMOVE_AT _tmp_interface_link_libs ${_pthread_idx})

        set_property(TARGET ${_lib} PROPERTY
          INTERFACE_LINK_LIBRARIES ${_tmp_interface_link_libs})
      endif ()

      get_property(_tmp_interface_compile_opts TARGET ${_lib}
        PROPERTY INTERFACE_COMPILE_OPTIONS)
      set_property(TARGET ${_lib}
        PROPERTY INTERFACE_COMPILE_OPTIONS
          $<$<COMPILE_LANGUAGE:CXX>:${_tmp_interface_compile_opts}>)
    endif ()
  endforeach ()

  get_filename_component(_conduit_include_dirs
    "${CONDUIT_INCLUDE_DIRS}" DIRECTORY)

  if (HDF5_FOUND_WITH_MODULE)
    list(APPEND _conduit_interface_link_libs
      ${HDF5_LIBRARIES})

    list(APPEND _conduit_include_dirs
      "${HDF5_INCLUDE_DIRS}")
  endif ()

  set_property(TARGET conduit::conduit
    PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES
    "${_conduit_include_dirs}")

  set_target_properties(conduit::conduit
    PROPERTIES
    INTERFACE_LINK_LIBRARIES
    "${_conduit_interface_link_libs}")

  set(CONDUIT_LIBRARIES conduit::conduit)
endif (LBANN_WITH_CONDUIT)

if (LBANN_WITH_DISTCONV)
  message(STATUS "DISTCONV requested")
  find_package(P2P)
  find_package(DISTCONV)
  set(LBANN_HAS_DISTCONV ${DISTCONV_FOUND})
  if (NOT LBANN_HAS_DISTCONV)
    message(FATAL_ERROR
      "Requested LBANN_WITH_DISTCONV but Distconv not found. "
      "Distconv is now disabled. "
      "Try specifying DISTCONV_DIR as the root of a DISTCONV install. "
      "Alternatively, build with LBANN_WITH_DISTCONV=OFF.")
    set(LBANN_WITH_DISTCONV OFF)
  endif(NOT LBANN_HAS_DISTCONV)
  if (NOT P2P_FOUND)
    message(FATAL_ERROR
      "Requested LBANN_WITH_DISTCONV but its required package, P2P, not found. "
      "Distconv is now disabled. "
      "Try specifying P2P_DIR as the root of a P2P install. "
      "Alternatively, build with LBANN_WITH_DISTCONV=OFF.")
    set(LBANN_WITH_DISTCONV OFF)
  endif(NOT P2P_FOUND)
endif (LBANN_WITH_DISTCONV)

if (LBANN_WITH_UNIT_TESTING)
  find_package(Catch2 2.0.0 CONFIG QUIET
    HINTS ${CATCH2_DIR} $ENV{CATCH2_DIR} ${CATCH_DIR} $ENV{CATCH_DIR}
    PATH_SUFFIXES lib64/cmake/Catch2 lib/cmake/Catch2
    NO_DEFAULT_PATH)
  if (NOT Catch2_FOUND)
    find_package(Catch2 2.0.0 CONFIG QUIET REQUIRED)
  endif ()
  message(STATUS "Found Catch2: ${Catch2_DIR}")

  # Now that Catch2 has been found, start adding the unit tests
  include(CTest)
  include(Catch)
  add_subdirectory(src/proto/unit_test)
  add_subdirectory(src/utils/unit_test)
  add_subdirectory(src/transforms/unit_test)
  add_subdirectory(src/transforms/vision/unit_test)

  # Add this one last
  add_subdirectory(unit_test)
endif (LBANN_WITH_UNIT_TESTING)

# Handle the documentation
add_subdirectory(docs)

################################################################
# Build LBANN
################################################################

# Add LBANN source files
add_subdirectory(include)
add_subdirectory(src)

# Write the configure file
configure_file(
  "${CMAKE_SOURCE_DIR}/cmake/configure_files/lbann_config.hpp.in"
  "${CMAKE_BINARY_DIR}/lbann_config.hpp"
  @ONLY)

# Create the LBANN library
add_library(lbann ${LBANN_SOURCES} ${LBANN_HEADERS} ${LBANN_CUDA_SOURCES})

target_include_directories(lbann PUBLIC
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}>)

if (LBANN_HAS_PYTHON)
  target_include_directories(lbann PUBLIC ${Python_INCLUDE_DIRS})
endif ()

# Use the IMPORTED targets when possible.
target_link_libraries(lbann PUBLIC LbannProto)
target_link_libraries(lbann PUBLIC Threads::Threads)
target_link_libraries(lbann PUBLIC cereal)
target_link_libraries(lbann PUBLIC OpenMP::OpenMP_CXX)
target_link_libraries(lbann PUBLIC MPI::MPI_CXX)
target_link_libraries(lbann PUBLIC protobuf::libprotobuf)
target_link_libraries(lbann PUBLIC ${HYDROGEN_LIBRARIES})
target_link_libraries(lbann PUBLIC ${OpenCV_LIBRARIES})
target_link_libraries(lbann PUBLIC ${CONDUIT_LIBRARIES})

if (LBANN_HAS_TBINF)
  target_link_libraries(lbann PUBLIC TBinf)
endif ()

if (LBANN_HAS_CNPY)
  target_link_libraries(lbann PUBLIC CNPY::CNPY)
endif ()

if (LBANN_TOPO_AWARE)
  target_link_libraries(lbann PUBLIC HWLOC::hwloc)
endif ()

if (LBANN_HAS_ALUMINUM)
  target_link_libraries(lbann PUBLIC ${Aluminum_LIBRARIES})
endif ()

if (LBANN_HAS_DISTCONV)
  target_link_libraries(lbann PUBLIC DISTCONV::DISTCONV)
  target_link_libraries(lbann PUBLIC P2P::P2P)
endif ()

if (LBANN_HAS_VTUNE)
  target_link_libraries(lbann PUBLIC ${VTUNE_STATIC_LIB})
endif ()

if (LBANN_HAS_PYTHON)
  target_link_libraries(lbann PUBLIC ${Python_LIBRARIES})
endif ()

if (TARGET LBANN_CXX_FLAGS_werror)
  target_link_libraries(lbann PUBLIC LBANN_CXX_FLAGS_werror)
endif ()

target_link_libraries(lbann PUBLIC ${DL_LIBRARY})

# Clean things up
include(LBANNDebugUtilities)
lbann_remove_default_include_paths_from_all_subtargets(lbann)
# This is to fix a bug with certain compilers interacting poorly with
# NVCC. In particular, the NVCC include line would have things like
# "-isystem=/usr/include" which would cause issues resolving STL
# include paths. Since compilers include "/usr/include" in their
# system include paths anyway (things searched by #include <...>), we
# can safely remove these from the explicit link line.

if (LBANN_DEBUG_PRINT_SUBTARGETS)
  lbann_print_all_subtargets(lbann)
endif ()

# Add the rest of the things
add_subdirectory(model_zoo)
add_subdirectory(model_zoo/tests)
add_subdirectory(model_zoo/jag_utils)
add_subdirectory(tests)
add_subdirectory(scripts)

################################################################
# Install LBANN
################################################################

include(CMakePackageConfigHelpers)

# Write the version file. This is independent of build/install tree.
write_basic_package_version_file(
  LBANNConfigVersion.cmake
  VERSION "${LBANN_VERSION}"
  COMPATIBILITY SameMajorVersion)

# This is for the build tree
set(INCLUDE_INSTALL_DIRS "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/include/lbann"
  "${CMAKE_BINARY_DIR}/include/lbann")
set(LIB_INSTALL_DIR "${CMAKE_BINARY_DIR}")
set(EXTRA_CMAKE_MODULE_DIR "${CMAKE_SOURCE_DIR}/cmake/modules")
configure_package_config_file(cmake/configure_files/LBANNConfig.cmake.in
  "${CMAKE_BINARY_DIR}/LBANNConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_BINARY_DIR}"
  PATH_VARS INCLUDE_INSTALL_DIRS LIB_INSTALL_DIR)

# Build tree export
export(EXPORT LBANNTargets NAMESPACE LBANN:: FILE LBANNTargets.cmake)

# Write the configure file for the install tree
set(INCLUDE_INSTALL_DIRS include)
set(LIB_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
set(CMAKE_INSTALL_DIR ${LIB_INSTALL_DIR}/cmake/lbann)
set(EXTRA_CMAKE_MODULE_DIR)
configure_package_config_file(cmake/configure_files/LBANNConfig.cmake.in
  "${CMAKE_BINARY_DIR}/LBANNConfig.cmake.install"
  INSTALL_DESTINATION "${CMAKE_INSTALL_DIR}"
  PATH_VARS INCLUDE_INSTALL_DIRS LIB_INSTALL_DIR)

# Install library
install(
  TARGETS lbann
  EXPORT LBANNTargets
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  )

# Install export
install(EXPORT LBANNTargets
  NAMESPACE LBANN::
  DESTINATION "${CMAKE_INSTALL_DIR}")

# Install the cmake stuff
install(FILES
  "${PROJECT_BINARY_DIR}/LBANNConfig.cmake.install"
  DESTINATION "${CMAKE_INSTALL_DIR}"
  RENAME "LBANNConfig.cmake")
install(FILES "${PROJECT_BINARY_DIR}/LBANNConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_DIR})
install(DIRECTORY cmake/modules
  DESTINATION "${CMAKE_INSTALL_DIR}"
  FILES_MATCHING PATTERN "*.cmake")

# Install header files
install(
  DIRECTORY "${PROJECT_SOURCE_DIR}/include/lbann"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
install(
  FILES "${PROJECT_BINARY_DIR}/lbann_config.hpp"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# Install Python frontend
# Note (tym): Python best practices are to put setup.py at the package
# root and setuptools only accepts relative paths. However, we need to
# insert a config file containing install-specific file paths and make
# sure setup.py can pick it up. I see three approaches for the build
# process:
#   1) Inject the config file into a known location in the source
#      directory so that setup.py can pick it up.
#   2) Copy the Python source tree into the build directory and insert
#      setup.py and the config file.
#   3) Create setup.py and the config file in the build directory and
#      pass the source directory as a relative path.
# We go for option 3 since it's simple and lightweight, but it runs
# counter to the intent of setuptools. If we learn about any nicer
# approaches, we should use them.
if (LBANN_HAS_PYTHON)

  # Construct config file
  # NOTE (trb): python_config.ini is installed by setup.py
  set(_PYTHON_CONFIG_INI ${CMAKE_BINARY_DIR}/python_config.ini)
  set(_LBANN_PB2_PY ${PYTHON_INSTALL_PREFIX}/${CMAKE_INSTALL_PYTHONDIR}/lbann_pb2.py)
  set(_LBANN_EXE ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/lbann)
  configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/configure_files/python_config.ini.in"
    "${_PYTHON_CONFIG_INI}"
    @ONLY)

  # Construct setup.py
  set(_SETUP_PY ${CMAKE_BINARY_DIR}/setup.py)
  set(_LBANN_PYTHON_DIR "${CMAKE_SOURCE_DIR}/python")
  configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/configure_files/setup.py.in"
    "${_SETUP_PY}"
    @ONLY)

  # Install Python package with setuptools
  set(_PY_INSTALL_DIR "${PYTHON_INSTALL_PREFIX}/${CMAKE_INSTALL_PYTHONDIR}")
  set(_SETUP_PY_ARGS
    "${_SETUP_PY_ARGS} --root ${_PY_INSTALL_DIR} --install-lib . --install-data .")
  install(CODE
    "execute_process(COMMAND ${Python_EXECUTABLE} ${_SETUP_PY} install ${_SETUP_PY_ARGS})")

  set(_PY_INSTALL_MSG
    "
\n**********************************************************************

A Python package has been installed to ${_PY_INSTALL_DIR}. To use
this package, be sure to add this directory to your PYTHONPATH, e.g.:

  export PYTHONPATH=${_PY_INSTALL_DIR}:\\$\{PYTHONPATH\}

**********************************************************************\n
")
  install(CODE
    "execute_process(COMMAND ${CMAKE_COMMAND} -E echo \"${_PY_INSTALL_MSG}\")")

endif (LBANN_HAS_PYTHON)

# Install contributor list, license, readme
install(
  FILES "${PROJECT_SOURCE_DIR}/CONTRIBUTORS"
        "${PROJECT_SOURCE_DIR}/LICENSE"
        "${PROJECT_SOURCE_DIR}/README.md"
  DESTINATION "${CMAKE_INSTALL_DOCDIR}")

################################################################
# Configuration summary
################################################################

# This creates a formatted string that contains a list of variables,
# one per line, with their values interpreted as TRUE or FALSE. The
# purpose is to provide uniform output, rather than an odd mixture of
# "1", "0", "ON", "OFF", "TRUE" and "FALSE".
macro(append_str_tf STRING_VAR)
  lbann_get_max_str_length(_max_length ${ARGN})
  math(EXPR _max_length "${_max_length} + 2")

  foreach(var ${ARGN})
    string(LENGTH "${var}" _var_length)
    math(EXPR _num_spaces "${_max_length} - ${_var_length}")
    lbann_get_space_string(_spaces ${_num_spaces})
    if (${var})
      set(${var} "TRUE")
      string(APPEND ${STRING_VAR} "  ${var}:" "${_spaces}" "TRUE\n")
    else ()
      set(${var} "FALSE")
      string(APPEND ${STRING_VAR} "  ${var}:" "${_spaces}" "FALSE\n")
    endif ()
  endforeach()
endmacro ()

# NOTE: message() outputs to stderr by default. We now use a string to
# maintain this information and then have cmake echo it to stdout. The
# only side effects are that if you use the CMake GUI, you won't see
# this output anymore (they only report stderr) and that if you add
# something to the list, you must remember your newline!
set(_str "\n== LBANN Configuration Summary ==\n\n")
string(APPEND _str "  PROJECT_SOURCE_DIR:   ${PROJECT_SOURCE_DIR}\n"
  "  PROJECT_BINARY_DIR:   ${PROJECT_BINARY_DIR}\n\n"
  "  CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}\n"
  "  CMAKE_BUILD_TYPE:     ${CMAKE_BUILD_TYPE}\n\n")
if (CMAKE_BUILD_TYPE MATCHES None)
  string(APPEND _str
    "  CXX FLAGS:            ${CMAKE_CXX_FLAGS}\n")
elseif (CMAKE_BUILD_TYPE MATCHES Release)
  string(APPEND _str
    "  CXX FLAGS:            ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}\n")
elseif (CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
  string(APPEND _str
    "  CXX FLAGS:            ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}\n")
elseif (CMAKE_BUILD_TYPE MATCHES Debug)
  string(APPEND _str
    "  CXX FLAGS:            ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}\n")
endif ()
string(APPEND _str "\n")

#Print the true/false guys
append_str_tf(_str
  LBANN_GNU_LINUX
  LBANN_HAS_HYDROGEN
  LBANN_HAS_OPENCV
  LBANN_HAS_CEREAL
  LBANN_HAS_CUDA
  LBANN_HAS_CUDNN
  LBANN_HAS_NCCL2
  LBANN_HAS_PROTOBUF
  LBANN_HAS_CNPY
  LBANN_HAS_TBINF
  LBANN_HAS_VTUNE
  LBANN_NVPROF
  LBANN_HAS_DOXYGEN
  LBANN_HAS_LBANN_PROTO
  LBANN_HAS_ALUMINUM
  LBANN_HAS_PYTHON
  LBANN_HAS_DISTCONV)
string(APPEND _str
  "\n== End LBANN Configuration Summary ==\n")

# Output to stdout
execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${_str}")
set(_str)

#
# Write a basic modulefile
#
set(LBANN_MODULEFILE_NAME "lbann-${LBANN_VERSION}.lua"
  CACHE STRING
  "The name of the LBANN modulefile to install. Must end in .lua.")

if (NOT (LBANN_MODULEFILE_NAME MATCHES ".+\.lua"))
  message(WARNING
    "LBANN_MODULEFILE_NAME must have extension \".lua\". Appending.")
  set(LBANN_MODULEFILE_NAME "${LBANN_MODULEFILE_NAME}.lua"
    CACHE STRING "" FORCE)
endif ()

configure_file(
  "${CMAKE_SOURCE_DIR}/cmake/configure_files/lbann_module.lua.in"
  "${CMAKE_BINARY_DIR}/lbann_module.lua.install"
  @ONLY)
install(FILES "${CMAKE_BINARY_DIR}/lbann_module.lua.install"
  RENAME "${LBANN_MODULEFILE_NAME}"
  DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/modulefiles")
