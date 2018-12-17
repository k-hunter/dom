/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "mozilla/RefPtr.h"
#include "WebGLBuffer.h"
#include "WebGLContext.h"
#include "WebGLProgram.h"
#include "WebGLVertexArray.h"
#include "WebGLVertexAttribData.h"

namespace mozilla {

bool
WebGL2Context::ValidateUniformMatrixTranspose(bool /*transpose*/, const char* /*info*/)
{
    return true;
}

// -------------------------------------------------------------------------
// Uniforms

void
WebGL2Context::Uniform1ui(WebGLUniformLocation* loc, GLuint v0)
{
    GLuint rawLoc;
    if (!ValidateUniformSetter(loc, 1, LOCAL_GL_UNSIGNED_INT, "uniform1ui", &rawLoc))
        return;

    MakeContextCurrent();
    gl->fUniform1ui(rawLoc, v0);
}

void
WebGL2Context::Uniform2ui(WebGLUniformLocation* loc, GLuint v0, GLuint v1)
{
    GLuint rawLoc;
    if (!ValidateUniformSetter(loc, 2, LOCAL_GL_UNSIGNED_INT, "uniform2ui", &rawLoc))
        return;

    MakeContextCurrent();
    gl->fUniform2ui(rawLoc, v0, v1);
}

void
WebGL2Context::Uniform3ui(WebGLUniformLocation* loc, GLuint v0, GLuint v1, GLuint v2)
{
    GLuint rawLoc;
    if (!ValidateUniformSetter(loc, 3, LOCAL_GL_UNSIGNED_INT, "uniform3ui", &rawLoc))
        return;

    MakeContextCurrent();
    gl->fUniform3ui(rawLoc, v0, v1, v2);
}

void
WebGL2Context::Uniform4ui(WebGLUniformLocation* loc, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    GLuint rawLoc;
    if (!ValidateUniformSetter(loc, 4, LOCAL_GL_UNSIGNED_INT, "uniform4ui", &rawLoc))
        return;

    MakeContextCurrent();
    gl->fUniform4ui(rawLoc, v0, v1, v2, v3);
}

void
WebGL2Context::Uniform1uiv_base(WebGLUniformLocation* loc, size_t arrayLength,
                                const GLuint* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;

    if (!ValidateUniformArraySetter(loc, 1, LOCAL_GL_UNSIGNED_INT, arrayLength,
                                    "uniform1uiv", &rawLoc, &numElementsToUpload))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniform1uiv(rawLoc, numElementsToUpload, data);
}

void
WebGL2Context::Uniform2uiv_base(WebGLUniformLocation* loc, size_t arrayLength,
                                const GLuint* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;

    if (!ValidateUniformArraySetter(loc, 2, LOCAL_GL_UNSIGNED_INT, arrayLength,
                                    "uniform2uiv", &rawLoc, &numElementsToUpload))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniform2uiv(rawLoc, numElementsToUpload, data);
}

void
WebGL2Context::Uniform3uiv_base(WebGLUniformLocation* loc, size_t arrayLength,
                                const GLuint* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;

    if (!ValidateUniformArraySetter(loc, 3, LOCAL_GL_UNSIGNED_INT, arrayLength,
                                    "uniform3uiv", &rawLoc, &numElementsToUpload))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniform1uiv(rawLoc, numElementsToUpload, data);
}

void
WebGL2Context::Uniform4uiv_base(WebGLUniformLocation* loc, size_t arrayLength,
                                const GLuint* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;

    if (!ValidateUniformArraySetter(loc, 4, LOCAL_GL_UNSIGNED_INT, arrayLength,
                                    "uniform4uiv", &rawLoc, &numElementsToUpload)) {
        return;
    }

    MakeContextCurrent();
    gl->fUniform4uiv(rawLoc, numElementsToUpload, data);
}

void
WebGL2Context::UniformMatrix2x3fv_base(WebGLUniformLocation* loc, bool transpose,
                                       size_t arrayLength, const GLfloat* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;

    if (!ValidateUniformMatrixArraySetter(loc, 2, 3, LOCAL_GL_FLOAT, arrayLength,
                                          transpose, "uniformMatrix2x3fv",
                                          &rawLoc, &numElementsToUpload))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniformMatrix2x3fv(rawLoc, numElementsToUpload, transpose, data);
}

void
WebGL2Context::UniformMatrix2x4fv_base(WebGLUniformLocation* loc, bool transpose,
                                       size_t arrayLength, const GLfloat* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;

    if (!ValidateUniformMatrixArraySetter(loc, 2, 4, LOCAL_GL_FLOAT, arrayLength,
                                          transpose, "uniformMatrix2x4fv",
                                          &rawLoc, &numElementsToUpload))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniformMatrix2x4fv(rawLoc, numElementsToUpload, transpose, data);
}

void
WebGL2Context::UniformMatrix3x2fv_base(WebGLUniformLocation* loc, bool transpose,
                                       size_t arrayLength, const GLfloat* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;

    if (!ValidateUniformMatrixArraySetter(loc, 3, 2, LOCAL_GL_FLOAT, arrayLength,
                                          transpose, "uniformMatrix3x2fv",
                                          &rawLoc, &numElementsToUpload))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniformMatrix3x2fv(rawLoc, numElementsToUpload, transpose, data);
}

void
WebGL2Context::UniformMatrix3x4fv_base(WebGLUniformLocation* loc, bool transpose,
                                       size_t arrayLength, const GLfloat* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;

    if (!ValidateUniformMatrixArraySetter(loc, 3, 4, LOCAL_GL_FLOAT, arrayLength,
                                          transpose, "uniformMatrix3x4fv",
                                          &rawLoc, &numElementsToUpload))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniformMatrix3x4fv(rawLoc, numElementsToUpload, transpose, data);
}

void
WebGL2Context::UniformMatrix4x2fv_base(WebGLUniformLocation* loc, bool transpose,
                                       size_t arrayLength, const GLfloat* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;

    if (!ValidateUniformMatrixArraySetter(loc, 4, 2, LOCAL_GL_FLOAT, arrayLength,
                                          transpose, "uniformMatrix4x2fv",
                                          &rawLoc, &numElementsToUpload))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniformMatrix4x2fv(rawLoc, numElementsToUpload, transpose, data);
}

void
WebGL2Context::UniformMatrix4x3fv_base(WebGLUniformLocation* loc, bool transpose,
                                       size_t arrayLength, const GLfloat* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;

    if (!ValidateUniformMatrixArraySetter(loc, 4, 3, LOCAL_GL_FLOAT, arrayLength,
                                          transpose, "uniformMatrix4x3fv",
                                          &rawLoc, &numElementsToUpload))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniformMatrix4x3fv(rawLoc, numElementsToUpload, transpose, data);
}


// -------------------------------------------------------------------------
// Uniform Buffer Objects and Transform Feedback Buffers
// TODO(djg): Implemented in WebGLContext
/*
    void BindBufferBase(GLenum target, GLuint index, WebGLBuffer* buffer);
    void BindBufferRange(GLenum target, GLuint index, WebGLBuffer* buffer,
                         GLintptr offset, GLsizeiptr size);
*/

/* This doesn't belong here. It's part of state querying */
void
WebGL2Context::GetIndexedParameter(GLenum target, GLuint index,
                                   dom::Nullable<dom::OwningWebGLBufferOrLongLong>& retval)
{
    retval.SetNull();
    if (IsContextLost())
        return;

    GLint64 data = 0;

    MakeContextCurrent();

    switch (target) {
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
        if (index >= mGLMaxTransformFeedbackSeparateAttribs)
            return ErrorInvalidValue("getIndexedParameter: index should be less than "
                                     "MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS");

        retval.SetValue().SetAsWebGLBuffer() =
            mBoundTransformFeedbackBuffers[index].get();
        return;

    case LOCAL_GL_UNIFORM_BUFFER_BINDING:
        if (index >= mGLMaxUniformBufferBindings)
            return ErrorInvalidValue("getIndexedParameter: index should be than "
                                     "MAX_UNIFORM_BUFFER_BINDINGS");

        retval.SetValue().SetAsWebGLBuffer() = mBoundUniformBuffers[index].get();
        return;

    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_START:
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
    case LOCAL_GL_UNIFORM_BUFFER_START:
    case LOCAL_GL_UNIFORM_BUFFER_SIZE:
        gl->fGetInteger64i_v(target, index, &data);
        retval.SetValue().SetAsLongLong() = data;
        return;
    }

    ErrorInvalidEnumInfo("getIndexedParameter: target", target);
}

void
WebGL2Context::GetUniformIndices(WebGLProgram* program,
                                 const dom::Sequence<nsString>& uniformNames,
                                 dom::Nullable< nsTArray<GLuint> >& retval)
{
    retval.SetNull();
    if (IsContextLost())
        return;

    if (!ValidateObject("getUniformIndices: program", program))
        return;

    if (!uniformNames.Length())
        return;

    GLuint progname = program->mGLName;
    size_t count = uniformNames.Length();
    nsTArray<GLuint>& arr = retval.SetValue();

    MakeContextCurrent();

    for (size_t n = 0; n < count; n++) {
        NS_LossyConvertUTF16toASCII name(uniformNames[n]);
        //        const GLchar* glname = name.get();
        const GLchar* glname = nullptr;
        name.BeginReading(glname);

        GLuint index = 0;
        gl->fGetUniformIndices(progname, 1, &glname, &index);
        arr.AppendElement(index);
    }
}

void
WebGL2Context::GetActiveUniforms(WebGLProgram* program,
                                 const dom::Sequence<GLuint>& uniformIndices,
                                 GLenum pname,
                                 dom::Nullable< nsTArray<GLint> >& retval)
{
    retval.SetNull();
    if (IsContextLost())
        return;

    if (!ValidateObject("getActiveUniforms: program", program))
        return;

    size_t count = uniformIndices.Length();
    if (!count)
        return;

    GLuint progname = program->mGLName;
    nsTArray<GLint>& arr = retval.SetValue();
    arr.SetLength(count);

    MakeContextCurrent();
    gl->fGetActiveUniformsiv(progname, count, uniformIndices.Elements(), pname,
                             arr.Elements());
}

GLuint
WebGL2Context::GetUniformBlockIndex(WebGLProgram* program,
                                    const nsAString& uniformBlockName)
{
    if (IsContextLost())
        return 0;

    if (!ValidateObject("getUniformBlockIndex: program", program))
        return 0;

    return program->GetUniformBlockIndex(uniformBlockName);
}

void
WebGL2Context::GetActiveUniformBlockParameter(JSContext* cx, WebGLProgram* program,
                                              GLuint uniformBlockIndex, GLenum pname,
                                              dom::Nullable<dom::OwningUnsignedLongOrUint32ArrayOrBoolean>& retval,
                                              ErrorResult& rv)
{
    retval.SetNull();
    if (IsContextLost())
        return;

    if (!ValidateObject("getActiveUniformBlockParameter: program", program))
        return;

    MakeContextCurrent();

    switch(pname) {
    case LOCAL_GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
    case LOCAL_GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER:
    case LOCAL_GL_UNIFORM_BLOCK_BINDING:
    case LOCAL_GL_UNIFORM_BLOCK_DATA_SIZE:
    case LOCAL_GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS:
        program->GetActiveUniformBlockParam(uniformBlockIndex, pname, retval);
        return;

    case LOCAL_GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES:
        program->GetActiveUniformBlockActiveUniforms(cx, uniformBlockIndex, retval, rv);
        return;
    }

    ErrorInvalidEnumInfo("getActiveUniformBlockParameter: parameter", pname);
}

void
WebGL2Context::GetActiveUniformBlockName(WebGLProgram* program, GLuint uniformBlockIndex,
                                         nsAString& retval)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("getActiveUniformBlockName: program", program))
        return;

    program->GetActiveUniformBlockName(uniformBlockIndex, retval);
}

void
WebGL2Context::UniformBlockBinding(WebGLProgram* program, GLuint uniformBlockIndex,
                                   GLuint uniformBlockBinding)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("uniformBlockBinding: program", program))
        return;

    program->UniformBlockBinding(uniformBlockIndex, uniformBlockBinding);
}

} // namespace mozilla
