# determine the system flavor and define flavorqual_dir
#
# set_flavor_qual( [arch] )
#   sets flavorqual_dir. Default is "", i.e. install everything directly
#    under CMAKE_INSTALL_PREFIX
#   May add logic here to have platform-dependent binary installation subdirectories
#
# process_ups_files()
#   noop - no longer used

macro( set_flavor_qual )

message(STATUS "Building for ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}" )

SET (flavorqual_dir "" CACHE STRING "(Flavor-qualified) installation root directory" FORCE)

#message(STATUS "set_flavor_qual: flavorqual directory is ${flavorqual_dir}" )
endmacro( set_flavor_qual )

macro( process_ups_files )
endmacro( process_ups_files )
