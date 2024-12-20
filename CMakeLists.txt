cmake_minimum_required(VERSION 3.10)

project(NativeSticher LANGUAGES C)

set(LIBJPEG_TURBO_ROOT "${LIBJPEG_TURBO_ROOT}") 
set(LIBJPEG_TURBO_INCLUDE_DIR "${LIBJPEG_TURBO_ROOT}/include")
set(LIBJPEG_TURBO_LIB_DIR "${LIBJPEG_TURBO_ROOT}/lib")
set(LIBJPEG_LIBS turbojpeg)


include_directories(${LIBJPEG_TURBO_INCLUDE_DIR})
link_directories(${LIBJPEG_TURBO_LIB_DIR})

add_library(${PROJECT_NAME} SHARED laplaceBlending.c edJpeg.c utils.c)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBJPEG_LIBS})
