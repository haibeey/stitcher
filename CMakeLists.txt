cmake_minimum_required(VERSION 3.10)

project(NativeSticher LANGUAGES C CXX)

set(LIBJPEG_TURBO_ROOT "${LIBJPEG_TURBO_ROOT}") 

if(APPLE)
  if(IOS)
    set(PLATFORM "iOS")
  else()
    set(PLATFORM "macOS")
  endif()
elseif(ANDROID)
  set(PLATFORM "Android")
else()
  message(FATAL_ERROR "Unsupported platform")
endif()



set(LIBJPEG_TURBO_INCLUDE_DIR "${LIBJPEG_TURBO_ROOT}/include")
set(LIBJPEG_TURBO_LIB_DIR "${LIBJPEG_TURBO_ROOT}/lib")


include_directories(${LIBJPEG_TURBO_INCLUDE_DIR})
link_directories(${LIBJPEG_TURBO_LIB_DIR})

add_library(${PROJECT_NAME} SHARED laplaceBlending.c edJpeg.c utils.c)


set(LIBJPEG_LIBS turbojpeg)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBJPEG_LIBS})


#cmake .. -DLIBJPEG_TURBO_ROOT=/path/to/libjpeg-turbo/install -DANDROID_ABI=arm64-v8a

