#pragma once
#include <vitaGL.h>

void scrib_debug_begin(unsigned frame);
void scrib_debug_end(void);
unsigned scrib_debug_draw_count(void);
int scrib_debug_capturing(void);
void scrib_debug_self_test(void);
void scrib_debug_matrix_mode(GLenum mode);
void scrib_debug_push_matrix(void);
void scrib_debug_pop_matrix(void);
void scrib_debug_vertex_pointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void scrib_debug_draw_arrays(GLenum mode, GLint first, GLsizei count);
