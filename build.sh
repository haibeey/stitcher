#!/bin/bash
set -e 
BUILD_DIR_LIB_TURBOJPEG=$PWD/builds/libturbojpeg
INSTALL_DIR_LIB_TURBOJPEG=$PWD/installs/libturbojpeg
BUILD_DIR_LIB_NATIVE_STITCHER=$PWD/builds/native-stitcher
INSTALL_DIR_LIB_NATIVE_STITCHER=$PWD/installs/native-stitcher
LIBTURBO_JPEG_DIR=$PWD/libjpeg-turbo
CWD=$PWD


build_macos() {
  echo "Building libjpeg-turbo for macOS..."
  for ARCH in x86_64 arm64; do
    BUILD_DIR="$BUILD_DIR_LIB_TURBOJPEG/macos/$ARCH"
    INSTALL_DIR="$INSTALL_DIR_LIB_TURBOJPEG/macos/$ARCH"
    mkdir -p "$BUILD_DIR"
    pushd "$BUILD_DIR"

    if [ "$ARCH" == "arm64" ]; then
      TARGET_FLAG="-target arm64-apple-macos11"
    elif [ "$ARCH" == "x86_64" ]; then
      TARGET_FLAG="-target x86_64-apple-macos11"
    else
      echo "Unsupported architecture: $ARCH"
      popd
      continue
    fi

    cmake $LIBTURBO_JPEG_DIR \
      -G"Unix Makefiles" \
      -DCMAKE_SYSTEM_NAME=Darwin \
      -DCMAKE_OSX_ARCHITECTURES=$ARCH \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
      -DCMAKE_SYSTEM_PROCESSOR=$ARCH \
      -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
      -DCMAKE_C_COMPILER=$(xcrun --sdk macosx --find clang) \
      -DCMAKE_C_FLAGS="$TARGET_FLAG"

    make -j$(sysctl -n hw.logicalcpu)
    make install
    popd

    BUILD_DIR="$BUILD_DIR_LIB_NATIVE_STITCHER/macos/$ARCH"
    INSTALL_DIR="$INSTALL_DIR_LIB_NATIVE_STITCHER/macos/$ARCH"
    mkdir -p "$BUILD_DIR"
    mkdir -p "$INSTALL_DIR"
    pushd "$BUILD_DIR"
      cmake $CWD \
        -G"Unix Makefiles" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
        -DLIBJPEG_TURBO_ROOT=$CWD/installs/libturbojpeg/macos/$ARCH \
        -DCMAKE_SYSTEM_PROCESSOR=$ARCH \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DCMAKE_C_COMPILER=$(xcrun --sdk macosx --find clang) \
        -DCMAKE_C_FLAGS="$TARGET_FLAG"
      make -j$(sysctl -n hw.logicalcpu)
      make install
    popd    
  done
}

build_android() {
  echo "Building libjpeg-turbo for Android..."
  for ARCH in arm64-v8a armeabi-v7a x86 x86_64; do
    BUILD_DIR="$BUILD_DIR_LIB_TURBOJPEG/andriod/$ARCH"
    INSTALL_DIR="$INSTALL_DIR_LIB_TURBOJPEG/andriod/$ARCH"
    mkdir -p "$BUILD_DIR"
    

    if [ "$ARCH" == "arm64-v8a" ]; then
      TARGET_FLAG="-target aarch64-linux-android21"
      ARM_MODE="arm"
    elif [ "$ARCH" == "armeabi-v7a" ]; then
      TARGET_FLAG="-target armv7a-linux-androideabi21"
      ARM_MODE="arm"
    elif [ "$ARCH" == "x86" ]; then
      TARGET_FLAG="-target i686-linux-android21"
      ARM_MODE=""
    elif [ "$ARCH" == "x86_64" ]; then
      TARGET_FLAG="-target x86_64-linux-android21"
      ARM_MODE=""
    else
      echo "Unsupported architecture: $ARCH"
      popd
      continue
    fi

    pushd "$BUILD_DIR"
      cmake $LIBTURBO_JPEG_DIR \
        -G"Unix Makefiles" \
        -DANDROID_ABI=$ARCH \
        -DANDROID_ARM_MODE=$ARM_MODE \
        -DANDROID_PLATFORM=android-21 \
        -DANDROID_TOOLCHAIN=clang \
        -DCMAKE_SYSTEM_PROCESSOR=$ARCH \
        -DCMAKE_ASM_FLAGS="$TARGET_FLAG" \
        -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
        -DCMAKE_ANDROID_NDK=$ANDROID_NDK \
        -DCMAKE_SYSTEM_NAME=Android \
        -DCMAKE_ANDROID_ARCH_ABI=$ARCH \
        -DCMAKE_SYSTEM_VERSION=21 \
        -DCMAKE_C_FLAGS="$TARGET_FLAG" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"

      make -j$(sysctl -n hw.logicalcpu)
      make install
    popd

    BUILD_DIR="$BUILD_DIR_LIB_NATIVE_STITCHER/andriod/$ARCH"
    INSTALL_DIR="$INSTALL_DIR_LIB_NATIVE_STITCHER/andriod/$ARCH"
    mkdir -p "$BUILD_DIR"
    mkdir -p "$INSTALL_DIR"
    pushd "$BUILD_DIR"
      cmake $CWD \
        -G"Unix Makefiles" \
        -DLIBJPEG_TURBO_ROOT=$CWD/installs/libturbojpeg/andriod/$ARCH \
        -DANDROID_ABI=$ARCH \
        -DCMAKE_SYSTEM_PROCESSOR=$ARCH \
        -DANDROID_TOOLCHAIN=$ANDROID_NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/clang \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DCMAKE_C_COMPILER=$ANDROID_NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/clang \
        -DCMAKE_C_FLAGS="$TARGET_FLAG" \
        -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake 
      make -j$(sysctl -n hw.logicalcpu)
      make install
    popd

  done
}

build_ios() {
    echo "Building libjpeg-turbo for IOS..."
    ARCH=arm64
    BUILD_DIR="$BUILD_DIR_LIB_TURBOJPEG/ios/$ARCH"
    INSTALL_DIR="$INSTALL_DIR_LIB_TURBOJPEG/ios/$ARCH"
    mkdir -p "$BUILD_DIR"
    pushd "$BUILD_DIR"

      IOS_PLATFORMDIR=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform
      IOS_SYSROOT=($IOS_PLATFORMDIR/Developer/SDKs/iPhoneOS*.sdk)
      TARGET_FLAG="-Wall -miphoneos-version-min=12.6 -funwind-tables"

      cmake $LIBTURBO_JPEG_DIR \
        -G"Unix Makefiles" \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_ARCHITECTURES=$ARCH \
        -DCMAKE_SYSTEM_PROCESSOR=$ARCH \
        -DCMAKE_OSX_SYSROOT=${IOS_SYSROOT[0]} \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DCMAKE_C_COMPILER=$(xcrun --sdk iphoneos --find clang) \
        -DCMAKE_C_FLAGS="$TARGET_FLAG" 

      make -j$(sysctl -n hw.logicalcpu)
      make install
    popd

    BUILD_DIR="$BUILD_DIR_LIB_NATIVE_STITCHER/ios/$ARCH"
    INSTALL_DIR="$INSTALL_DIR_LIB_NATIVE_STITCHER/ios/$ARCH"
    mkdir -p "$BUILD_DIR"
    mkdir -p "$INSTALL_DIR"
    pushd "$BUILD_DIR"
      cmake $CWD \
        -G"Unix Makefiles" \
        -DLIBJPEG_TURBO_ROOT=$CWD/installs/libturbojpeg/ios/$ARCH \
        -DCMAKE_SYSTEM_PROCESSOR=$ARCH \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_SYSROOT=${IOS_SYSROOT[0]} \
        -DCMAKE_C_FLAGS="-target arm64-apple-ios12.0" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DCMAKE_C_COMPILER=$(xcrun --sdk iphoneos --find clang) 
      make -j$(sysctl -n hw.logicalcpu)
      make install
    popd
}

build_ios_sim() {
    echo "Building libjpeg-turbo for IOS emulators..."
    for ARCH in x86_64 arm64; do
      BUILD_DIR="$BUILD_DIR_LIB_TURBOJPEG/ios-sim/$ARCH"
      INSTALL_DIR="$INSTALL_DIR_LIB_TURBOJPEG/ios-sim/$ARCH"
      mkdir -p "$BUILD_DIR"
      pushd "$BUILD_DIR"

        IOS_PLATFORMDIR=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform
        IOS_SYSROOT=($IOS_PLATFORMDIR/Developer/SDKs/iPhoneSimulator*.sdk)
        TARGET_FLAG="-Wall -miphonesimulator-version-min=12.6 -funwind-tables"

        cmake $LIBTURBO_JPEG_DIR \
          -G"Unix Makefiles" \
          -DCMAKE_SYSTEM_NAME=iOS \
          -DCMAKE_OSX_ARCHITECTURES=$ARCH \
          -DCMAKE_SYSTEM_PROCESSOR=$ARCH \
          -DCMAKE_OSX_SYSROOT=${IOS_SYSROOT[0]} \
          -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
          -DCMAKE_C_COMPILER=$(xcrun --sdk iphoneos --find clang) \
          -DCMAKE_C_FLAGS="$TARGET_FLAG" 

        make -j$(sysctl -n hw.logicalcpu)
        make install
      popd

      BUILD_DIR="$BUILD_DIR_LIB_NATIVE_STITCHER/ios-sim/$ARCH"
      INSTALL_DIR="$INSTALL_DIR_LIB_NATIVE_STITCHER/ios-sim/$ARCH"
      mkdir -p "$BUILD_DIR"
      mkdir -p "$INSTALL_DIR"
      pushd "$BUILD_DIR"
        cmake $CWD \
          -G"Unix Makefiles" \
          -DLIBJPEG_TURBO_ROOT=$CWD/installs/libturbojpeg/ios-sim/$ARCH \
          -DCMAKE_SYSTEM_PROCESSOR=$ARCH \
          -DCMAKE_SYSTEM_NAME=iOS \
          -DCMAKE_OSX_SYSROOT=${IOS_SYSROOT[0]} \
          -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
          -DCMAKE_C_FLAGS="-target $ARCH-apple-ios14.0-simulator" \
          -DCMAKE_C_COMPILER=$(xcrun --sdk iphonesimulator --find clang) 
        make -j$(sysctl -n hw.logicalcpu)
        make install
      popd
    done
}




clean(){
  ALLOWED_PLATFORMS=("ios" "android" "macos" "ios-sim")
  if [[ " ${ALLOWED_PLATFORMS[@]} " =~ " $1 " ]]; then
      rm -rf "$BUILD_DIR_LIB_TURBOJPEG/$1"
      rm -rf "$INSTALL_DIR_LIB_TURBOJPEG/$1"
      rm -rf "$BUILD_DIR_LIB_NATIVE_STITCHER/$1"
      rm -rf "$INSTALL_DIR_LIB_NATIVE_STITCHER/$1"
  else
      echo "$1 is not a valid platform. Allowed: ${ALLOWED_PLATFORMS[*]}"
      exit 1
  fi
}

test(){
  pushd "tests"
    gcc-14 -I../ -I/usr/local/include -L/usr/local/lib -lturbojpeg -pthread -fsanitize=address -g -o test  ../laplace_blending.c ../jpeg.c ../utils.c ../test.c && ./test
  popd
}

help() {
  echo "
Command: build.sh
Usage:
  build.sh  --help        # Show this help information.
  build.sh  clean <platform>  # Clean build files for architecture.
  build.sh  build <platform>        # Build the lib native library.
  "
  exit 1
}



case "$1" in
  clean)
      clean $2
    ;;
  build)
      case "$2" in
        macos)
          build_macos
            ;;
        ios)
          build_ios
            ;;
        android)
            build_android
            ;;
        ios-sim)
            build_ios_sim
            ;;              
        *)
        help
        ;;
      esac
      ;;
  test)
    test
    ;;
  *)
  help
  ;;
esac


# gcc-14 -I../ -I/usr/local/include -L/usr/local/lib -lturbojpeg -pthread -fsanitize=address -g -o downsampled ../laplace_blending.c ../jpeg.c ../utils.c downsampling.c && ./downsampled
# gcc-14 -I../ -I/usr/local/include -L/usr/local/lib -lturbojpeg -pthread -fsanitize=address -g -o stitch  ../laplace_blending.c ../jpeg.c ../utils.c stitch.c && ./stitch