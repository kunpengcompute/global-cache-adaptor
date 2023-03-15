set(EXECUTABLE_OUTPUT_PATH ${BASE_DIR}/build/bin)
#add_subdirectory(client_adaptor)
if (NOT DEFINED PROXY_ONLY)
add_subdirectory(ceph_proxy)
add_subdirectory(server_adaptor)
else()
add_subdirectory(ceph_proxy)
endif()

set(INSTALL_BASE_DIR "")
set(INSTALL_INCLUDE_DIR include)
set(INSTALL_LIBRARY_DIR lib)
set(INSTALL_ARCHIVE_DIR lib)
set(INSTALL_OBJECT_DIR lib)
set(INSTALL_RUNTIME_DIR bin)
set(INSTALL_CONFIG_DIR lib)
set(CMAKE_SKIP_RPATH true)
install(TARGETS
    ${INSTALL_TARGETS}
    RUNTIME DESTINATION ${INSTALL_RUNTIME_DIR}
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}
    ARCHIVE DESTINATION ${INSTALL_ARCHIVE_DIR}
)

include_directories(${CEPH_INCLUDE_DIR})
get_property(dirs DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
message(STATUS "Client Adaptor include directory = ${dirs}")
