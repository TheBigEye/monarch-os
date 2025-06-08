#include "demo.h"
#include "draw.h"
#include "font.h"

#include "../../boot/binaries.h"
#include "../../common/random.h"

#include "../CPU/PIT/timer.h"
#include "../drivers/FBC/graphics.h"
#include "../drivers/VGA/video.h"


void bglPlayWork(void) {
    // We create the main surface, we use existing bitmap pixels
    Surface* wallpaper = bglCreateSurfaceFrom(myhill_640, 640, 480);
    bglFlipSurface(wallpaper, false, true); // We flip the wallpaper surface

    // We create another surface ...
    Surface* overlay = bglCreateSurfaceFrom(mywork_640, 640, 480);
    if (!overlay) {
        bglDestroySurface(wallpaper); // Clean up if we fail
        return;
    }

    // bglResizeSurface(overlay, 640 / 2, 480 / 2);

    // We need to set the color key for the overlay surface
    bglSetColorKey(overlay, PX_BLUE); // Blue color is transparency now ...
    bglSetBlendMode(overlay, BGL_BLEND_ALPHA); // Full alpha blending

    // We make a single surface blitting both surfaces
    Rect destRect = (Rect) {0, 0, 640, 640};
    bglBlit(overlay, NULL, wallpaper, &destRect); // We blit the overlay into the wallpaper

    // Now, we blit the modified wallpaper into the screen
    bglBlitToScreen(wallpaper, NULL, 0, 0);

    // And now ... we clean the objects
    bglDestroySurface(overlay); // Fist the overlay
    bglDestroySurface(wallpaper); // And then the wallpaper
}


void bglPlayDemo(void) {
    // These are our magic numbers for the demo animation
    const uint16_t SQUARE_SIZE = 64;      // A nice size for our bouncing square
    const uint16_t CIRCLE_RADIUS = 32;    // Half the size of the square for the circle
    const uint16_t MOVE_SPEED = 2;        // Not too fast, not too slow ...

    // First, we need our main canvas to draw everything on
    Surface* screen = bglCreateSurface(640, 480);
    if (!screen) return;

    // We need a pretty background for our animation
    Surface* background = bglCreateSurfaceFrom(myhill_640, 640, 480);
    if (!background) {
        bglDestroySurface(screen);    // Oops, clean up and return if we fail
        return;
    }

    Font* smaller_font = bglCreateFont(small_font, 8, 8, NULL);
    if (!smaller_font) {
        bglDestroySurface(background); // Clean up if we fail
        bglDestroySurface(screen);
        return;
    }

    Rect textRect = (Rect) {8, 8, 0, 0};
    bglDrawText(background, smaller_font, "Graphics Test 01", &textRect, PX_WHITE, PX_BLACK);

    // Now, we create special surfaces for our shapes
    // This way we only need to draw them once, then we can reuse them!
    Surface* squareSurface = bglCreateSurface(SQUARE_SIZE, SQUARE_SIZE);
    Surface* circleSurface = bglCreateSurface(CIRCLE_RADIUS * 2, CIRCLE_RADIUS * 2);
    if (!squareSurface || !circleSurface) {
        // If something goes wrong, we clean everything up ...
        bglDestroySurface(background);
        bglDestroySurface(screen);
        if (squareSurface) bglDestroySurface(squareSurface);
        if (circleSurface) bglDestroySurface(circleSurface);
        return;
    }

    // The circle surface needs a black background for transparency
    bglFillSurface(circleSurface, PX_BLACK);  // Fill it with black first
    bglSetColorKey(circleSurface, PX_BLACK); // We tell BGL that black is our transparency color
    bglSetBlendMode(circleSurface, BGL_BLEND_ALPHA); // Make it blend

    // Now we draw our shapes once
    Rect squareRect = (Rect) {0, 0, SQUARE_SIZE, SQUARE_SIZE};
    bglFillRect(squareSurface, &squareRect, PX_GREEN); // A green square
    bglFillCircle(circleSurface, CIRCLE_RADIUS, CIRCLE_RADIUS, CIRCLE_RADIUS, PX_RED);  // And a red circle

    // Starting positions and movement vectors for our shapes
    uint16_t square_x = 100;              // Square starts here ...
    uint16_t square_y = 100;
    int8_t square_dx = MOVE_SPEED;        // ... and moves this way
    int8_t square_dy = MOVE_SPEED;

    uint16_t circle_x = 200;              // Circle starts at a different spot ...
    uint16_t circle_y = 200;
    int8_t circle_dx = -MOVE_SPEED;       // ... and moves the opposite way
    int8_t circle_dy = MOVE_SPEED;

    // We need to keep track of what areas of the screen change
    // This makes our animation much more efficient!
    DirtyRectList* dirtyRects = bglCreateDirtyRectList(4);
    if (!dirtyRects) {
        // More cleanup if things go wrong ...
        bglDestroySurface(squareSurface);
        bglDestroySurface(circleSurface);
        bglDestroySurface(background);
        bglDestroySurface(screen);
        return;
    }

    // Let's start with a clean screen
    bglBlit(background, NULL, screen, NULL);       // Copy our background
    bglBlitToScreen(screen, NULL, 0, 0);          // Show it to the world

    uint16_t frame = 0;  // We'll run for 256 frames

    // Here comes the fun part
    while ((frame++) < 256) {
        // Start fresh each frame
        bglClearDirtyRects(dirtyRects);

        // Remember where our shapes were last frame
        Rect old_square = {square_x, square_y, SQUARE_SIZE, SQUARE_SIZE};
        Rect old_circle = {circle_x - CIRCLE_RADIUS, circle_y - CIRCLE_RADIUS, CIRCLE_RADIUS * 2, CIRCLE_RADIUS * 2};

        // Move our shapes according to their speed
        square_x += square_dx;
        square_y += square_dy;
        circle_x += circle_dx;
        circle_y += circle_dy;

        // Make our shapes bounce off the screen edges
        if (square_x <= 0 || square_x >= 640 - SQUARE_SIZE) square_dx = -square_dx;
        if (square_y <= 0 || square_y >= 480 - SQUARE_SIZE) square_dy = -square_dy;
        if (circle_x <= CIRCLE_RADIUS || circle_x >= 640 - CIRCLE_RADIUS) circle_dx = -circle_dx;
        if (circle_y <= CIRCLE_RADIUS || circle_y >= 480 - CIRCLE_RADIUS) circle_dy = -circle_dy;

        // Calculate where the shapes are now
        Rect new_square = {square_x, square_y, SQUARE_SIZE, SQUARE_SIZE};
        Rect new_circle = {circle_x - CIRCLE_RADIUS, circle_y - CIRCLE_RADIUS, CIRCLE_RADIUS * 2, CIRCLE_RADIUS * 2};

        // Mark all the areas that need updating
        bglAddDirtyRect(dirtyRects, &old_square);     // Where the square was
        bglAddDirtyRect(dirtyRects, &old_circle);     // Where the circle was
        bglAddDirtyRect(dirtyRects, &new_square);     // Where the square is going
        bglAddDirtyRect(dirtyRects, &new_circle);     // Where the circle is going

        // Clean up the old positions by restoring the background
        bglUpdateRects(screen, background, dirtyRects);

        // Draw our shapes in their new positions
        bglBlit(squareSurface, NULL, screen, &new_square);    // Place the square
        bglBlit(circleSurface, NULL, screen, &new_circle);    // Place the circle

        // Show the result to the screen
        bglBlitToScreen(screen, NULL, 0, 0);
    }

    // Cleanup resources
    bglDestroySurface(screen);
    bglDestroySurface(background);

    bglDestroyFont(smaller_font);

    bglDestroySurface(squareSurface);
    bglDestroySurface(circleSurface);

    bglDestroyDirtyRectList(dirtyRects);
}


void bglPlayDemoEx(void) {
    const uint16_t SQUARE_SIZE = 240;
    const uint16_t MOVE_SPEED = 2;

    // Create main screen surface
    Surface* screen = bglCreateSurface(640, 480);
    if (!screen) return;

    // Create background surface
    Surface* background = bglCreateSurfaceFrom(myhill_640, 640, 480);
    if (!background) {
        bglDestroySurface(screen);
        return;
    }

    Font* smaller_font = bglCreateFont(small_font, 8, 8, NULL);
    if (!smaller_font) {
        bglDestroySurface(background); // Clean up if we fail
        bglDestroySurface(screen);
        return;
    }

    Rect textRect = (Rect) {8, 8, 0, 0};
    bglDrawText(background, smaller_font, "Graphics Test 02", &textRect, PX_WHITE, PX_BLACK);

    Surface* spritesheet = bglCreateSurfaceFrom(bigeye_480, 480, 480);
    if (!spritesheet) {
        bglDestroySurface(background);
        bglDestroySurface(screen);
        return;
    }

    Rect sprite_rect = (Rect) {0, 0, SQUARE_SIZE, SQUARE_SIZE}; // x, y, width, height
    Surface* sprite = bglCreateSprite(spritesheet, sprite_rect);
    bglSetColorKey(sprite, PX_BLACK); // Black is transparency now ...
    bglSetBlendMode(sprite, BGL_BLEND_ALPHA);

    // Initial positions for square and circle
    uint16_t square_x = 100;
    uint16_t square_y = 100;
    int8_t square_dx = MOVE_SPEED;
    int8_t square_dy = MOVE_SPEED;

    // Create dirty rect list, we only need 4 rects
    DirtyRectList* dirtyRects = bglCreateDirtyRectList(4);
    if (!dirtyRects) {
        bglDestroySurface(sprite);
        bglDestroySurface(spritesheet);
        bglDestroySurface(background);
        bglDestroySurface(screen);
        return;
    }

    // Copy background to screen initially
    bglBlit(background, NULL, screen, NULL);
    bglBlitToScreen(screen, NULL, 0, 0);

    uint16_t frame = 0;

    // Animation loop
    while ((frame++) < 256) {
        // Clear dirty rects from previous frame
        bglClearDirtyRects(dirtyRects);

        // Previous positions (for erasing)
        Rect old_square = {square_x, square_y, SQUARE_SIZE, SQUARE_SIZE};

        // Update positions
        square_x += square_dx;
        square_y += square_dy;

        // Bounce off edges
        if (square_x <= 0 || square_x >= 640 - SQUARE_SIZE) square_dx = -square_dx;
        if (square_y <= 0 || square_y >= 480 - SQUARE_SIZE) square_dy = -square_dy;

        // New positions
        Rect new_square = {square_x, square_y, SQUARE_SIZE, SQUARE_SIZE};

        // Add old and new positions to dirty rects
        bglAddDirtyRect(dirtyRects, &old_square);
        bglAddDirtyRect(dirtyRects, &new_square);

        // Restore background in dirty regions
        bglUpdateRects(screen, background, dirtyRects);

        bglBlit(sprite, &sprite_rect, screen, &new_square);

        // Update screen
        bglBlitToScreen(screen, NULL, 0, 0);

        // Simple delay
        //timerSleep(1);
    }

    // Cleanup
    bglDestroySurface(screen);
    bglDestroySurface(background);
    bglDestroyFont(smaller_font);
    bglDestroySurface(spritesheet);
    bglDestroySurface(sprite);
    bglDestroyDirtyRectList(dirtyRects);
}


void bglPlayTextDemo(void) {
    Surface* screen = bglCreateSurface(640, 480);
    if (!screen) return;

    Surface* background = bglCreateSurfaceFrom(myhill_640, 640, 480);
    if (!background) {
        bglDestroySurface(screen);
        return;
    }

    // Create fonts with different scales
    Font* smaller_font = bglCreateFont(small_font, 8, 8, 1);
    Font* medium_font = bglCreateFont(small_font, 8, 8, 2);
    Font* large_font = bglCreateFont(small_font, 8, 8, 3);

    if (!smaller_font || !medium_font || !large_font) {
        bglDestroySurface(background);
        bglDestroySurface(screen);
        if (smaller_font) bglDestroyFont(smaller_font);
        if (medium_font) bglDestroyFont(medium_font);
        if (large_font) bglDestroyFont(large_font);
        return;
    }


    Rect textRect = (Rect) {8, 8, 0, 0};
    bglDrawText(background, smaller_font, "Graphics Test 03", &textRect, PX_WHITE, PX_BLACK);

    // Create dirty rect list, 16 for old and new positions of 8 texts
    DirtyRectList* dirtyRects = bglCreateDirtyRectList(16);
    if (!dirtyRects) {
        bglDestroyFont(smaller_font);
        bglDestroyFont(medium_font);
        bglDestroyFont(large_font);
        bglDestroySurface(background);
        bglDestroySurface(screen);
        return;
    }

    // Copy background to screen initially
    bglBlit(background, NULL, screen, NULL);
    bglBlitToScreen(screen, NULL, 0, 0);

    // Initialize text positions spread across the screen
    uint16_t text_x[8] = {50, 350, 50, 350, 50, 350, 50, 350};
    uint16_t text_y[8] = {50, 50, 150, 150, 250, 250, 350, 350};
    int8_t text_dx[8] = {2, -2, 2, -2, 2, -2, 2, -2};
    int8_t text_dy[8] = {2, -2, -2, 2, 2, -2, -2, 2};

    const uint8_t text_color[8] = {
        PX_WHITE,
        PX_GREEN,
        PX_BLUE,
        PX_DKGRAY,
        PX_CYAN,
        PX_MAGENTA,
        PX_WHITE,
        PX_LTGRAY
    };

    const uint8_t text_bgcolor[8] = {
        PX_BLACK,
        PX_WHITE,
        PX_YELLOW,
        PX_LTGRAY,
        PX_LTRED,
        PX_LTGREEN,
        PX_BLACK,
        PX_MAGENTA
    };

    const char* text[8] = {
        "Hello BGL!",
        "Graphics Demo",
        "Pretty Cool",
        "Fast & Smooth",
        "Text Engine",
        "Butterfly OS",
        "Moving Text",
        "Demo System"
    };

    Rect text_rects[8] = {0};
    Font* fonts[8];

    // Pre-calculate initial text rectangles and assign fonts
    for (int i = 0; i < 8; i++) {
        fonts[i] = (i % 3 == 0) ? smaller_font : (i % 3 == 1) ? medium_font : large_font;

        text_rects[i].x = text_x[i];
        text_rects[i].y = text_y[i];
        bglGetTextSize(fonts[i], text[i], &text_rects[i].w, &text_rects[i].h);
    }

    uint16_t frame = 0;
    while (frame++ < 512) {
        bglClearDirtyRects(dirtyRects);

        for (int i = 0; i < 8; i++) {
            // Store old position
            Rect old_rect = text_rects[i];

            // Update position
            text_x[i] += text_dx[i];
            text_y[i] += text_dy[i];

            // Bounce off edges
            if (text_x[i] <= 0 || text_x[i] >= 640 - text_rects[i].w) {
                text_dx[i] = -text_dx[i];
                text_x[i] += text_dx[i];
            }
            if (text_y[i] <= 0 || text_y[i] >= 480 - text_rects[i].h) {
                text_dy[i] = -text_dy[i];
                text_y[i] += text_dy[i];
            }

            // Update rectangle position
            text_rects[i].x = text_x[i];
            text_rects[i].y = text_y[i];

            // Add both old and new positions to dirty rects
            bglAddDirtyRect(dirtyRects, &old_rect);
            bglAddDirtyRect(dirtyRects, &text_rects[i]);
        }

        // Restore background in all dirty regions at once
        bglUpdateRects(screen, background, dirtyRects);

        // Draw all texts in their new positions
        for (int i = 0; i < 8; i++) {
            bglDrawText(screen, fonts[i], text[i], &text_rects[i], text_color[i], text_bgcolor[i]);
        }

        bglBlitToScreen(screen, NULL, 0, 0);
    }

    // Cleanup
    bglDestroyFont(smaller_font);
    bglDestroyFont(medium_font);
    bglDestroyFont(large_font);
    bglDestroySurface(screen);
    bglDestroySurface(background);
    bglDestroyDirtyRectList(dirtyRects);
}
