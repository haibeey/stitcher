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
    popd

  done
}

build_libturbojpeg_ios() {
    BUILD_DIR="build-ios-$ARCH"
    mkdir -p "$BUILD_DIR"
    pushd "$BUILD_DIR"

    cmake .. \
      -G"Unix Makefiles" \
      -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_ARCHITECTURES=$ARCH \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
      -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR/ios/$ARCH" \
      -DCMAKE_C_COMPILER=$(xcrun --sdk iphoneos --find clang) \
      -DCMAKE_C_FLAGS="-target arm64-apple-ios14.0" \
      -DCMAKE_CXX_COMPILER=$(xcrun --sdk iphoneos --find clang++) \
      -DCMAKE_CXX_FLAGS="-target arm64-apple-ios14.0"

    make -j$(sysctl -n hw.logicalcpu)
    make install
    pop
}

build_libturbojpeg_ios_simulators() {
  echo "Building libjpeg-turbo for iOS Simulators..."
  for ARCH in x86_64 arm64; do
    BUILD_DIR="build-ios-simulator-$ARCH"
    mkdir -p "$BUILD_DIR"
    pushd "$BUILD_DIR"

    cmake .. \
      -G"Unix Makefiles" \
      -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_ARCHITECTURES=$ARCH \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
      -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR/ios-simulator/$ARCH" \
      -DCMAKE_C_COMPILER=$(xcrun --sdk iphonesimulator --find clang) \
      -DCMAKE_C_FLAGS="-target $ARCH-apple-ios14.0-simulator" \
      -DCMAKE_CXX_COMPILER=$(xcrun --sdk iphonesimulator --find clang++) \
      -DCMAKE_CXX_FLAGS="-target $ARCH-apple-ios14.0-simulator"

    make -j$(sysctl -n hw.logicalcpu)
    make install
    popd
  done
}




clean(){
  rm -rf "$BUILD_DIR_LIB_TURBOJPEG/$1"
  rm -rf "$INSTALL_DIR_LIB_TURBOJPEG/$1"
  rm -rf "$BUILD_DIR_LIB_NATIVE_STITCHER/$1"
  rm -rf "$INSTALL_DIR_LIB_NATIVE_STITCHER/$1"
}

help() {
  echo "
Command: build.sh
Usage:
  build.sh  --help        # Show this help information.
  build.sh  clean <platform>  # Clean build files for architecture.
  build.sh  <platform>        # Build the lib native library.
  "
  exit 1
}



# case "$1" in
#   clean)
#       clean $2
#     ;;
#   build)
#       case "$2" in
#         macos)
#           build_native_sticher_macos
#             ;;
#         ios)
#           build_native_sticher_ios
#             ;;
#         andriod)
#             build_native_sticher_andriod
#             ;;
#         ios-sim)
#             build_native_sticher_ios_sim
#             ;;              
#         *)
#         help
#         ;;
#       esac
#       ;;
#   *)
#   help
#   ;;
# esac

clean andriod
build_android