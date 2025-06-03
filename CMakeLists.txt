cmake_minimum_required(VERSION 2.8.12...3.28)

project(NativeStitcher LANGUAGES C)

set(LIBJPEG_TURBO_ROOT      "${LIBJPEG_TURBO_ROOT}")
set(LIBJPEG_TURBO_INCLUDE_DIR "${LIBJPEG_TURBO_ROOT}/include")
set(LIBJPEG_TURBO_LIB_DIR     "${LIBJPEG_TURBO_ROOT}/lib")
set(LIBJPEG_LIBS turbojpeg)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    message(FATAL_ERROR
      "CMAKE_INSTALL_PREFIX not set. "
      "Please provide a prefix using -DCMAKE_INSTALL_PREFIX=<path>")
endif()


include_directories(
    ${LIBJPEG_TURBO_INCLUDE_DIR}
     ${CMAKE_SOURCE_DIR}/simde
)
link_directories(${LIBJPEG_TURBO_LIB_DIR})

add_library(${PROJECT_NAME} STATIC
    image_operations.c
    blending.c
    jpeg.c
    utils.c
)

target_compile_options(${PROJECT_NAME} PRIVATE -O3 -pthread)
target_link_libraries(${PROJECT_NAME} PRIVATE -pthread)
target_compile_definitions(${PROJECT_NAME} PRIVATE SIMDE_ENABLE_NATIVE_ALIASES)

target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBJPEG_LIBS})

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION lib)

install(DIRECTORY ${LIBJPEG_TURBO_INCLUDE_DIR}/
        DESTINATION include)

install(FILES image_operations.h
              blending.h
              utils.h
              jpeg.h
        DESTINATION include)
