cmake_minimum_required(VERSION 3.14)

message("Start server adaptor cmake job.")
set(OSA osa)
set(INSTALL_LIBDIR "lib64")
set(CMAKE_CXX_FLAGS " ")
if (${CMAKE_BUILD_TYPE} STREQUAL "RELEASE")
    message("OpenServerAdaptor cmake This is RELEASE")
    set(COMPILE_FLAGS "-Wl,-z,relro,-z,now -s -fstack-protector-all -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D__linux__ -Wall -Wtype-limits -Wignored-qualifiers -Winit-self -Wpointer-arith -fno-strict-aliasing -fsigned-char -Wno-unknown-pragmas -rdynamic  -ftemplate-depth-1024 -Wnon-virtual-dtor -Wno-unknown-pragmas -Wno-ignored-qualifiers -Wstrict-null-sentinel -Woverloaded-virtual -fno-new-ttp-matching -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fdiagnostics-color=auto -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free -O2 -DNDEBUG -fPIE -DHAVE_CONFIG_H -D__CEPH__ -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -std=c++1z -ldl -lrt -lresolv -lpthread -fvisibility=hidden")
else()
    message("OpenServerAdaptor cmake This is DEBUG.")
    set(COMPILE_FLAGS "-Wl,-z,relro,-z,now -fstack-protector-all -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D__linux__ -Wall -Wtype-limits -Wignored-qualifiers -Winit-self -Wpointer-arith -fno-strict-aliasing -fsigned-char -Wno-unknown-pragmas -rdynamic  -ftemplate-depth-1024 -Wnon-virtual-dtor -Wno-unknown-pragmas -Wno-ignored-qualifiers -Wstrict-null-sentinel -Woverloaded-virtual -fno-new-ttp-matching -U_FORTIFY_SOURCE  -fstack-protector-strong -fdiagnostics-color=auto -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free -O0 -g -fPIE -DHAVE_CONFIG_H -D__CEPH__ -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -std=c++1z -ldl -lrt -lresolv -lpthread")
endif()

if(NOT DEFINED FTDS)
    set(FTDS TRUE)
endif()
if(${FTDS})
    message("FTDS ON")
    set(CMAKE_C_FLAGS -DFTDS_ON)
    set(CMAKE_CXX_FLAGS -DFTDS_ON)
endif()

if (${USE_ASAN})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=leak -fno-omit-frame-pointer -fno-common")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILE_FLAGS}")
message("OpenServerAdaptor CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")

if (NOT CEPH_DIR)
    set(CEPH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../ceph-14.2.8)
endif()
option(SA_PERF "SA PERF" OFF)
if (SA_PERF)
    message("OpenServerAdaptor define SA_PERF.")
    add_definitions(-DSA_PERF)
endif()
message("CEPH_DIR=${CEPH_DIR}")
find_library(CEPH_COMMON_LIB
  NAMES
  libceph-common.so.0
  PATHS
  ${CEPH_DIR}/build/lib
)
find_library(GLOBAL
  NAMES
  libglobal.a
  PATHS
  ${CEPH_DIR}/build/lib
)
find_library(ERASURE_CODE
  NAMES
  liberasure_code.a
  PATHS
  ${CEPH_DIR}/build/lib
)

aux_source_directory(. SRCS_LIST_OSA)

add_library( ${OSA}
  SHARED
  ${SRCS_LIST_OSA} )

if (NOT GLOBAL_CACHE_DIR)
    set(GLOBAL_CACHE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../global_cache)
endif()
message("GLOBAL_CACHE_DIR=${GLOBAL_CACHE_DIR}")

find_library(CONFPARSER_LIB
    NAMES
    libconfparser.so
    PATHS
    ${GLOBAL_CACHE_DIR}/build/my_pkg_root/lib
)

target_include_directories(${OSA}
  PUBLIC
  .
  ${CEPH_DIR}/src
  ${CEPH_DIR}/build/include
)

target_compile_definitions(${OSA} PUBLIC -DCLASS_PATH="${CLASS_PATH}")

target_link_libraries(${OSA}
  ${CEPH_COMMON_LIB}
  ${GLOBAL}
  ${ERASURE_CODE}
  ${CONFPARSER_LIB}
)

install(TARGETS ${OSA} LIBRARY DESTINATION ${INSTALL_LIBDIR})

