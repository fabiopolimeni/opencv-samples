# opencv_samples
OpenCV learning samples

## Bgfx issues:
1. RGB8 is not handled correctly by the D3D11/12 renderer, it seems to generate textures that always expect 4 channels.
2. OpenGL renderer on Windows seems to be locked at 30Hz regardless the refresh rate of the monitor or whether the vsync is enabled or not.
3. Shift modifiers on Linux doesn't work