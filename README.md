


// if you you have libturbojpeg install you can run this command.
// gcc-14 -pthread -fsanitize=address -g -o stitch -I../ -I/usr/local/include -L/usr/local/lib -lturbojpeg  stitch.c ../laplaceBlending.c ../edJpeg.c ../utils.c && time ./stitch
// if you built the stitcher lib your self  can run the command as show below
// gcc-14 -pthread -fsanitize=address -g -o stitch -I../installs/native-stitcher/macos/x86_64/include -L../installs/native-stitcher/macos/x86_64/lib -Wl,-rpath,../installs/native-stitcher/macos/x86_64/lib -lNativeSticher stitch.c && ./stitch
## commands 
./build.sh clean macos
./build.sh build macos