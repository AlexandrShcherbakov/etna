# Etna
The goal of this framework is simplest real-time graphics feature prototyping.

## Main concepts
Adding of a graphics feature should consist of 3 changes in program:
1. Create resources (textures or buffers).
2. Create a pipeline (shaders + render states).
3. Add necessary drawcalls/dispatches in command buffer creation.

We don't want to have a global state on a command buffer creation, so we are going to implement RAII classes which should set/reset render targets and other resources.
