# ======================================================================
#  art_root_io main build file
#
#  cd .../path/to/build/directory
#  source .../path/to/art_root_io/ups/setup_for_development
#  cmake [-DCMAKE_INSTALL_PREFIX=/install/path]
#        -DCMAKE_BUILD_TYPE=$CETPKG_TYPE
#        $CETPKG_SOURCE
#  make
#  make test
#  make install
#  make package (builds distribution tarfile)
# ======================================================================

cmake_minimum_required(VERSION 3.12...3.14 FATAL_ERROR)
project(art_root_io LANGUAGES CXX C)

# cetbuildtools contains our cmake modules
find_package(cetbuildtools REQUIRED)

include(CetCMakeEnv)
cet_cmake_env()

cet_set_compiler_flags(DIAGS VIGILANT
  WERROR
  NO_UNDEFINED
  EXTRA_FLAGS -pedantic
  EXTRA_CXX_FLAGS -Wnon-virtual-dtor -Wdelete-non-virtual-dtor -DBOOST_TEST_NO_OLD_TOOLS)

cet_have_qual(sse2 SSE2)
if (SSE2)
  cet_add_compiler_flags(CXX -msse2 -ftree-vectorizer-verbose=2)
endif()

if($ENV{CET_SANITIZE_ADDRESS})
  if(NOT "$ENV{GCC_FQ_DIR}" STREQUAL "")
    find_file(ASAN_LIB libasan.so NO_DEFAULT_PATH PATHS $ENV{GCC_FQ_DIR}/lib64)
  endif()
endif()

if($ENV{CET_SANITIZE_THREAD})
  if(NOT "$ENV{GCC_FQ_DIR}" STREQUAL "")
    find_file(TSAN_LIB libtsan.so NO_DEFAULT_PATH PATHS $ENV{GCC_FQ_DIR}/lib64)
  endif()
endif()

if ($ENV{CET_SANITIZE_ADDRESS})
cet_add_compiler_flags(CXX -fsanitize=address)
cet_add_compiler_flags(C -fsanitize=address)
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address")
set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} -fsanitize=address")
set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=address")
endif()

if ($ENV{CET_SANITIZE_THREAD})
cet_add_compiler_flags(CXX -fsanitize=thread -D__SANITIZE_THREAD__)
cet_add_compiler_flags(C -fsanitize=thread -D__SANITIZE_THREAD__)
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=thread -D__SANITIZE_THREAD__")
set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} -fsanitize=thread -D__SANITIZE_THREAD__")
set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=thread -D__SANITIZE_THREAD__")
endif()

cet_report_compiler_flags()

# these are minimum required versions, not the actual product versions
find_ups_product(art)
find_ups_product(canvas_root_io)
find_ups_product(canvas)
find_ups_product(messagefacility)
find_ups_product(fhiclcpp)
find_ups_product(cetlib)
find_ups_product(cetlib_except)
find_ups_product(clhep)
find_ups_product(hep_concurrency)
find_ups_product(sqlite)
find_ups_boost(BOOST_TARGETS)
find_ups_root()
find_ups_product(catch)

# SQLite
cet_find_library(SQLITE3 NAMES sqlite3_ups PATHS ENV SQLITE_LIB NO_DEFAULT_PATH)

# macros for making art libraries/dictionaries, etc. and plugins
include(ArtMake)
include(BuildPlugins)

# source
add_subdirectory(art_root_io)

# ups - table and config files
add_subdirectory(ups)

# tools - migration scripts
add_subdirectory(tools)

# packaging utility
include(UseCPack)
