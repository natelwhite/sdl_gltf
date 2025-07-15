# Necessary Installations
- [SDL Wiki - Windows](https://wiki.libsdl.org/SDL3/README-windows)
- [SDL Wiki - Linux](https://wiki.libsdl.org/SDL3/README-linux)
# Build
- First, clone the repo and it's submodules
	- `git clone --recurse-submodules https://natelwhite/sdl_gltf.git`
- Then, while in `./sdl_gltf/`:
	- Generate build system: `cmake -B build`
	- Execute build system: `cmake --build build`
- The sdl_gltf executable should now be in ./sdl_gltf/build/.

