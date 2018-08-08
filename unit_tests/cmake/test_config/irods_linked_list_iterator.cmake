set(IRODS_TEST_TARGET irods_linked_list_iterator)

set(IRODS_TEST_SOURCE_FILES main.cpp
                            test_irods_linked_list_iterator.cpp)

set(IRODS_TEST_INCLUDE_PATH ${CMAKE_SOURCE_DIR}/lib/api/include
                            ${CMAKE_BINARY_DIR}/lib/core/include
                            ${CMAKE_SOURCE_DIR}/lib/core/include
                            ${CMAKE_SOURCE_DIR}/server/core/include
                            ${CMAKE_SOURCE_DIR}/server/drivers/include
                            ${CMAKE_SOURCE_DIR}/server/re/include
                            ${IRODS_EXTERNALS_FULLPATH_CATCH2}/include
                            ${IRODS_EXTERNALS_FULLPATH_BOOST}/include
                            ${IRODS_EXTERNALS_FULLPATH_JANSSON}/include)
 
set(IRODS_TEST_LINK_LIBRARIES irods_server
                              c++abi)