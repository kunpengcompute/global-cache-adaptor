cmake_minimum_required(VERSION 3.14.1)
project(ADAPTOR)

set(BASE_DIR_UP ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
if (NOT DEFINED CEPH_DIR)
	set(CEPH_DIR ${PROJECT_SOURCE_DIR}/../../ceph-14.2.8)
endif()
if (NOT DEFINED GLOBAL_CACHE_DIR)
        set(GLOBAL_CACHE_DIR ${PROJECT_SOURCE_DIR}/../global_cache)
endif()
set(GLOBAL_CACHE_INFRAS_LIB_DIR ${GLOBAL_CACHE_DIR}/lib)
message(STATUS "CEPH_DIR: " ${CEPH_DIR})
message(STATUS "GLOBAL_CACHE_DIR: " ${GLOBAL_CACHE_DIR})
message(STATUS "GLOBAL_CACHE_INFRAS_LIB_DIR: " ${GLOBAL_CACHE_INFRAS_LIB_DIR})

include_directories(
    ${CEPH_DIR}/src/
    ${CEPH_DIR}/src/boost/
    ${CEPH_DIR}/src/include/
    ${CEPH_DIR}/build/include/
    ${CEPH_DIR}/build/src/include/
    ${BASE_DIR}/src/dependency/include/
)
get_property(dirs DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
message(STATUS "adaptor include directory = ${dirs}")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE DEBUG)
endif()
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

if(NOT DEFINED USE_ASAN)
    set(USE_ASAN False)
endif()
message(STATUS "USE_ASAN: ${USE_ASAN}")


if (${CMAKE_BUILD_TYPE} STREQUAL "RELEASE")
    set(COMPILE_FLAGS "-Wl,-z,relro,-z,now -s -fstack-protector-all -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D__linux__ -Wall -Wtype-limits -Wignored-qualifiers -Winit-self -Wpointer-arith -fno-strict-aliasing -fsigned-char -Wno-unknown-pragmas -rdynamic  -ftemplate-depth-1024 -Wnon-virtual-dtor -Wno-unknown-pragmas -Wno-ignored-qualifiers -Wstrict-null-sentinel -Woverloaded-virtual -fno-new-ttp-matching -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fdiagnostics-color=auto -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free -O2 -DNDEBUG -fPIE -DHAVE_CONFIG_H -D__CEPH__ -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -std=c++1z -ldl -lrt -lresolv -lpthread")
else()
    set(COMPILE_FLAGS "-Wl,-z,relro,-z,now -ftrapv -fstack-protector-all -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D__linux__ -Wall -Wtype-limits -Wignored-qualifiers -Winit-self -Wpointer-arith -fno-strict-aliasing -fsigned-char -Wno-unknown-pragmas -rdynamic -ftemplate-depth-1024 -Wnon-virtual-dtor -Wno-ignored-qualifiers -Wstrict-null-sentinel -Woverloaded-virtual -fno-new-ttp-matching -fstack-protector-strong -fdiagnostics-color=auto -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free -g -O0 -fPIE -DHAVE_CONFIG_H -D__CEPH__ -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -std=c++1z ")
endif()
set(CMAKE_CXX_FLAGS "${COMPILE_FLAGS}")

if(NOT DEFINED UT)
    set(UT False)
endif()
message(STATUS "UT: ${UT}")

if(NOT DEFINED CLASS_PATH)
    set(CLASS_PATH "/opt/gcache/lib")
endif()
message(STATUS "CLASS_PATH: ${CLASS_PATH}")

if(NOT DEFINED CPU_TYPE)
    set(CPU_TYPE "arm_64")
endif()
message(STATUS "CPU_TYPE: ${CPU_TYPE}")

#设置默认的so输出路径
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BASE_DIR}/build/lib)#.a
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BASE_DIR}/build/lib)#.so

set(GLOBAL_CACHE_ADAPTOR_LIBS sa)

set(PROXY_LIBS proxy)
#set(PROXY_LIBS ceph_proxy_stub)

set(CCM_LIBS -lccm_lib)
set(PERSISTENCE_LIBS -lstream -lbdm -lplog)
set(VERBS_LIBS )
set(COMMON_LIBS -lpthread -ldl -lm)
set(OPENSSL_LIBS -lzookeeper_mt)
set(INFRASTRUCTURE_LIBS  -ldplog -ldpumm_mm  -ldpumm_cmm -ldposen -ldposax -lmxml -ldpdiagnose -lftdsclient -losax_util)
set(PLOG_SDK_LIBS -ldplog -ldpumm_cmm -ldpumm_mm  -ldposax -lmxml -ldpdiagnose -lftdsclient -losax_util)
set(INDEX_LIBS -lart -lart_repair -lkvs -lmessage -lpelagodb)
set(INDEX_STUB_LIBS -lindex_ccdb_client_stub -lkvs_ctrl_stub)

set(CACHE_LIBS -lcache)
set(CACHE_DEP_LIBS
	 -ldl -liod -ldpdiagnose -lsecurec -lpatmatch -lftdsclient -ldposax -losax_util
	 -ldpumm_cmm -ldpumm_mm -ldptracepoint -llwt -ldplog -ldif -lmxml -lupf
)
set(EXTRA_LIBS -laio -lstdc++ -lupf -ldif -llz4 -llwt -lscpart_mgr)
set(CLI_LIBS -lcli_server_usr)
set(CONF_LIBS -lconfparser)
if(${UT})
    #设置编译ut时 so和bin文件的输出路径
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BASE_DIR}/build/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BASE_DIR}/build/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BASE_DIR}/build/ut/cache)
    add_subdirectory(src)
    add_subdirectory(test)
else()
message(STATUS "BASE DIRECTORY: ${BASE_DIR}")
message(STATUS "LIBRARY DIRECTORY: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

# 引入子目录
add_subdirectory(src)
endif()

