#ifndef MONARCH_BASE_SYS_GFX_FB_H
#define MONARCH_BASE_SYS_GFX_FB_H 1

#ifndef MONARCH_KERNEL_BUILD
#error "base/sys is kernel-only; userspace must use base/fbd or base/gfx instead."
#endif

/**
 * @file gfx_fb.h
 * @brief Kernel framebuffer adapter for the generic GFX surface renderer.
 */

#include "base/gfx/render.h"
#include "drivers/video/framebuffer.h"

/** Convert framebuffer bit layout into a generic pixel_format. */
int gfx_framebuffer_format(struct framebuffer *fb, struct pixel_format *out);

/** Create a non-owning surface view over framebuffer memory. */
int gfx_framebuffer_surface(struct framebuffer *fb, struct surface *out);

/** Allocate a GFX surface whose pixels match the framebuffer's native format. */
struct surface *gfx_surface_native(struct framebuffer *fb, uint32_t width, uint32_t height);

/** Convert an existing surface to the framebuffer's native pixel format. */
struct surface *gfx_surface_convert(struct framebuffer *fb, struct surface *src);

/** Initialise a renderer that draws directly into the framebuffer surface view. */
void render_framebuffer(struct renderer *r, struct framebuffer *fb);

#endif /* MONARCH_BASE_SYS_GFX_FB_H */
