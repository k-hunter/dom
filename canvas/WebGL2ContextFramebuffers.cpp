/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "GLScreenBuffer.h"
#include "WebGLContextUtils.h"
#include "WebGLFormats.h"
#include "WebGLFramebuffer.h"

namespace mozilla {

using gl::GLContext;
using gl::GLFormats;
using webgl::EffectiveFormat;
using webgl::FormatInfo;
using webgl::ComponentType;

// Returns one of FLOAT, INT, UNSIGNED_INT.
// Fixed-points (normalized ints) are considered FLOAT.
static GLenum
ValueTypeForFormat(GLenum internalFormat)
{
    switch (internalFormat) {
    // Fixed-point
    case LOCAL_GL_R8:
    case LOCAL_GL_RG8:
    case LOCAL_GL_RGB565:
    case LOCAL_GL_RGB8:
    case LOCAL_GL_RGBA4:
    case LOCAL_GL_RGB5_A1:
    case LOCAL_GL_RGBA8:
    case LOCAL_GL_RGB10_A2:
    case LOCAL_GL_ALPHA8:
    case LOCAL_GL_LUMINANCE8:
    case LOCAL_GL_LUMINANCE8_ALPHA8:
    case LOCAL_GL_SRGB8:
    case LOCAL_GL_SRGB8_ALPHA8:
    case LOCAL_GL_R8_SNORM:
    case LOCAL_GL_RG8_SNORM:
    case LOCAL_GL_RGB8_SNORM:
    case LOCAL_GL_RGBA8_SNORM:

    // Floating-point
    case LOCAL_GL_R16F:
    case LOCAL_GL_RG16F:
    case LOCAL_GL_RGB16F:
    case LOCAL_GL_RGBA16F:
    case LOCAL_GL_ALPHA16F_EXT:
    case LOCAL_GL_LUMINANCE16F_EXT:
    case LOCAL_GL_LUMINANCE_ALPHA16F_EXT:

    case LOCAL_GL_R32F:
    case LOCAL_GL_RG32F:
    case LOCAL_GL_RGB32F:
    case LOCAL_GL_RGBA32F:
    case LOCAL_GL_ALPHA32F_EXT:
    case LOCAL_GL_LUMINANCE32F_EXT:
    case LOCAL_GL_LUMINANCE_ALPHA32F_EXT:

    case LOCAL_GL_R11F_G11F_B10F:
    case LOCAL_GL_RGB9_E5:
        return LOCAL_GL_FLOAT;

    // Int
    case LOCAL_GL_R8I:
    case LOCAL_GL_RG8I:
    case LOCAL_GL_RGB8I:
    case LOCAL_GL_RGBA8I:

    case LOCAL_GL_R16I:
    case LOCAL_GL_RG16I:
    case LOCAL_GL_RGB16I:
    case LOCAL_GL_RGBA16I:

    case LOCAL_GL_R32I:
    case LOCAL_GL_RG32I:
    case LOCAL_GL_RGB32I:
    case LOCAL_GL_RGBA32I:
        return LOCAL_GL_INT;

    // Unsigned int
    case LOCAL_GL_R8UI:
    case LOCAL_GL_RG8UI:
    case LOCAL_GL_RGB8UI:
    case LOCAL_GL_RGBA8UI:

    case LOCAL_GL_R16UI:
    case LOCAL_GL_RG16UI:
    case LOCAL_GL_RGB16UI:
    case LOCAL_GL_RGBA16UI:

    case LOCAL_GL_R32UI:
    case LOCAL_GL_RG32UI:
    case LOCAL_GL_RGB32UI:
    case LOCAL_GL_RGBA32UI:

    case LOCAL_GL_RGB10_A2UI:
        return LOCAL_GL_UNSIGNED_INT;

    default:
        MOZ_CRASH("Bad `internalFormat`.");
    }
}

// -------------------------------------------------------------------------
// Framebuffer objects

static bool
GetFBInfoForBlit(const WebGLFramebuffer* fb, WebGLContext* webgl,
                 const char* const fbInfo, GLsizei* const out_samples,
                 GLenum* const out_colorFormat, GLenum* const out_depthFormat,
                 GLenum* const out_stencilFormat)
{
    auto status = fb->PrecheckFramebufferStatus();
    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
        webgl->ErrorInvalidOperation("blitFramebuffer: %s is not"
                                     " framebuffer-complete.", fbInfo);
        return false;
    }

    *out_samples = 1; // TODO

    if (fb->ColorAttachment(0).IsDefined()) {
        const auto& attachement = fb->ColorAttachment(0);
        *out_colorFormat = attachement.EffectiveInternalFormat().get();
    } else {
        *out_colorFormat = 0;
    }

    if (fb->DepthStencilAttachment().IsDefined()) {
        const auto& attachement = fb->DepthStencilAttachment();
        *out_depthFormat = attachement.EffectiveInternalFormat().get();
        *out_stencilFormat = *out_depthFormat;
    } else {
        if (fb->DepthAttachment().IsDefined()) {
            const auto& attachement = fb->DepthAttachment();
            *out_depthFormat = attachement.EffectiveInternalFormat().get();
        } else {
            *out_depthFormat = 0;
        }

        if (fb->StencilAttachment().IsDefined()) {
            const auto& attachement = fb->StencilAttachment();
            *out_stencilFormat = attachement.EffectiveInternalFormat().get();
        } else {
            *out_stencilFormat = 0;
        }
    }
    return true;
}

void
WebGL2Context::BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                               GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                               GLbitfield mask, GLenum filter)
{
    const GLbitfield validBits = LOCAL_GL_COLOR_BUFFER_BIT |
                                 LOCAL_GL_DEPTH_BUFFER_BIT |
                                 LOCAL_GL_STENCIL_BUFFER_BIT;
    if ((mask | validBits) != validBits) {
        ErrorInvalidValue("blitFramebuffer: Invalid bit set in mask.");
        return;
    }

    switch (filter) {
    case LOCAL_GL_NEAREST:
    case LOCAL_GL_LINEAR:
        break;
    default:
        ErrorInvalidEnumInfo("blitFramebuffer: Bad `filter`:", filter);
        return;
    }

    const GLbitfield depthAndStencilBits = LOCAL_GL_DEPTH_BUFFER_BIT |
                                           LOCAL_GL_STENCIL_BUFFER_BIT;
    if (mask & depthAndStencilBits &&
        filter != LOCAL_GL_NEAREST)
    {
        ErrorInvalidOperation("blitFramebuffer: DEPTH_BUFFER_BIT and"
                              " STENCIL_BUFFER_BIT can only be used with"
                              " NEAREST filtering.");
        return;
    }

    if (mBoundReadFramebuffer == mBoundDrawFramebuffer) {
        // TODO: It's actually more complicated than this. We need to check that
        // the underlying buffers are not the same, not the framebuffers
        // themselves.
        ErrorInvalidOperation("blitFramebuffer: Source and destination must"
                              " differ.");
        return;
    }

    GLsizei srcSamples;
    GLenum srcColorFormat = 0;
    GLenum srcDepthFormat = 0;
    GLenum srcStencilFormat = 0;

    if (mBoundReadFramebuffer) {
        if (!GetFBInfoForBlit(mBoundReadFramebuffer, this, "READ_FRAMEBUFFER",
                              &srcSamples, &srcColorFormat, &srcDepthFormat,
                              &srcStencilFormat))
        {
            return;
        }
    } else {
        srcSamples = 1; // Always 1.

        // TODO: Don't hardcode these.
        srcColorFormat = mOptions.alpha ? LOCAL_GL_RGBA8 : LOCAL_GL_RGB8;

        if (mOptions.depth && mOptions.stencil) {
            srcDepthFormat = LOCAL_GL_DEPTH24_STENCIL8;
            srcStencilFormat = srcDepthFormat;
        } else {
            if (mOptions.depth) {
                srcDepthFormat = LOCAL_GL_DEPTH_COMPONENT16;
            }
            if (mOptions.stencil) {
                srcStencilFormat = LOCAL_GL_STENCIL_INDEX8;
            }
        }
    }

    GLsizei dstSamples;
    GLenum dstColorFormat = 0;
    GLenum dstDepthFormat = 0;
    GLenum dstStencilFormat = 0;

    if (mBoundDrawFramebuffer) {
        if (!GetFBInfoForBlit(mBoundDrawFramebuffer, this, "DRAW_FRAMEBUFFER",
                              &dstSamples, &dstColorFormat, &dstDepthFormat,
                              &dstStencilFormat))
        {
            return;
        }
    } else {
        dstSamples = gl->Screen()->Samples();

        // TODO: Don't hardcode these.
        dstColorFormat = mOptions.alpha ? LOCAL_GL_RGBA8 : LOCAL_GL_RGB8;

        if (mOptions.depth && mOptions.stencil) {
            dstDepthFormat = LOCAL_GL_DEPTH24_STENCIL8;
            dstStencilFormat = dstDepthFormat;
        } else {
            if (mOptions.depth) {
                dstDepthFormat = LOCAL_GL_DEPTH_COMPONENT16;
            }
            if (mOptions.stencil) {
                dstStencilFormat = LOCAL_GL_STENCIL_INDEX8;
            }
        }
    }


    if (mask & LOCAL_GL_COLOR_BUFFER_BIT) {
        const GLenum srcColorType = srcColorFormat ? ValueTypeForFormat(srcColorFormat)
                                                   : 0;
        const GLenum dstColorType = dstColorFormat ? ValueTypeForFormat(dstColorFormat)
                                                   : 0;
        if (dstColorType != srcColorType) {
            ErrorInvalidOperation("blitFramebuffer: Color buffer value type"
                                  " mismatch.");
            return;
        }

        const bool srcIsInt = srcColorType == LOCAL_GL_INT ||
                              srcColorType == LOCAL_GL_UNSIGNED_INT;
        if (srcIsInt && filter != LOCAL_GL_NEAREST) {
            ErrorInvalidOperation("blitFramebuffer: Integer read buffers can only"
                                  " be filtered with NEAREST.");
            return;
        }
    }

    /* GLES 3.0.4, p199:
     *   Calling BlitFramebuffer will result in an INVALID_OPERATION error if
     *   mask includes DEPTH_BUFFER_BIT or STENCIL_BUFFER_BIT, and the source
     *   and destination depth and stencil buffer formats do not match.
     *
     * jgilbert: The wording is such that if only DEPTH_BUFFER_BIT is specified,
     * the stencil formats must match. This seems wrong. It could be a spec bug,
     * or I could be missing an interaction in one of the earlier paragraphs.
     */
    if (mask & LOCAL_GL_DEPTH_BUFFER_BIT &&
        dstDepthFormat != srcDepthFormat)
    {
        ErrorInvalidOperation("blitFramebuffer: Depth buffer formats must match"
                              " if selected.");
        return;
    }

    if (mask & LOCAL_GL_STENCIL_BUFFER_BIT &&
        dstStencilFormat != srcStencilFormat)
    {
        ErrorInvalidOperation("blitFramebuffer: Stencil buffer formats must"
                              " match if selected.");
        return;
    }

    if (dstSamples != 1) {
        ErrorInvalidOperation("blitFramebuffer: DRAW_FRAMEBUFFER may not have"
                              " multiple samples.");
        return;
    }

    if (srcSamples != 1) {
        if (mask & LOCAL_GL_COLOR_BUFFER_BIT &&
            dstColorFormat != srcColorFormat)
        {
            ErrorInvalidOperation("blitFramebuffer: Color buffer formats must"
                                  " match if selected, when reading from a"
                                  " multisampled source.");
            return;
        }

        if (dstX0 != srcX0 ||
            dstX1 != srcX1 ||
            dstY0 != srcY0 ||
            dstY1 != srcY1)
        {
            ErrorInvalidOperation("blitFramebuffer: If the source is"
                                  " multisampled, then the source and dest"
                                  " regions must match exactly.");
            return;
        }
    }

    MakeContextCurrent();
    gl->fBlitFramebuffer(srcX0, srcY0, srcX1, srcY1,
                         dstX0, dstY0, dstX1, dstY1,
                         mask, filter);
}

static bool
ValidateTextureLayerAttachment(GLenum attachment)
{
    if (LOCAL_GL_COLOR_ATTACHMENT0 < attachment &&
        attachment <= LOCAL_GL_COLOR_ATTACHMENT15)
    {
        return true;
    }

    switch (attachment) {
    case LOCAL_GL_DEPTH_ATTACHMENT:
    case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
    case LOCAL_GL_STENCIL_ATTACHMENT:
        return true;
    }

    return false;
}

void
WebGL2Context::FramebufferTextureLayer(GLenum target, GLenum attachment,
                                       WebGLTexture* texture, GLint level, GLint layer)
{
    if (IsContextLost())
        return;

    if (!ValidateFramebufferTarget(target, "framebufferTextureLayer"))
        return;

    if (!ValidateTextureLayerAttachment(attachment))
        return ErrorInvalidEnumInfo("framebufferTextureLayer: attachment:", attachment);

    if (texture) {
        if (texture->IsDeleted()) {
            return ErrorInvalidValue("framebufferTextureLayer: texture must be a valid "
                                     "texture object.");
        }

        if (level < 0)
            return ErrorInvalidValue("framebufferTextureLayer: layer must be >= 0.");

        switch (texture->Target()) {
        case LOCAL_GL_TEXTURE_3D:
            if ((GLuint) layer >= mGLMax3DTextureSize) {
                return ErrorInvalidValue("framebufferTextureLayer: layer must be < "
                                         "MAX_3D_TEXTURE_SIZE");
            }
            break;

        case LOCAL_GL_TEXTURE_2D_ARRAY:
            if ((GLuint) layer >= mGLMaxArrayTextureLayers) {
                return ErrorInvalidValue("framebufferTextureLayer: layer must be < "
                                         "MAX_ARRAY_TEXTURE_LAYERS");
            }
            break;

        default:
            return ErrorInvalidOperation("framebufferTextureLayer: texture must be an "
                                         "existing 3D texture, or a 2D texture array.");
        }
    } else {
        return ErrorInvalidOperation("framebufferTextureLayer: texture must be an "
                                     "existing 3D texture, or a 2D texture array.");
    }

    WebGLFramebuffer* fb;
    switch (target) {
    case LOCAL_GL_FRAMEBUFFER:
    case LOCAL_GL_DRAW_FRAMEBUFFER:
        fb = mBoundDrawFramebuffer;
        break;

    case LOCAL_GL_READ_FRAMEBUFFER:
        fb = mBoundReadFramebuffer;
        break;

    default:
        MOZ_CRASH("Bad target.");
    }

    if (!fb) {
        return ErrorInvalidOperation("framebufferTextureLayer: cannot modify"
                                     " framebuffer 0.");
    }

    fb->FramebufferTextureLayer(attachment, texture, level, layer);
}

JS::Value
WebGL2Context::GetFramebufferAttachmentParameter(JSContext* cx,
                                                 GLenum target,
                                                 GLenum attachment,
                                                 GLenum pname,
                                                 ErrorResult& rv)
{
    if (IsContextLost())
        return JS::NullValue();

    // OpenGL ES 3.0.4 (August 27, 2014) 6.1. QUERYING GL STATE 240
    // "getFramebufferAttachmentParamter returns information about attachments of a bound
    // framebuffer object. target must be DRAW_FRAMEBUFFER, READ_FRAMEBUFFER, or
    // FRAMEBUFFER."

    if (!ValidateFramebufferTarget(target, "getFramebufferAttachmentParameter"))
        return JS::NullValue();

    // FRAMEBUFFER is equivalent to DRAW_FRAMEBUFFER.
    if (target == LOCAL_GL_FRAMEBUFFER)
        target = LOCAL_GL_DRAW_FRAMEBUFFER;

    WebGLFramebuffer* boundFB = nullptr;
    switch (target) {
    case LOCAL_GL_DRAW_FRAMEBUFFER: boundFB = mBoundDrawFramebuffer; break;
    case LOCAL_GL_READ_FRAMEBUFFER: boundFB = mBoundReadFramebuffer; break;
    }

    if (boundFB) {
        return boundFB->GetAttachmentParameter(cx, attachment, pname, rv);
    }

    // Handle default FB
    const gl::GLFormats& formats = gl->GetGLFormats();
    GLenum internalFormat = LOCAL_GL_NONE;

    /* If the default framebuffer is bound to target, then attachment must be BACK,
       identifying the color buffer; DEPTH, identifying the depth buffer; or STENCIL,
       identifying the stencil buffer. */
    switch (attachment) {
    case LOCAL_GL_BACK:
        internalFormat = formats.color_texInternalFormat;
        break;

    case LOCAL_GL_DEPTH:
        internalFormat = formats.depth;
        break;

    case LOCAL_GL_STENCIL:
        internalFormat = formats.stencil;
        break;

    default:
        ErrorInvalidEnum("getFramebufferAttachmentParameter: Can only query "
                         "attachment BACK, DEPTH, or STENCIL from default "
                         "framebuffer");
        return JS::NullValue();
    }

    const FormatInfo* info = webgl::GetInfoBySizedFormat(internalFormat);
    MOZ_RELEASE_ASSERT(info);
    EffectiveFormat effectiveFormat = info->effectiveFormat;

    switch (pname) {
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        return JS::Int32Value(LOCAL_GL_FRAMEBUFFER_DEFAULT);

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
        return JS::Int32Value(webgl::GetComponentSize(effectiveFormat, pname));

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
        if (attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT &&
            pname == LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE)
        {
            ErrorInvalidOperation("getFramebufferAttachmentParameter: Querying "
                                  "FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE against "
                                  "DEPTH_STENCIL_ATTACHMENT is an error.");
            return JS::NullValue();
        }

        return JS::Int32Value(webgl::GetComponentType(effectiveFormat));

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
        return JS::Int32Value(webgl::GetColorEncoding(effectiveFormat));
    }

    /* Any combinations of framebuffer type and pname not described above will generate an
       INVALID_ENUM error. */
    ErrorInvalidEnum("getFramebufferAttachmentParameter: Invalid combination of ");
    return JS::NullValue();
}

// Map attachments intended for the default buffer, to attachments for a non-
// default buffer.
static bool
TranslateDefaultAttachments(const dom::Sequence<GLenum>& in, dom::Sequence<GLenum>* out)
{
    for (size_t i = 0; i < in.Length(); i++) {
        switch (in[i]) {
            case LOCAL_GL_COLOR:
                if (!out->AppendElement(LOCAL_GL_COLOR_ATTACHMENT0, fallible)) {
                    return false;
                }
                break;

            case LOCAL_GL_DEPTH:
                if (!out->AppendElement(LOCAL_GL_DEPTH_ATTACHMENT, fallible)) {
                    return false;
                }
                break;

            case LOCAL_GL_STENCIL:
                if (!out->AppendElement(LOCAL_GL_STENCIL_ATTACHMENT, fallible)) {
                    return false;
                }
                break;
        }
    }

    return true;
}

void
WebGL2Context::InvalidateFramebuffer(GLenum target,
                                     const dom::Sequence<GLenum>& attachments,
                                     ErrorResult& rv)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();

    if (!ValidateFramebufferTarget(target, "framebufferRenderbuffer"))
        return;

    const WebGLFramebuffer* fb;
    bool isDefaultFB;
    switch (target) {
    case LOCAL_GL_FRAMEBUFFER:
    case LOCAL_GL_DRAW_FRAMEBUFFER:
        fb = mBoundDrawFramebuffer;
        isDefaultFB = gl->Screen()->IsDrawFramebufferDefault();
        break;

    case LOCAL_GL_READ_FRAMEBUFFER:
        fb = mBoundReadFramebuffer;
        isDefaultFB = gl->Screen()->IsReadFramebufferDefault();
        break;

    default:
        MOZ_CRASH("Bad target.");
    }

    for (size_t i = 0; i < attachments.Length(); i++) {
        if (!ValidateFramebufferAttachment(fb, attachments[i],
                                           "invalidateFramebuffer"))
        {
            return;
        }
    }

    // InvalidateFramebuffer is a hint to the driver. Should be OK to
    // skip calls if not supported, for example by OSX 10.9 GL
    // drivers.
    static bool invalidateFBSupported = gl->IsSupported(gl::GLFeature::invalidate_framebuffer);
    if (!invalidateFBSupported)
        return;

    if (!fb && !isDefaultFB) {
        dom::Sequence<GLenum> tmpAttachments;
        if (!TranslateDefaultAttachments(attachments, &tmpAttachments)) {
            rv.Throw(NS_ERROR_OUT_OF_MEMORY);
            return;
        }

        gl->fInvalidateFramebuffer(target, tmpAttachments.Length(), tmpAttachments.Elements());
    } else {
        gl->fInvalidateFramebuffer(target, attachments.Length(), attachments.Elements());
    }
}

void
WebGL2Context::InvalidateSubFramebuffer(GLenum target, const dom::Sequence<GLenum>& attachments,
                                        GLint x, GLint y, GLsizei width, GLsizei height,
                                        ErrorResult& rv)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();

    if (!ValidateFramebufferTarget(target, "framebufferRenderbuffer"))
        return;

    const WebGLFramebuffer* fb;
    bool isDefaultFB;
    switch (target) {
    case LOCAL_GL_FRAMEBUFFER:
    case LOCAL_GL_DRAW_FRAMEBUFFER:
        fb = mBoundDrawFramebuffer;
        isDefaultFB = gl->Screen()->IsDrawFramebufferDefault();
        break;

    case LOCAL_GL_READ_FRAMEBUFFER:
        fb = mBoundReadFramebuffer;
        isDefaultFB = gl->Screen()->IsReadFramebufferDefault();
        break;

    default:
        MOZ_CRASH("Bad target.");
    }

    for (size_t i = 0; i < attachments.Length(); i++) {
        if (!ValidateFramebufferAttachment(fb, attachments[i],
                                           "invalidateSubFramebuffer"))
        {
            return;
        }
    }

    // InvalidateFramebuffer is a hint to the driver. Should be OK to
    // skip calls if not supported, for example by OSX 10.9 GL
    // drivers.
    static bool invalidateFBSupported = gl->IsSupported(gl::GLFeature::invalidate_framebuffer);
    if (!invalidateFBSupported)
        return;

    if (!fb && !isDefaultFB) {
        dom::Sequence<GLenum> tmpAttachments;
        if (!TranslateDefaultAttachments(attachments, &tmpAttachments)) {
            rv.Throw(NS_ERROR_OUT_OF_MEMORY);
            return;
        }

        gl->fInvalidateSubFramebuffer(target, tmpAttachments.Length(), tmpAttachments.Elements(),
                                      x, y, width, height);
    } else {
        gl->fInvalidateSubFramebuffer(target, attachments.Length(), attachments.Elements(),
                                      x, y, width, height);
    }
}

void
WebGL2Context::ReadBuffer(GLenum mode)
{
    if (IsContextLost())
        return;

    const bool isColorAttachment = (mode >= LOCAL_GL_COLOR_ATTACHMENT0 &&
                                    mode <= LastColorAttachment());

    if (mode != LOCAL_GL_NONE && mode != LOCAL_GL_BACK && !isColorAttachment) {
        ErrorInvalidEnum("readBuffer: `mode` must be one of NONE, BACK, or "
                         "COLOR_ATTACHMENTi. Was %s",
                         EnumName(mode));
        return;
    }

    if (mBoundReadFramebuffer) {
        if (mode != LOCAL_GL_NONE &&
            !isColorAttachment)
        {
            ErrorInvalidOperation("readBuffer: If READ_FRAMEBUFFER is non-null, `mode` "
                                  "must be COLOR_ATTACHMENTi or NONE. Was %s",
                                  EnumName(mode));
            return;
        }

        MakeContextCurrent();
        gl->fReadBuffer(mode);
        return;
    }

    // Operating on the default framebuffer.
    if (mode != LOCAL_GL_NONE &&
        mode != LOCAL_GL_BACK)
    {
        ErrorInvalidOperation("readBuffer: If READ_FRAMEBUFFER is null, `mode`"
                              " must be BACK or NONE. Was %s",
                              EnumName(mode));
        return;
    }

    gl->Screen()->SetReadBuffer(mode);
}

} // namespace mozilla
