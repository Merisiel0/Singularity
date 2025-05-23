# Singularity

A simple shader displayer.<br>
I found a shader I liked called Singularity. I wanted it as my wallpaper so I made this.<br>
Turns out Cinnamon doesn't let you run a custom program as background so I repurposed it into a shader displayer.<br>
Enjoy.

## Tech used:

C++, Vulkan, SDL3, EnTT, GLM, vcpkg, CMake, stb

## Prerequisites:

You must have installed:
* vcpkg
* CMake
* Ninja
* g++

Environment variables:
* VCPKG_ROOT must be set to vcpkg's executable's path.
* VCPKG_ROOT must be added to PATH.

## Build and run
1. Clone this repository
2. Open with your code editor of choice (vscode and visual studio are confirmed to work)
3. Wait for CMake to finish generating build files
4. Launch the program with this command: ./Singularity --xy widthxheight --shader path/to/shader
