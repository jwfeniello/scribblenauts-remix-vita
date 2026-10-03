#include "scrib_debug.h"
#include "utils/logger.h"
#include <psp2/io/stat.h>
#include <psp2/io/fcntl.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static unsigned current_frame, draw_count, error_count;
static int capture;
static GLint vertex_size;
static GLenum vertex_type;
static GLsizei vertex_stride;
static const unsigned char *vertices;
static unsigned matrix_events;

void scrib_debug_self_test(void) {
    GLint saved_mode;
    GLfloat saved[16], restored[16];
    glGetIntegerv(GL_MATRIX_MODE, &saved_mode);
    glMatrixMode(GL_MODELVIEW);
    glGetFloatv(GL_MODELVIEW_MATRIX, saved);
    GLenum stale = glGetError();
    glLoadIdentity();
    glPushMatrix();
    GLenum push_error = glGetError();
    glTranslatef(123, 456, 789);
    glPopMatrix();
    GLenum pop_error = glGetError();
    glGetFloatv(GL_MODELVIEW_MATRIX, restored);
    l_info("Matrix self-test: stale=%x push=%x pop=%x restored=%f,%f,%f diag=%f,%f,%f,%f",
           stale, push_error, pop_error, restored[12], restored[13], restored[14],
           restored[0], restored[5], restored[10], restored[15]);
    glLoadMatrixf(saved);
    glMatrixMode(saved_mode);
    GLint saved_texture, units;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &saved_texture);
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &units);
    unsigned failed = 0;
    const GLenum modes[] = {GL_MODELVIEW, GL_PROJECTION, GL_TEXTURE};
    for (unsigned i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        glMatrixMode(modes[i]);
        // Reproduce the game's DisableTU(1), DisableTU(2), DisableTU(3), TU(0).
        for (unsigned unit = 1; unit <= 4; ++unit) {
            glActiveTexture(GL_TEXTURE0 + unit % 4);
            GLint mode;
            glGetIntegerv(GL_MATRIX_MODE, &mode);
            if (mode != modes[i]) ++failed;
        }
    }
    glActiveTexture(saved_texture);
    glMatrixMode(saved_mode);
    l_info("Texture matrix self-test: units=%d mode_failures=%u error=%x", units, failed, glGetError());
}

static void matrix_event(const char *name, GLint requested, GLint before, void *caller) {
    GLint after;
    GLfloat model[16];
    glGetIntegerv(GL_MATRIX_MODE, &after);
    glGetFloatv(GL_MODELVIEW_MATRIX, model);
    GLenum error = glGetError();
    l_info("MATRIX frame=%u %s requested=%x before=%x after=%x error=%x caller=%p model=%f,%f,%f diag=%f,%f",
           current_frame, name, requested, before, after, error, caller,
           model[12], model[13], model[14], model[0], model[5]);
}

void scrib_debug_matrix_mode(GLenum mode) {
    GLint before = 0;
    int trace = matrix_events++ < 100;
    if (trace) glGetIntegerv(GL_MATRIX_MODE, &before);
    glMatrixMode(mode);
    if (trace) matrix_event("mode", mode, before, __builtin_return_address(0));
}

void scrib_debug_push_matrix(void) {
    GLint before = 0;
    int trace = matrix_events++ < 100;
    if (trace) glGetIntegerv(GL_MATRIX_MODE, &before);
    glPushMatrix();
    if (trace) matrix_event("push", 0, before, __builtin_return_address(0));
}

void scrib_debug_pop_matrix(void) {
    GLint before = 0;
    int trace = matrix_events++ < 100;
    if (trace) glGetIntegerv(GL_MATRIX_MODE, &before);
    glPopMatrix();
    if (trace) matrix_event("pop", 0, before, __builtin_return_address(0));
}

void scrib_debug_begin(unsigned frame) {
    current_frame = frame;
    draw_count = 0;
    capture = 0; // Screenshots and detailed draw traces are now opt-in.
    if (frame % 30 == 0) {
        SceIoStat st;
        if (sceIoGetstat(DATA_PATH "capture.request", &st) >= 0) {
            sceIoRemove(DATA_PATH "capture.request");
            capture = 1;
        }
    }
}

void scrib_debug_vertex_pointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) {
    vertex_size = size;
    vertex_type = type;
    vertex_stride = stride;
    vertices = pointer;
    glVertexPointer(size, type, stride, pointer);
}

static float coordinate(int vertex, int component) {
    if (!vertices || component >= vertex_size) return 0;
    unsigned bytes = vertex_type == GL_SHORT ? 2 : 4;
    const unsigned char *p = vertices + vertex * (vertex_stride ? vertex_stride : bytes * vertex_size) + bytes * component;
    if (vertex_type == GL_FLOAT) { float n; memcpy(&n, p, 4); return n; }
    if (vertex_type == GL_SHORT) { int16_t n; memcpy(&n, p, 2); return n; }
    if (vertex_type == GL_FIXED) { int32_t n; memcpy(&n, p, 4); return n / 65536.0f; }
    return 0;
}

void scrib_debug_draw_arrays(GLenum mode, GLint first, GLsizei count) {
    if (capture && draw_count < 120) {
        GLint texture, viewport[4];
        GLfloat model[16], projection[16];
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetFloatv(GL_MODELVIEW_MATRIX, model);
        glGetFloatv(GL_PROJECTION_MATRIX, projection);
        l_info("DRAW frame=%u #%u mode=%x count=%d tex=%d vertex=%dx%x stride=%d viewport=%d,%d,%d,%d",
               current_frame, draw_count, mode, count, texture, vertex_size, vertex_type, vertex_stride,
               viewport[0], viewport[1], viewport[2], viewport[3]);
        l_info("VERTEX first=%d (%f,%f,%f) second=(%f,%f,%f)", first,
               coordinate(first, 0), coordinate(first, 1), coordinate(first, 2),
               coordinate(first + (count > 1), 0), coordinate(first + (count > 1), 1), coordinate(first + (count > 1), 2));
        l_info("MODEL diag=%f,%f,%f,%f translate=%f,%f,%f; PROJECTION diag=%f,%f,%f,%f translate=%f,%f,%f",
               model[0], model[5], model[10], model[15], model[12], model[13], model[14],
               projection[0], projection[5], projection[10], projection[15], projection[12], projection[13], projection[14]);
    }
    ++draw_count;
    glDrawArrays(mode, first, count);
}

static void screenshot(void) {
    const unsigned width = 960, height = 544;
    unsigned char *pixels = malloc(width * height * 4);
    if (!pixels) { l_error("No memory for screen capture"); return; }
    GLenum pending = glGetError();
    if (pending) l_error("Pending GL error before capture: %x", pending);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    GLenum error = glGetError();
    if (error) { l_error("Screen readback failed: %x", error); free(pixels); return; }
    // Display scanout ignores alpha. Record visible RGB as opaque pixels.
    for (unsigned i = 3; i < width * height * 4; i += 4) pixels[i] = 255;
    char path[256];
    snprintf(path, sizeof(path), DATA_PATH "frame-%06u.png", current_frame);
    FILE *file = fopen(path, "wb");
    if (!file) { free(pixels); l_error("Could not open capture %s", path); return; }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info) {
        if (png) png_destroy_write_struct(&png, NULL);
        fclose(file); free(pixels); return;
    }
    if (!setjmp(png_jmpbuf(png))) {
        png_init_io(png, file);
        png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGBA,
                     PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
        png_write_info(png, info);
        for (int y = height - 1; y >= 0; --y) png_write_row(png, pixels + y * width * 4);
        png_write_end(png, info);
        l_info("Captured %s (%u draw calls)", path, draw_count);
    } else l_error("PNG capture failed");
    png_destroy_write_struct(&png, &info);
    fclose(file);
    free(pixels);
}

void scrib_debug_end(void) {
    if (capture) screenshot();
    GLenum error = glGetError();
    if (error && error_count++ < 20)
        l_warn("GL error %x at frame=%u after %u draws", error, current_frame, draw_count);
}

unsigned scrib_debug_draw_count(void) { return draw_count; }
int scrib_debug_capturing(void) { return capture; }
