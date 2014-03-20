
set(FIND_ES_LIB_SUFFIX "")
if(ES_STATIC_LIBRARY)
    set(FIND_ES_LIB_SUFFIX "-s")
endif()

find_path(ES_INCLUDE_DIRS es/component.hpp
    PATH_SUFFIXES include
    PATHS
        $ENV{BUILD_ROOT}
        /usr/local
        /usr
        ~/Library
        /Library
        /opt/local
        /opt
        )

find_library(ES_LIBRARIES
    NAMES libes${FIND_ES_LIB_SUFFIX} es${FIND_ES_LIB_SUFFIX}
    PATH_SUFFIXES lib64 lib
    PATHS
        $ENV{BUILD_ROOT}
        /usr/local
        /usr
        ~/Library
        /Library
        /opt/local
        /opt
        )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ES DEFAULT_MSG
                                  ES_LIBRARIES ES_INCLUDE_DIRS)

