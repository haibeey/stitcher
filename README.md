# NativeSticher
**NativeSticher** is a C library for multiband image merging using the Laplacian blending technique.
It enables seamless blending of overlapping images, useful for applications such as panorama generation, image stitching, and exposure fusion. Only supports JPEGS for now.

---

## Building
A helper script (`build.sh`) is included to streamline the build process for different platforms.
Use this script to build or clean the project easily.

### 🔨 Build Instructions:
- **Build for macOS:**
  ```bash
  ./build.sh build macos
  ```
- **Build for ios:**
  ```bash
  ./build.sh build ios
  ```

# Testing

To verify the functionality of **NativeSticher**, follow the instructions below based on your setup.

---

### 1. Testing with libturbojpeg (Direct Compilation)
If you have `libturbojpeg` installed, compile and run the test with the following command:
```bash
gcc-14 -O3 -mavx2 -mfma -I simde/ -pthread -fsanitize=address -g -o stitch \
-I../ -I/usr/local/include \
-L/usr/local/lib -lturbojpeg \
stitch.c ../blending.c ../jpeg.c ../image_operations.c ../utils.c && time ./stitch
```

### 2. Testing with Custom-Built NativeSticher Library
If you have manually built the NativeSticher library, use the following command to test it:
```bash
gcc-14 -pthread -fsanitize=address -g -o stitch \
-I../installs/native-stitcher/macos/x86_64/include \
-L../installs/native-stitcher/macos/x86_64/lib \
-Wl,-rpath,../installs/native-stitcher/macos/x86_64/lib \
-lNativeSticher stitch.c && ./stitch
```
The commands above assume a mac as working machine.
