# Video Player

The video player of Cutefish Desktop. It plays local files and network
streams through libmpv, with subtitles, folder playlists, resume, seek-bar
previews and picture in picture, behind overlay controls that hide while
the video plays.

## Dependencies

Video Player uses Qt 6, libmpv, FishUI and the Cutefish framework
(`cutefish-framework`).

```sh
sudo apt install cmake pkg-config qt6-base-dev qt6-declarative-dev qt6-tools-dev qt6-tools-dev-tools qt6-shadertools-dev libmpv-dev
```

The tests make their sample clips with `ffmpeg`.

## Build and install

```sh
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
sudo make install
```

## Tests

```sh
cmake -DBUILD_TESTING=ON ..
make
dbus-run-session -- ctest --output-on-failure
```

## License

This project has been licensed by GPLv3.
