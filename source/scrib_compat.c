#include "scrib_compat.h"
#include "utils/logger.h"
#include <psp2/kernel/processmgr.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

int scrib_uname(void *out) {
    if (!out) { errno = EFAULT; return -1; }
    char (*fields)[65] = out;
    memset(out, 0, 6 * 65);
    snprintf(fields[0], 65, "Linux");
    snprintf(fields[1], 65, "PSVita");
    snprintf(fields[2], 65, "3.0.0");
    snprintf(fields[3], 65, "Scribblenauts Vita compatibility layer");
    snprintf(fields[4], 65, "armv7l");
    return 0;
}
int scrib_sigprocmask(int how, const uint32_t *set, uint32_t *old) {
    if (old) *old = 0;
    // Android signal handling is not implemented. Breakpad is disabled separately.
    if (set) { errno = ENOSYS; l_warn("sigprocmask is unsupported"); return -1; }
    return 0;
}
void scrib_exit(int status) {
    l_error("Android library requested exit(%d)", status);
    sceKernelExitProcess(status);
}

typedef struct { GLuint name; GLenum format; GLsizei width, height; } Renderbuffer;
static Renderbuffer buffers[64];
static GLuint bound_buffer;
static Renderbuffer *find_buffer(GLuint name, int create) {
    if (!name) return NULL;
    for (unsigned i = 0; i < 64; ++i) if (buffers[i].name == name) return &buffers[i];
    if (create) for (unsigned i = 0; i < 64; ++i) if (!buffers[i].name) {
        buffers[i].name = name; return &buffers[i];
    }
    return NULL;
}
void scrib_bind_renderbuffer(GLenum target, GLuint name) {
    glBindRenderbuffer(target, name);
    bound_buffer = name;
}
void scrib_renderbuffer_storage(GLenum target, GLenum format, GLsizei width, GLsizei height) {
    glRenderbufferStorage(target, format, width, height);
    Renderbuffer *buffer = find_buffer(bound_buffer, 1);
    if (buffer) { buffer->format = format; buffer->width = width; buffer->height = height; }
    else l_warn("Untracked renderbuffer %u", bound_buffer);
}
void scrib_delete_renderbuffers(GLsizei count, const GLuint *names) {
    for (int i = 0; i < count; ++i) {
        Renderbuffer *buffer = find_buffer(names[i], 0);
        if (buffer) memset(buffer, 0, sizeof(*buffer));
        if (bound_buffer == names[i]) bound_buffer = 0;
    }
    glDeleteRenderbuffers(count, names);
}
void scrib_get_renderbuffer_parameter(GLenum target, GLenum pname, GLint *value) {
    if (!value) return;
    *value = 0;
    Renderbuffer *buffer = find_buffer(bound_buffer, 0);
    if (!buffer) { l_warn("Query of untracked renderbuffer %u", bound_buffer); return; }
    switch (pname) {
    case 0x8D42: *value = buffer->width; break; // GL_RENDERBUFFER_WIDTH_OES
    case 0x8D43: *value = buffer->height; break;
    case 0x8D44: *value = buffer->format; break;
    default: l_warn("Renderbuffer query %x needs implementation", pname); break;
    }
}
