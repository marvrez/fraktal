#pragma once
#include <log.h>

struct fArray
{
    GLuint fbo;
    GLuint color0;
    int width;
    int height;
    int channels;
    fEnum format;
    fEnum access;
};

static bool fraktal_format_to_gl_format(int channels,
                                 fEnum format,
                                 GLenum *internal_format,
                                 GLenum *data_format,
                                 GLenum *data_type)
{
    if (format == FRAKTAL_FLOAT)
    {
        *data_type = GL_FLOAT;
        if      (channels == 1) { *internal_format = GL_R32F; *data_format = GL_RED; return true; }
        else if (channels == 2) { *internal_format = GL_RG32F; *data_format = GL_RG; return true; }
        else if (channels == 4) { *internal_format = GL_RGBA32F; *data_format = GL_RGBA; return true; }
    }
    else if (format == FRAKTAL_UINT8)
    {
        *data_type = GL_UNSIGNED_BYTE;
        if      (channels == 1) { *internal_format = GL_R8; *data_format = GL_RED; return true; }
        else if (channels == 2) { *internal_format = GL_RG8; *data_format = GL_RG; return true; }
        else if (channels == 4) { *internal_format = GL_RGBA8; *data_format = GL_RGBA; return true; }
    }
    return false;
}

fArray *fraktal_create_array(
    const void *data,
    int width,
    int height,
    int channels,
    fEnum format,
    fEnum access)
{
    fraktal_ensure_context();
    fraktal_check_gl_error();
    fraktal_assert(channels > 0 && channels <= 4);
    fraktal_assert(width > 0 && height > 0);
    fraktal_assert(access == FRAKTAL_READ_ONLY || access == FRAKTAL_READ_WRITE);
    fraktal_assert(channels == 1 || channels == 2 || channels == 4);

    GLenum internal_format,data_format,data_type;
    fraktal_assert(fraktal_format_to_gl_format(channels, format, &internal_format, &data_format, &data_type) && "Invalid array format");

    GLenum target = height == 1 ? GL_TEXTURE_1D : GL_TEXTURE_2D;

    GLuint color0 = 0;
    {
        glGenTextures(1, &color0);
        glBindTexture(target, color0);
        if (target == GL_TEXTURE_1D)
        {
            glTexImage1D(target, 0, internal_format, width, 0, data_format, data_type, data);
            glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        }
        else if (target == GL_TEXTURE_2D)
        {
            glTexImage2D(target, 0, internal_format, width, height, 0, data_format, data_type, data);
            glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(target, 0);
        if (glGetError() != GL_NO_ERROR)
        {
            glDeleteTextures(1, &color0);
            log_err("Failed to create OpenGL texture object.\n");
            return NULL;
        }
    }

    GLuint fbo = 0;
    if (access == FRAKTAL_READ_WRITE)
    {
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        if (target == GL_TEXTURE_1D)
            glFramebufferTexture1D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, color0, 0);
        else if (target == GL_TEXTURE_2D)
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, color0, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (glGetError() != GL_NO_ERROR)
        {
            glDeleteFramebuffers(1, &fbo);
            glDeleteTextures(1, &color0);
            log_err("Failed to create framebuffer object.\n");
            return NULL;
        }
    }

    fArray *a = (fArray*)calloc(1, sizeof(fArray));
    a->color0 = color0;
    a->fbo = fbo;
    a->width = width;
    a->height = height;
    a->channels = channels;
    a->format = format;
    a->access = access;
    fraktal_check_gl_error();
    return a;
}

void fraktal_destroy_array(fArray *a)
{
    if (a)
    {
        fraktal_ensure_context();
        fraktal_check_gl_error();
        glDeleteTextures(1, &a->color0);
        glDeleteFramebuffers(1, &a->fbo);
        free(a);
        fraktal_check_gl_error();
    }
}

void fraktal_zero_array(fArray *a)
{
    fraktal_assert(a);
    fraktal_assert(a->access == FRAKTAL_READ_WRITE);
    fraktal_assert(a->fbo);
    fraktal_assert(a->color0);
    fraktal_ensure_context();
    fraktal_check_gl_error();
    GLint last_framebuffer; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &last_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, a->fbo);
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, last_framebuffer);
    fraktal_check_gl_error();
}

void fraktal_to_cpu(void *cpu_memory, fArray *a)
{
    fraktal_assert(cpu_memory);
    fraktal_assert(a);
    fraktal_assert(a->color0);
    fraktal_ensure_context();
    fraktal_check_gl_error();
    GLenum target = a->height == 1 ? GL_TEXTURE_1D : GL_TEXTURE_2D;
    GLenum internal_format,data_format,data_type;
    fraktal_assert(fraktal_format_to_gl_format(a->channels, a->format, &internal_format, &data_format, &data_type));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glBindTexture(target, a->color0);
    glGetTexImage(target, 0, data_format, data_type, cpu_memory);
    glBindTexture(target, 0);
    fraktal_check_gl_error();
}

void fraktal_array_size(fArray *a, int *width, int *height)
{
    if (a)
    {
        if (width) *width = a->width;
        if (height) *height = a->height;
    }
}

int fraktal_array_channels(fArray *a)
{
    if (a) return a->channels;
    return 0;
}

fEnum fraktal_array_format(fArray *a)
{
    if (a) return a->format;
    return -1;
}

bool fraktal_is_valid_array(fArray *a)
{
    return a &&
           a->width > 0 &&
           a->height > 0 &&
           (a->channels == 1 || a->channels == 2 || a->channels == 4) &&
           (a->access == FRAKTAL_READ_ONLY || (a->access == FRAKTAL_READ_WRITE && a->fbo)) &&
           (a->format == FRAKTAL_FLOAT || a->format == FRAKTAL_UINT8);
}

unsigned int fraktal_get_gl_handle(fArray *a)
{
    return a->color0;
}
