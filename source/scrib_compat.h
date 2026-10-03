#ifndef SCRIB_COMPAT_H
#define SCRIB_COMPAT_H
#include <vitaGL.h>
#include <stdint.h>
int scrib_uname(void *out);
int scrib_sigprocmask(int how, const uint32_t *set, uint32_t *old);
void scrib_exit(int status);
void scrib_bind_renderbuffer(GLenum target, GLuint name);
void scrib_renderbuffer_storage(GLenum target, GLenum format, GLsizei width, GLsizei height);
void scrib_delete_renderbuffers(GLsizei count, const GLuint *names);
void scrib_get_renderbuffer_parameter(GLenum target, GLenum pname, GLint *value);
#endif
