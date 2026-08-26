// MobileGL-PZCompat - isolated Project Zomboid compatibility frontend.
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once

#include <Includes.h>

namespace MobileGL::MG_Impl::GLImpl::PZCompat {
    // Returns true when the capability belongs to the compatibility frontend and
    // therefore must not be passed to the core-profile state validator.
    Bool SetLegacyCapability(GLenum cap, Bool enabled);
    Bool GetLegacyCapability(GLenum cap, GLboolean* enabled);

    void AlphaFunc(GLenum func, GLclampf ref);
    void MatrixMode(GLenum mode);
    void LoadIdentity();
    void LoadMatrixf(const GLfloat* matrix);
    void MultMatrixf(const GLfloat* matrix);
    void PushMatrix();
    void PopMatrix();
    void Ortho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
               GLdouble nearValue, GLdouble farValue);
    void Rotate(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
    void Scale(GLdouble x, GLdouble y, GLdouble z);
    void Translate(GLdouble x, GLdouble y, GLdouble z);

    void Color(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    void Normal(GLfloat x, GLfloat y, GLfloat z);
    void TexCoord(GLfloat s, GLfloat t);
    void TexEnvi(GLenum target, GLenum pname, GLint param);
    void ColorMaterial(GLenum face, GLenum mode);
    void Lightf(GLenum light, GLenum pname, GLfloat param);
    void Lightfv(GLenum light, GLenum pname, const GLfloat* params);
    void Materialfv(GLenum face, GLenum pname, const GLfloat* params);

    void PushAttrib(GLbitfield mask);
    void PopAttrib();
    void PushClientAttrib(GLbitfield mask);
    void PopClientAttrib();

    void Begin(GLenum mode);
    void End();
    void Vertex(GLfloat x, GLfloat y, GLfloat z, GLfloat w = 1.0f);

    void EnableClientState(GLenum cap);
    void DisableClientState(GLenum cap);
    void VertexPointer(GLint size, GLenum type, GLsizei stride, const void* pointer);

    // Return true if a fixed-function draw was consumed by PZCompat.  A false
    // result tells the exporting wrapper to use MobileGL's normal core path.
    Bool DrawArrays(GLenum mode, GLint first, GLsizei count);
    Bool DrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices);
    Bool DrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count,
                           GLenum type, const void* indices);

    // PZ's AngelCodeFont explicitly disables display-list caching when
    // glGenLists returns zero.  This is safer than pretending to record only a
    // subset of the commands issued between glNewList/glEndList.
    GLuint GenLists(GLsizei range);
    GLboolean IsList(GLuint list);
    void DeleteLists(GLuint list, GLsizei range);
    void NewList(GLuint list, GLenum mode);
    void EndList();
    void CallList(GLuint list);

    // KHR_debug is not advertised by the DirectGLES PZ profile, but LWJGL may
    // still resolve these symbols while constructing its capability table.
    void DebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count,
                             const GLuint* ids, GLboolean enabled);
    void DebugMessageCallback(GLDEBUGPROC callback, const void* userParam);

    // GL_ARB_texture_view is intentionally not advertised by the GL 4.0
    // DirectGLES profile.  Keep a deterministic guarded entry point for LWJGL.
    void TextureView(GLuint texture, GLenum target, GLuint originalTexture, GLenum internalFormat,
                     GLuint minLevel, GLuint numLevels, GLuint minLayer, GLuint numLayers);
}

