set(lib_ceph_origin_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../../../ceph)
set(lib_rados_include_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../../../ceph/src/include)
set(lib_rbd_include_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../../../ceph/src/include)

set(lib_dpax_include_dir ${GLOBAL_CACHE_DIR}/src/include/infras/infrastructure/osax)
set(lib_lvos_include_idr ${GLOBAL_CACHE_DIR}/src/include/infras/infrastructure/lvos)
set(lib_ftds_include_dir ${GLOBAL_CACHE_DIR}/src/include/infras/ftds)
set(lib_dplog_include_dir ${GLOBAL_CACHE_DIR}/src/include/infras/infrastructure/log)
set(lib_confparser_include_dir ${GLOBAL_CACHE_DIR}/src/include/infras/infrastructure/conf_parser)
set(lib_dpax_redef_include_dir ${GLOBAL_CACHE_DIR}/src/include/infras)
set(third_part_dir /usr/local/gcache_adaptor)
set(CMAKE_SKIP_RPATH true)
set(ceph_proxy_srcs CephProxyInterface.cc
                    CephProxy.cc
                    CephProxyOp.cc
                    RadosWorker.cc
                    RadosWrapper.cc
                    ConfigRead.cc
                    RadosMonitor.cc
                    CephProxyFtds.cc
                    CcmAdaptor.cc
                    RbdWrapper.cc
                    CephMsgr.cc
                    CephProxyLog.h)

if (NOT RADOS_DIR)
    set(RADOS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../ceph-14.2.8)
endif()
message("RADOS_DIR=${RADOS_DIR}")
find_library(RADOS_LIB
   NAMES
   librados.so.2
   PATHS
   ${RADOS_DIR}/build/lib
   NO_DEFAULT_PATH
)

if (NOT RBD_DIR)
    set(RBD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../ceph-14.2.8)
endif()
message("RBD_DIR=${RBD_DIR}")
find_library(RBD_LIB
   NAMES
   librbd.so.1
   PATHS
   ${RBD_DIR}/build/lib
   NO_DEFAULT_PATH
)

find_library(CEPH_COMMON_LIB
  NAMES
  libceph-common.so.0
  PATHS
  ${RADOS_DIR}/build/lib
)

find_library(FTDS_LIB
   NAMES
   libftdsclient.so
   PATHS
   ${GLOBAL_CACHE_DIR}/lib
   ${third_part_dir}/lib
)

find_library(DPLOG_LIB
   NAMES
   libdplog.so
   PATHS
   ${GLOBAL_CACHE_DIR}/lib
   ${third_part_dir}/lib
)

find_library(CONFPARSER_LIB
    NAMES
    libconfparser.so
    PATHS
    ${GLOBAL_CACHE_DIR}/lib
    ${third_part_dir}/lib
)

find_library(DPOSAX_LIB
   NAMES
   libdposax.so
   PATHS
   ${GLOBAL_CACHE_DIR}/lib
   ${third_part_dir}/lib
)

add_library(proxy SHARED ${ceph_proxy_srcs})
if (${CMAKE_BUILD_TYPE} STREQUAL "RELEASE")
    message(STATUS "Proxy cmake this is Release")
    target_compile_options(proxy PUBLIC -std=c++17 -g -DDPAX_LOG
                                         -fstack-protector-all
                                         -fvisibility=hidden
                                         -fno-omit-frame-pointer
                                         -fPIC
                                         -DFTDS_ON
                                         -D_FORTIFY_SOURCE=2 -O2)

    target_link_options(proxy PUBLIC -Wl,-z,relro,-z,now
                                      -Wl,-z,noexecstack
                                      -pie)
else()
    message(STATUS "Proxy cmake this is Debug")
    
if (${USE_ASAN})
    message(STATUS "Proxy USE_ASAN")
    target_compile_options(proxy PRIVATE -g -fsanitize=address -fsanitize=leak -fno-omit-frame-pointer -fno-common)
    target_link_options(proxy PRIVATE -g -fsanitize=address -fsanitize=leak -fno-omit-frame-pointer -fno-common)
endif()

    target_compile_options(proxy PRIVATE -std=c++17 -g -ftrapv -DDPAX_LOG -DFTDS_ON)
endif()

message("proxy CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")

if (${WITH_TESTS_ONLY})
    add_definitions(-DWITH_TESTS_ONLY)
    message("proxy WITH TESTS")
endif()
 
target_include_directories(proxy PUBLIC .
                                        ${lib_ceph_origin_dir}
                                        ${lib_rados_include_dir}
                                        ${lib_rbd_include_dir}
                                        ${lib_dpax_include_dir}
                                        ${lib_lvos_include_idr}
                                        ${lib_ftds_include_dir}
                                        ${lib_dplog_include_dir}
                                        ${lib_confparser_include_dir}
                                        ${lib_dpax_redef_include_dir})

target_link_libraries(proxy ${CEPH_COMMON_LIB} ${RADOS_LIB} ${RBD_LIB} ${FTDS_LIB} ${DPLOG_LIB} ${DPOSAX_LIB} ${CONFPARSER_LIB} -lpthread)
