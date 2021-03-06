# macro for building ROOT dictionaries
#
# USAGE:
# build_dictionary( [<dictionary_name>]
#                   [COMPILE_FLAGS <flags>]
#                   [CLASSES_DEF_XML <filepath>]
#                   [CLASSES_H <filepath>]
#                   [DICT_NAME_VAR <var>]
#                   [DICTIONARY_LIBRARIES <library list>]
#                   [USE_PRODUCT_NAME]
#                   [NO_INSTALL]
#                   [DICT_FUNCTIONS]
#                   [NO_CHECK_CLASS_VERSION]
#                   [CCV_ENVIRONMENT <env_list>]
#                   [REQUIRED_DICTIONARIES <dictionary_list>]
#                   [RECURSIVE|NO_RECURSIVE]
#                   [UPDATE_IN_PLACE]
#                 )
#
# * <dictionary_name> defaults to a name based on the current source
# code subdirectory.
#
# * ${REFLEX} is always appended to the library list (even if it is
# empty).
#
# * Specify NO_INSTALL when building a dictionary for tests.
#
# * The default behavior is to generate a dictionary for data only. Use
# the DICT_FUNCTIONS option to reactivate the generation of dictionary
# entries for functions.
#
# * CLASSES_DEF_XML and CLASSES_H are optional and if not specified, we
# use classes_def.xml and classes.h respectively from the current source
# directory.
#
# * If DICT_NAME_VAR is specified, <var> will be set to contain the
# dictionary name.
#
# * check_class_version() is run by default. Use NO_CHECK_CLASS_VERSION
# to disable this behavior. CCV_ENVIRONMENT (as ENVIRONMENT),
# REQUIRED_DICTIONARIES, UPDATE_IN_PLACE, and {NO_,}RECURSIVE are passed
# through to check_class_version.
#
# * Any other macros or functions in this file are for internal use
# only.
#
########################################################################
include(CMakeParseArguments)
include(CetCurrentSubdir)
include(CheckClassVersion)

# make sure ROOT_VERSION has been defined
if( NOT ROOT_VERSION )
  message(FATAL_ERROR "build_dictionary: ROOT_VERSION is undefined")
elseif(NOT (HAVE_ROOT6 OR HAVE_ROOT5))
  message(FATAL_ERROR "build_dictionary: missing ROOT classification variables.")
endif()

if (HAVE_ROOT6)
  set(BD_WANT_ROOTMAP TRUE)
  set(BD_WANT_PCM TRUE)
  set(GENREFLEX_FLAGS
    --fail_on_warnings
    )
  if (ROOT6_HAS_NOINCLUDEPATHS)
    list(APPEND GENREFLEX_FLAGS
      --noIncludePaths
      )
  endif()
else() # ROOT5
  set(BD_WANT_CAP_FILE TRUE)
  set( GENREFLEX_FLAGS
    --fail_on_warnings
    --iocomments
    --gccxmlopt=--gccxml-compiler
    --gccxmlopt=$ENV{GCC_FQ_DIR}/bin/g++
    -D_REENTRANT
    -DGNU_SOURCE
    -DGNU_GCC
    -D__STRICT_ANSI__
    -DPROJECT_NAME="${PROJECT_NAME}"
    -DPROJECT_VERSION="${version}"
    )
endif()

macro( _set_dictionary_name )
   # base name on current subdirectory
   _cet_current_subdir( CURRENT_SUBDIR2 )
   # remove leading /
   STRING( REGEX REPLACE "^/(.*)" "\\1" CURRENT_SUBDIR "${CURRENT_SUBDIR2}" )
   # replace remaining slashes with underscores
   STRING( REGEX REPLACE "/" "_" dictname "${CURRENT_SUBDIR}" )
endmacro( _set_dictionary_name )

function( _generate_dictionary dictname CLASSES_DEF_XML CLASSES_H)
  cmake_parse_arguments(GD "DICT_FUNCTIONS" "ROOTMAP_OUTPUT;PCM_OUTPUT_VAR" "" ${ARGN})
  set(generate_dictionary_usage "_generate_dictionary( [DICT_FUNCTIONS] [dictionary_name] )")
  #message(STATUS "calling generate_dictionary with ${ARGC} arguments: ${ARGV}")
  if (NOT HAVE_ROOT6 AND NOT GD_DICT_FUNCTIONS AND NOT CET_DICT_FUNCTIONS)
    set(GENREFLEX_FLAGS ${GENREFLEX_FLAGS} --dataonly)
  endif()
  #message(STATUS "_GENERATE_DICTIONARY: generate dictionary source code for ${dictname}")
  get_directory_property( genpath INCLUDE_DIRECTORIES )
  foreach( inc ${genpath} )
      set( GENREFLEX_INCLUDES ${GENREFLEX_INCLUDES} -I${inc} )
  endforeach(inc)
  # add any local compile definitions
  get_directory_property(compile_defs COMPILE_DEFINITIONS)
  foreach( def ${compile_defs} )
      set( GENREFLEX_FLAGS ${GENREFLEX_FLAGS} -D${def} )
  endforeach(def)
  #message(STATUS "_GENERATE_DICTIONARY: using genreflex flags ${GENREFLEX_FLAGS} ")
  #message(STATUS "_GENERATE_DICTIONARY: using genreflex cleanup ${GENREFLEX_CLEANUP} ")
  if (GENREFLEX_CLEANUP)
    set(CLEANUP_COMMAND  || { rm -f ${dictname}_dict.cpp ${dictname}_map.cpp "\;" /bin/false "\;" })
  endif()

  if (HAVE_ROOT6)
    list(APPEND GENREFLEX_FLAGS
      -l $<TARGET_PROPERTY:${dictname}_dict,LIBRARY_OUTPUT_DIRECTORY>/${CMAKE_SHARED_LIBRARY_PREFIX}${dictname}_dict${CMAKE_SHARED_LIBRARY_SUFFIX}
      )
  endif()
  if (GD_ROOTMAP_OUTPUT)
    list(APPEND GENREFLEX_FLAGS
      --rootmap-lib=${CMAKE_SHARED_LIBRARY_PREFIX}${dictname}_dict${CMAKE_SHARED_LIBRARY_SUFFIX}
      --rootmap=${GD_ROOTMAP_OUTPUT}
      )
  endif()
  if (BD_WANT_CAP_FILE)
    set(SOURCE_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${dictname}_map.cpp)
    list(APPEND GENREFLEX_FLAGS
      --capabilities=${SOURCE_OUTPUT}
      )
  endif()
  if (BD_WANT_PCM)
    set(PCM_OUTPUT
      $<TARGET_PROPERTY:${dictname}_dict,LIBRARY_OUTPUT_DIRECTORY>/${CMAKE_SHARED_LIBRARY_PREFIX}${dictname}_dict_rdict.pcm)
    if (GD_PCM_OUTPUT_VAR)
      set(${GD_PCM_OUTPUT_VAR} ${PCM_OUTPUT} PARENT_SCOPE)
    endif()
  endif()
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${dictname}_dict.cpp
    # Extra outputs commented out until custom_command OUTPUT supports
    # generator flags. See
    # https://gitlab.kitware.com/cmake/cmake/issues/12877.
    ${SOURCE_OUTPUT} # ${GD_ROOTMAP_OUTPUT} ${PCM_OUTPUT}
    COMMAND ${ROOT_GENREFLEX} ${CLASSES_H}
    -s ${CLASSES_DEF_XML}
    -I${CMAKE_SOURCE_DIR}
    ${GENREFLEX_INCLUDES} ${GENREFLEX_FLAGS}
    -o ${dictname}_dict.cpp
    ${CLEANUP_COMMAND}
    IMPLICIT_DEPENDS CXX ${CLASSES_H}
    DEPENDS ${CLASSES_DEF_XML}
    COMMENT "Generating dictionary files for target ${dictname}"
    )
  # set variable for install_source
  set(cet_generated_code
    ${CMAKE_CURRENT_BINARY_DIR}/${dictname}_dict.cpp
    ${SOURCE_OUTPUT}
    PARENT_SCOPE)
endfunction( _generate_dictionary )

# dictionaries are built in art with this
function ( build_dictionary )
  #message(STATUS "BUILD_DICTIONARY: called with ${ARGC} arguments: ${ARGV}")
  set(build_dictionary_usage "USAGE: build_dictionary( [dictionary_name] [DICTIONARY_LIBRARIES <library list>] [COMPILE_FLAGS <flags>] [DICT_NAME_VAR <var>] [NO_INSTALL] )")
  cmake_parse_arguments( BD
    "NOINSTALL;NO_INSTALL;DICT_FUNCTIONS;USE_PRODUCT_NAME;NO_CHECK_CLASS_VERSION;NO_DEFAULT_LIBRARIES;UPDATE_IN_PLACE;RECURSIVE;NO_RECURSIVE"
    "DICT_NAME_VAR;CLASSES_DEF_XML;CLASSES_H"
    "CCV_ENVIRONMENT;DICTIONARY_LIBRARIES;COMPILE_FLAGS;REQUIRED_DICTIONARIES" ${ARGN})
  #message(STATUS "BUILD_DICTIONARY: unparsed arguments: ${BD_UNPARSED_ARGUMENTS}")
  #message(STATUS "BUILD_DICTIONARY: install flag is  ${BD_NO_INSTALL} ")
  #message(STATUS "BUILD_DICTIONARY: COMPILE_FLAGS: ${BD_COMPILE_FLAGS}")
  if( BD_NOINSTALL )
    message( FATAL_ERROR "build_dictionary now requires NO_INSTALL, you have used the old NOINSTALL command")
  endif( BD_NOINSTALL )
  if( BD_UNPARSED_ARGUMENTS )
    list(LENGTH BD_UNPARSED_ARGUMENTS dlen)
    if(dlen GREATER 1 )
	    message(FATAL_ERROR  "build_dictionary: too many arguments. ${ARGV} \n ${build_dictionary_usage}")
    endif()
    list(GET BD_UNPARSED_ARGUMENTS 0 dictname)
    #message(STATUS "BUILD_DICTIONARY: have ${dlen}default arguments")
    #message(STATUS "BUILD_DICTIONARY: default arguments dictionary name: ${dictname}")
  else()
    #message(STATUS "BUILD_DICTIONARY: no default arguments, call _set_dictionary_name")
    _set_dictionary_name()
    if (BD_USE_PRODUCT_NAME)
      set( dictname ${product}_${dictname} )
    endif()
    #message(STATUS "BUILD_DICTIONARY debug: calculated dictionary name is ${dictname} for ${product}")
  endif()
  if (BD_DICT_NAME_VAR)
    set(${BD_DICT_NAME_VAR} ${dictname} PARENT_SCOPE)
  endif()
  if (NOT BD_CLASSES_DEF_XML)
    set(BD_CLASSES_DEF_XML ${CMAKE_CURRENT_SOURCE_DIR}/classes_def.xml)
  endif()
  if (NOT BD_CLASSES_H)
    set(BD_CLASSES_H ${CMAKE_CURRENT_SOURCE_DIR}/classes.h)
  endif()
  if(BD_DICTIONARY_LIBRARIES)
    # check library names and translate where necessary
    set(dictionary_liblist "")
    foreach (lib ${BD_DICTIONARY_LIBRARIES})
      string(REGEX MATCH [/] has_path "${lib}")
      if( has_path )
	      list(APPEND dictionary_liblist ${lib})
      else()
	      string(TOUPPER  ${lib} ${lib}_UC )
	      #_cet_debug_message( "simple_plugin: check ${lib}" )
	      if( ${${lib}_UC} )
          _cet_debug_message( "changing ${lib} to ${${${lib}_UC}}")
          list(APPEND dictionary_liblist ${${${lib}_UC}})
	      else()
          list(APPEND dictionary_liblist ${lib})
	      endif()
      endif( has_path )
    endforeach()
  endif()
  list(APPEND dictionary_liblist ${ROOT_CORE} ${ROOT_REFLEX})
  #message(STATUS "BUILD_DICTIONARY: building dictionary ${dictname}")
  #message(STATUS "BUILD_DICTIONARY: link dictionary ${dictname} with ${dictionary_liblist} ")
  add_library(${dictname}_dict SHARED ${CMAKE_CURRENT_BINARY_DIR}/${dictname}_dict.cpp )
  if (BD_WANT_ROOTMAP)
    set(ROOTMAP_OUTPUT
      $<TARGET_PROPERTY:${dictname}_dict,LIBRARY_OUTPUT_DIRECTORY>/${CMAKE_SHARED_LIBRARY_PREFIX}${dictname}_dict.rootmap)
    _generate_dictionary(${dictname} ${BD_CLASSES_DEF_XML} ${BD_CLASSES_H}
      ROOTMAP_OUTPUT ${ROOTMAP_OUTPUT}
      PCM_OUTPUT_VAR PCM_OUTPUT)
  else()
    _generate_dictionary(${dictname} ${BD_CLASSES_DEF_XML} ${BD_CLASSES_H}
      PCM_OUTPUT_VAR PCM_OUTPUT)
  endif()
  if (BD_WANT_ROOTMAP AND NOT ROOT6_HAS_NOINCLUDEPATHS)
    # Header line and OS X lib name fixing only necessary for older ROOT6.
    add_custom_command(TARGET ${dictname}_dict POST_BUILD
      COMMAND perl -wapi.bak -e s&\\.dylib\\.so&.dylib&g ${ROOTMAP_OUTPUT}
      COMMAND rm -f ${ROOTMAP_OUTPUT}.bak
      COMMENT Fixing shared library reference in ${ROOTMAP_OUTPUT}
      VERBATIM
      )
  endif()
  if (BD_COMPILE_FLAGS)
    set_target_properties(${dictname}_dict
      PROPERTIES COMPILE_FLAGS ${BD_COMPILE_FLAGS})
    if (BD_WANT_CAP_FILE)
      set_target_properties(${dictname}_map
        PROPERTIES COMPILE_FLAGS ${BD_COMPILE_FLAGS})
    endif()
  endif()
  target_link_libraries( ${dictname}_dict ${dictionary_liblist} )
  if (BD_WANT_CAP_FILE)
    add_library(${dictname}_map SHARED ${CMAKE_CURRENT_BINARY_DIR}/${dictname}_map.cpp )
    target_link_libraries( ${dictname}_map ${dictionary_liblist} )
    add_dependencies(${dictname}_map ${dictname}_dict)
  endif()
  if( NOT BD_NO_INSTALL )
    if (cet_generated_code) # Local scope, set by _generate_dictionary.
      set(cet_generated_code ${cet_generated_code} PARENT_SCOPE)
    endif()
    #message( STATUS "BUILD_DICTIONARY: installing ${dictname}_dict" )
    install ( TARGETS ${dictname}_dict DESTINATION ${${product}_lib_dir} )
    # add to library list for package configure file
    cet_add_to_library_list( ${dictname}_dict )
    if (BD_WANT_CAP_FILE)
      install ( TARGETS ${dictname}_map DESTINATION ${${product}_lib_dir} )
    endif()
    if (BD_WANT_ROOTMAP)
      install ( FILES ${ROOTMAP_OUTPUT} DESTINATION ${${product}_lib_dir} )
    endif()
    if (PCM_OUTPUT)
      install ( FILES ${PCM_OUTPUT} DESTINATION ${${product}_lib_dir} )
    endif()
  endif()
  if (NOT TARGET BuildDictionary_AllDicts)
    add_custom_target(BuildDictionary_AllDicts)
  endif()
  add_dependencies(BuildDictionary_AllDicts ${dictname}_dict)
  #message(STATUS "Calling check_class_version with args ${BD_ARGS}")
  if (BD_NO_CHECK_CLASS_VERSION OR NOT DEFINED CCV_DEFAULT_RECURSIVE)
    # Turned off manually, or we're using or building an older art.
    if (BD_UPDATE_IN_PLACE OR BD_REQUIRED_DICTIONARIES OR
        BD_RECURSIVE OR BD_NO_RECURSIVE OR BD_CCV_ENVIRONMENT)
      message(WARNING "BuildDictionary: NO_CHECK_CLASS_VERSION is set: CCV_ENVIRONMENT, UPDATE_IN_PLACE, REQUIRED_DICTIONARIES, RECURSIVE AND NO_RECURSIVE are ignored.")
    endif()
  else ()
    if(BD_UPDATE_IN_PLACE)
      message(WARNING "BuildDictionary: UPDATE_IN_PLACE is ignored as we always invoke check_class_version this way.")
    endif()
    if (BD_REQUIRED_DICTIONARIES)
      set(BD_CCV_ARGS ${BD_CCV_ARGS} REQUIRED_DICTIONARIES ${BD_REQUIRED_DICTIONARIES})
    endif()
    if (BD_CLASSES_DEF_XML)
      set(BD_CCV_ARGS ${BD_CCV_ARGS} CLASSES_DEF_XML ${BD_CLASSES_DEF_XML})
    endif()
    if (BD_RECURSIVE)
      set(BD_CCV_ARGS ${BD_CCV_ARGS} RECURSIVE)
    elsif (BD_NO_RECURSIVE)
      set(BD_CCV_ARGS ${BD_CCV_ARGS} NO_RECURSIVE)
    endif()
    if (BD_CCV_ENVIRONMENT)
      list(APPEND BD_CCV_ARGS ENVIRONMENT ${BD_CCV_ENVIRONMENT})
    endif()
    check_class_version(${BD_LIBRARIES} UPDATE_IN_PLACE ${BD_CCV_ARGS})
  endif()
endfunction ( build_dictionary )
