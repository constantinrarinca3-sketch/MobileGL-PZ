#pragma once

// The differential PixelStore test is CPU-only. Includes.h already declares
// the three opaque Xlib types Vulkan needs before including vulkan.h, so the
// test deliberately supplies an empty Xlib shim instead of requiring X11 dev
// packages in the verification environment.
