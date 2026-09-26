# Interactive 3D Space Artifact Gallery

A medium-sized interactive 3D space museum developed using C++, legacy
OpenGL, GLU, and FreeGLUT as a university Computer Graphics Lab project.

![Gallery Overview](screenshots/gallery-overview.png)

## Features

- Interactive perspective camera
- Animated mechanical Orrery
- Independently orbiting and rotating planets
- Rotating satellite with solar panels and antennas
- Detailed rocket exhibition model
- Moon-rock and astronaut displays
- Procedurally generated floor, wall, and wood textures
- Fixed-function lighting and materials
- Day/night exterior environment
- Moon, stars, mountains, city silhouettes, and moving comet
- Animated entrance door
- Selectable artifact transformations
- On-screen help and status information

## Technologies

- C++
- Legacy OpenGL
- GLU
- FreeGLUT
- MinGW / Code::Blocks

No shaders, model-loading libraries, game engines, or external image assets
are used.

## Build on Windows with MinGW

```powershell
g++ -std=c++11 -Wall -Wextra src/main.cpp -o SpaceGallery.exe -lfreeglut -lopengl32 -lglu32
