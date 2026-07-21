# CUDA Mandelbrot - Stage Planning
End goal:

Produce 4k@60 HDR video zooming in on points of the Mandelbrot set

---

## Stages

Obviously, I can't do this in a day (without just vibe coding it, but that's boring and I learn nothing). This file breaks it down into distinct stages that I can build it in, and I may also use it as a scratchpad for the design

### 0 - Confirm Setup
I haven't run anything on our testing computer yet. Create and run a basic vector adder just to show that it works

### 1 - Naive Luminance Generation
Make the fundamental kernel that can identify the divergence point for a pixel. For now, just have a couple of small tests that get the value for known positions

### 2 - Naive Luminance Rendering
On the CPU side, take the Luminance values from 1 and produce a black and white image

### 3 - Apply a Color Mapping
This is primarily a research task. How do you turn a luminance value into something RGB that actually looks interesting. More importantly, how do the videos constantly and smoothly shift the color space?

### 4 - Optimize Luminance Generation
From the single frame optimizations section



## Optimization Possibilities
### Single Frame
#### Integer packing
The GPU memory bus can clearly handle more than the 16 bits that we'll need for luminance at a time. Is it an improvement to find a way to have threads send them together?

### Video
#### Frame reuse
In the video implementation, it really doesn't zoom in much frame-to-frame. Instead of redoing what is nearly the same point I did in the last frame, is there a way that I can be smart and reuse some values? Would definitely be lossy, but there might be a good way to do it.
