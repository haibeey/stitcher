cmake_minimum_required(VERSION 2.8.12...3.28)

project(NativeSticher LANGUAGES C)

set(LIBJPEG_TURBO_ROOT "${LIBJPEG_TURBO_ROOT}") 
set(LIBJPEG_TURBO_INCLUDE_DIR "${LIBJPEG_TURBO_ROOT}/include")
set(LIBJPEG_TURBO_LIB_DIR "${LIBJPEG_TURBO_ROOT}/lib")
set(LIBJPEG_LIBS turbojpeg)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    message(FATAL_ERROR "CMAKE_INSTALL_PREFIX not set. Please provide a prefix using -DCMAKE_INSTALL_PREFIX=<path>")
endif()


include_directories(${LIBJPEG_TURBO_INCLUDE_DIR})
link_directories(${LIBJPEG_TURBO_LIB_DIR})

add_library(${PROJECT_NAME} SHARED laplaceBlending.c edJpeg.c utils.c)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBJPEG_LIBS})


install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
message(STATUS "${LIBJPEG_TURBO_INCLUDE_DIR} yoyo ${CMAKE_SOURCE_DIR}")
install(DIRECTORY ${LIBJPEG_TURBO_INCLUDE_DIR}/ DESTINATION include) 
install(FILES laplaceBlending.h utils.h edJpeg.h  DESTINATION include)

