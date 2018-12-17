/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include <queue>

#include "AccessCheck.h"
#include "gfxContext.h"
#include "gfxCrashReporterUtils.h"
#include "gfxPattern.h"
#include "gfxPrefs.h"
#include "gfxUtils.h"
#include "GLBlitHelper.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "GLReadTexImageHelper.h"
#include "GLScreenBuffer.h"
#include "ImageContainer.h"
#include "ImageEncoder.h"
#include "Layers.h"
#include "LayerUserData.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/EnumeratedArrayCycleCollection.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProcessPriorityManager.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsError.h"
#include "nsIClassInfoImpl.h"
#include "nsIConsoleService.h"
#include "nsIDOMEvent.h"
#include "nsIGfxInfo.h"
#include "nsIObserverService.h"
#include "nsIVariant.h"
#include "nsIWidget.h"
#include "nsIXPConnect.h"
#include "nsServiceManagerUtils.h"
#include "nsSVGEffects.h"
#include "prenv.h"
#include "ScopedGLHelpers.h"

#ifdef MOZ_WIDGET_GONK
#include "mozilla/layers/ShadowLayers.h"
#endif

// Local
#include "CanvasUtils.h"
#include "WebGL1Context.h"
#include "WebGLActiveInfo.h"
#include "WebGLBuffer.h"
#include "WebGLContextLossHandler.h"
#include "WebGLContextUtils.h"
#include "WebGLExtensions.h"
#include "WebGLFramebuffer.h"
#include "WebGLMemoryTracker.h"
#include "WebGLObjectModel.h"
#include "WebGLProgram.h"
#include "WebGLQuery.h"
#include "WebGLSampler.h"
#include "WebGLShader.h"
#include "WebGLTransformFeedback.h"
#include "WebGLVertexArray.h"
#include "WebGLVertexAttribData.h"

// Generated
#include "mozilla/dom/WebGLRenderingContextBinding.h"


namespace mozilla {

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::gl;
using namespace mozilla::layers;

WebGLContextOptions::WebGLContextOptions()
    : alpha(true)
    , depth(true)
    , stencil(false)
    , premultipliedAlpha(true)
    , antialias(true)
    , preserveDrawingBuffer(false)
    , failIfMajorPerformanceCaveat(false)
{
    // Set default alpha state based on preference.
    if (gfxPrefs::WebGLDefaultNoAlpha())
        alpha = false;
}

WebGLContext::WebGLContext()
    : WebGLContextUnchecked(nullptr)
    , mBypassShaderValidation(false)
    , mGLMaxSamples(1)
    , mNeedsFakeNoAlpha(false)
    , mNeedsFakeNoDepth(false)
    , mNeedsFakeNoStencil(false)
{
    mGeneration = 0;
    mInvalidated = false;
    mCapturedFrameInvalidated = false;
    mShouldPresent = true;
    mResetLayer = true;
    mOptionsFrozen = false;

    mActiveTexture = 0;
    mPixelStoreFlipY = false;
    mPixelStorePremultiplyAlpha = false;
    mPixelStoreColorspaceConversion = BROWSER_DEFAULT_WEBGL;

    mFakeBlackStatus = WebGLContextFakeBlackStatus::NotNeeded;

    mVertexAttrib0Vector[0] = 0;
    mVertexAttrib0Vector[1] = 0;
    mVertexAttrib0Vector[2] = 0;
    mVertexAttrib0Vector[3] = 1;
    mFakeVertexAttrib0BufferObjectVector[0] = 0;
    mFakeVertexAttrib0BufferObjectVector[1] = 0;
    mFakeVertexAttrib0BufferObjectVector[2] = 0;
    mFakeVertexAttrib0BufferObjectVector[3] = 1;
    mFakeVertexAttrib0BufferObjectSize = 0;
    mFakeVertexAttrib0BufferObject = 0;
    mFakeVertexAttrib0BufferStatus = WebGLVertexAttrib0Status::Default;

    mViewportX = 0;
    mViewportY = 0;
    mViewportWidth = 0;
    mViewportHeight = 0;

    mDitherEnabled = 1;
    mRasterizerDiscardEnabled = 0; // OpenGL ES 3.0 spec p244
    mScissorTestEnabled = 0;
    mDepthTestEnabled = 0;
    mStencilTestEnabled = 0;

    // initialize some GL values: we're going to get them from the GL and use them as the sizes of arrays,
    // so in case glGetIntegerv leaves them uninitialized because of a GL bug, we would have very weird crashes.
    mGLMaxVertexAttribs = 0;
    mGLMaxTextureUnits = 0;
    mGLMaxTextureSize = 0;
    mGLMaxTextureSizeLog2 = 0;
    mGLMaxCubeMapTextureSize = 0;
    mGLMaxCubeMapTextureSizeLog2 = 0;
    mGLMaxRenderbufferSize = 0;
    mGLMaxTextureImageUnits = 0;
    mGLMaxVertexTextureImageUnits = 0;
    mGLMaxVaryingVectors = 0;
    mGLMaxFragmentUniformVectors = 0;
    mGLMaxVertexUniformVectors = 0;
    mGLMaxColorAttachments = 1;
    mGLMaxDrawBuffers = 1;
    mGLMaxTransformFeedbackSeparateAttribs = 0;
    mGLMaxUniformBufferBindings = 0;
    mGLMax3DTextureSize = 0;
    mGLMaxArrayTextureLayers = 0;

    // See OpenGL ES 2.0.25 spec, 6.2 State Tables, table 6.13
    mPixelStorePackAlignment = 4;
    mPixelStoreUnpackAlignment = 4;

    if (NS_IsMainThread()) {
        // XXX mtseng: bug 709490, not thread safe
        WebGLMemoryTracker::AddWebGLContext(this);
    }

    mAllowContextRestore = true;
    mLastLossWasSimulated = false;
    mContextLossHandler = new WebGLContextLossHandler(this);
    mContextStatus = ContextNotLost;
    mLoseContextOnMemoryPressure = false;
    mCanLoseContextInForeground = true;
    mRestoreWhenVisible = false;

    mAlreadyGeneratedWarnings = 0;
    mAlreadyWarnedAboutFakeVertexAttrib0 = false;
    mAlreadyWarnedAboutViewportLargerThanDest = false;

    mMaxWarnings = gfxPrefs::WebGLMaxWarningsPerContext();
    if (mMaxWarnings < -1) {
        GenerateWarning("webgl.max-warnings-per-context size is too large (seems like a negative value wrapped)");
        mMaxWarnings = 0;
    }

    mLastUseIndex = 0;

    InvalidateBufferFetching();

    mBackbufferNeedsClear = true;

    mDisableFragHighP = false;

    mDrawCallsSinceLastFlush = 0;
}

WebGLContext::~WebGLContext()
{
    RemovePostRefreshObserver();

    DestroyResourcesAndContext();
    if (NS_IsMainThread()) {
        // XXX mtseng: bug 709490, not thread safe
        WebGLMemoryTracker::RemoveWebGLContext(this);
    }

    mContextLossHandler->DisableTimer();
    mContextLossHandler = nullptr;
}

void
WebGLContext::DestroyResourcesAndContext()
{
    if (!gl)
        return;

    gl->MakeCurrent();

    mBound2DTextures.Clear();
    mBoundCubeMapTextures.Clear();
    mBound3DTextures.Clear();
    mBoundSamplers.Clear();
    mBoundArrayBuffer = nullptr;
    mBoundCopyReadBuffer = nullptr;
    mBoundCopyWriteBuffer = nullptr;
    mBoundPixelPackBuffer = nullptr;
    mBoundPixelUnpackBuffer = nullptr;
    mBoundTransformFeedbackBuffer = nullptr;
    mBoundUniformBuffer = nullptr;
    mCurrentProgram = nullptr;
    mActiveProgramLinkInfo = nullptr;
    mBoundDrawFramebuffer = nullptr;
    mBoundReadFramebuffer = nullptr;
    mActiveOcclusionQuery = nullptr;
    mBoundRenderbuffer = nullptr;
    mBoundVertexArray = nullptr;
    mDefaultVertexArray = nullptr;
    mBoundTransformFeedback = nullptr;
    mDefaultTransformFeedback = nullptr;

    mBoundTransformFeedbackBuffers.Clear();
    mBoundUniformBuffers.Clear();

    while (!mTextures.isEmpty())
        mTextures.getLast()->DeleteOnce();
    while (!mVertexArrays.isEmpty())
        mVertexArrays.getLast()->DeleteOnce();
    while (!mBuffers.isEmpty())
        mBuffers.getLast()->DeleteOnce();
    while (!mRenderbuffers.isEmpty())
        mRenderbuffers.getLast()->DeleteOnce();
    while (!mFramebuffers.isEmpty())
        mFramebuffers.getLast()->DeleteOnce();
    while (!mShaders.isEmpty())
        mShaders.getLast()->DeleteOnce();
    while (!mPrograms.isEmpty())
        mPrograms.getLast()->DeleteOnce();
    while (!mQueries.isEmpty())
        mQueries.getLast()->DeleteOnce();
    while (!mSamplers.isEmpty())
        mSamplers.getLast()->DeleteOnce();
    while (!mTransformFeedbacks.isEmpty())
        mTransformFeedbacks.getLast()->DeleteOnce();

    mBlackOpaqueTexture2D = nullptr;
    mBlackOpaqueTextureCubeMap = nullptr;
    mBlackTransparentTexture2D = nullptr;
    mBlackTransparentTextureCubeMap = nullptr;

    if (mFakeVertexAttrib0BufferObject)
        gl->fDeleteBuffers(1, &mFakeVertexAttrib0BufferObject);

    // disable all extensions except "WEBGL_lose_context". see bug #927969
    // spec: http://www.khronos.org/registry/webgl/specs/latest/1.0/#5.15.2
    for (size_t i = 0; i < size_t(WebGLExtensionID::Max); ++i) {
        WebGLExtensionID extension = WebGLExtensionID(i);

        if (!IsExtensionEnabled(extension) || (extension == WebGLExtensionID::WEBGL_lose_context))
            continue;

        mExtensions[extension]->MarkLost();
        mExtensions[extension] = nullptr;
    }

    // We just got rid of everything, so the context had better
    // have been going away.
#ifdef DEBUG
    if (gl->DebugMode())
        printf_stderr("--- WebGL context destroyed: %p\n", gl.get());
#endif

    gl = nullptr;
}

void
WebGLContext::Invalidate()
{
    if (!mCanvasElement)
        return;

    mCapturedFrameInvalidated = true;

    if (mInvalidated)
        return;

    nsSVGEffects::InvalidateDirectRenderingObservers(mCanvasElement);

    mInvalidated = true;
    mCanvasElement->InvalidateCanvasContent(nullptr);
}

void
WebGLContext::OnVisibilityChange()
{
    if (!IsContextLost()) {
        return;
    }

    if (!mRestoreWhenVisible || mLastLossWasSimulated) {
        return;
    }

    ForceRestoreContext();
}

void
WebGLContext::OnMemoryPressure()
{
    bool shouldLoseContext = mLoseContextOnMemoryPressure;

    if (!mCanLoseContextInForeground &&
        ProcessPriorityManager::CurrentProcessIsForeground())
    {
        shouldLoseContext = false;
    }

    if (shouldLoseContext)
        ForceLoseContext();
}

//
// nsICanvasRenderingContextInternal
//

NS_IMETHODIMP
WebGLContext::SetContextOptions(JSContext* cx, JS::Handle<JS::Value> options,
                                ErrorResult& aRvForDictionaryInit)
{
    if (options.isNullOrUndefined() && mOptionsFrozen)
        return NS_OK;

    WebGLContextAttributes attributes;
    if (!attributes.Init(cx, options)) {
      aRvForDictionaryInit.Throw(NS_ERROR_UNEXPECTED);
      return NS_ERROR_UNEXPECTED;
    }

    WebGLContextOptions newOpts;

    newOpts.stencil = attributes.mStencil;
    newOpts.depth = attributes.mDepth;
    newOpts.premultipliedAlpha = attributes.mPremultipliedAlpha;
    newOpts.antialias = attributes.mAntialias;
    newOpts.preserveDrawingBuffer = attributes.mPreserveDrawingBuffer;
    newOpts.failIfMajorPerformanceCaveat = attributes.mFailIfMajorPerformanceCaveat;

    if (attributes.mAlpha.WasPassed())
        newOpts.alpha = attributes.mAlpha.Value();

    // Don't do antialiasing if we've disabled MSAA.
    if (!gfxPrefs::MSAALevel())
      newOpts.antialias = false;

#if 0
    GenerateWarning("aaHint: %d stencil: %d depth: %d alpha: %d premult: %d preserve: %d\n",
               newOpts.antialias ? 1 : 0,
               newOpts.stencil ? 1 : 0,
               newOpts.depth ? 1 : 0,
               newOpts.alpha ? 1 : 0,
               newOpts.premultipliedAlpha ? 1 : 0,
               newOpts.preserveDrawingBuffer ? 1 : 0);
#endif

    if (mOptionsFrozen && newOpts != mOptions) {
        // Error if the options are already frozen, and the ones that were asked for
        // aren't the same as what they were originally.
        return NS_ERROR_FAILURE;
    }

    mOptions = newOpts;
    return NS_OK;
}

int32_t
WebGLContext::GetWidth() const
{
    return mWidth;
}

int32_t
WebGLContext::GetHeight() const
{
    return mHeight;
}

/* So there are a number of points of failure here. We might fail based
 * on EGL vs. WGL, or we might fail to alloc a too-large size, or we
 * might not be able to create a context with a certain combo of context
 * creation attribs.
 *
 * We don't want to test the complete fallback matrix. (for now, at
 * least) Instead, attempt creation in this order:
 * 1. By platform API. (e.g. EGL vs. WGL)
 * 2. By context creation attribs.
 * 3. By size.
 *
 * That is, try to create headless contexts based on the platform API.
 * Next, create dummy-sized backbuffers for the contexts with the right
 * caps. Finally, resize the backbuffer to an acceptable size given the
 * requested size.
 */

static bool
IsFeatureInBlacklist(const nsCOMPtr<nsIGfxInfo>& gfxInfo, int32_t feature)
{
    int32_t status;
    if (!NS_SUCCEEDED(gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo, feature, &status)))
        return false;

    return status != nsIGfxInfo::FEATURE_STATUS_OK;
}

static bool
HasAcceleratedLayers(const nsCOMPtr<nsIGfxInfo>& gfxInfo)
{
    int32_t status;

    gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo,
                                         nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS,
                                         &status);
    if (status)
        return true;
    gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo,
                                         nsIGfxInfo::FEATURE_DIRECT3D_10_LAYERS,
                                         &status);
    if (status)
        return true;
    gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo,
                                         nsIGfxInfo::FEATURE_DIRECT3D_10_1_LAYERS,
                                         &status);
    if (status)
        return true;
    gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo,
                                         nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS,
                                         &status);
    if (status)
        return true;
    gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo,
                                         nsIGfxInfo::FEATURE_OPENGL_LAYERS,
                                         &status);
    if (status)
        return true;

    return false;
}

static void
PopulateCapFallbackQueue(const gl::SurfaceCaps& baseCaps,
                         std::queue<gl::SurfaceCaps>* out_fallbackCaps)
{
    out_fallbackCaps->push(baseCaps);

    // Dropping antialias drops our quality, but not our correctness.
    // The user basically doesn't have to handle if this fails, they
    // just get reduced quality.
    if (baseCaps.antialias) {
        gl::SurfaceCaps nextCaps(baseCaps);
        nextCaps.antialias = false;
        PopulateCapFallbackQueue(nextCaps, out_fallbackCaps);
    }

    // If we have to drop one of depth or stencil, we'd prefer to keep
    // depth. However, the client app will need to handle if this
    // doesn't work.
    if (baseCaps.stencil) {
        gl::SurfaceCaps nextCaps(baseCaps);
        nextCaps.stencil = false;
        PopulateCapFallbackQueue(nextCaps, out_fallbackCaps);
    }

    if (baseCaps.depth) {
        gl::SurfaceCaps nextCaps(baseCaps);
        nextCaps.depth = false;
        PopulateCapFallbackQueue(nextCaps, out_fallbackCaps);
    }
}

static gl::SurfaceCaps
BaseCaps(const WebGLContextOptions& options, WebGLContext* webgl)
{
    gl::SurfaceCaps baseCaps;

    baseCaps.color = true;
    baseCaps.alpha = options.alpha;
    baseCaps.antialias = options.antialias;
    baseCaps.depth = options.depth;
    baseCaps.premultAlpha = options.premultipliedAlpha;
    baseCaps.preserve = options.preserveDrawingBuffer;
    baseCaps.stencil = options.stencil;

    if (!baseCaps.alpha)
        baseCaps.premultAlpha = true;

    // we should really have this behind a
    // |gfxPlatform::GetPlatform()->GetScreenDepth() == 16| check, but
    // for now it's just behind a pref for testing/evaluation.
    baseCaps.bpp16 = gfxPrefs::WebGLPrefer16bpp();

#ifdef MOZ_WIDGET_GONK
    do {
        auto canvasElement = webgl->GetCanvas();
        if (!canvasElement)
            break;

        auto ownerDoc = canvasElement->OwnerDoc();
        nsIWidget* docWidget = nsContentUtils::WidgetForDocument(ownerDoc);
        if (!docWidget)
            break;

        layers::LayerManager* layerManager = docWidget->GetLayerManager();
        if (!layerManager)
            break;

        // XXX we really want "AsSurfaceAllocator" here for generality
        layers::ShadowLayerForwarder* forwarder = layerManager->AsShadowForwarder();
        if (!forwarder)
            break;

        baseCaps.surfaceAllocator = static_cast<layers::ISurfaceAllocator*>(forwarder);
    } while (false);
#endif

    // Done with baseCaps construction.

    bool forceAllowAA = gfxPrefs::WebGLForceMSAA();
    nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
    if (!forceAllowAA &&
        IsFeatureInBlacklist(gfxInfo, nsIGfxInfo::FEATURE_WEBGL_MSAA))
    {
        webgl->GenerateWarning("Disallowing antialiased backbuffers due"
                               " to blacklisting.");
        baseCaps.antialias = false;
    }

    return baseCaps;
}

////////////////////////////////////////

static already_AddRefed<gl::GLContext>
CreateGLWithEGL(const gl::SurfaceCaps& caps, gl::CreateContextFlags flags,
                WebGLContext* webgl)
{
    RefPtr<GLContext> gl;
#ifndef XP_MACOSX // Mac doesn't have GLContextProviderEGL.
    gfx::IntSize dummySize(16, 16);
    gl = gl::GLContextProviderEGL::CreateOffscreen(dummySize, caps,
                                                                     flags);
    if (!gl) {
        webgl->GenerateWarning("Error during EGL OpenGL init.");
        return nullptr;
    }

    if (gl->IsANGLE())
        return nullptr;
#endif // XP_MACOSX
    return gl.forget();
}

static already_AddRefed<GLContext>
CreateGLWithANGLE(const gl::SurfaceCaps& caps, gl::CreateContextFlags flags,
                  WebGLContext* webgl)
{
    RefPtr<GLContext> gl;

#ifdef XP_WIN
    gfx::IntSize dummySize(16, 16);
    gl = gl::GLContextProviderEGL::CreateOffscreen(dummySize, caps, flags);
    if (!gl) {
        webgl->GenerateWarning("Error during ANGLE OpenGL init.");
        return nullptr;
    }

    if (!gl->IsANGLE())
        return nullptr;
#endif

    return gl.forget();
}

static already_AddRefed<gl::GLContext>
CreateGLWithDefault(const gl::SurfaceCaps& caps, gl::CreateContextFlags flags,
                    WebGLContext* webgl)
{
    nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();

    if (!(flags & CreateContextFlags::FORCE_ENABLE_HARDWARE) &&
        IsFeatureInBlacklist(gfxInfo, nsIGfxInfo::FEATURE_WEBGL_OPENGL))
    {
        webgl->GenerateWarning("Refused to create native OpenGL context because of"
                               " blacklisting.");
        return nullptr;
    }

    gfx::IntSize dummySize(16, 16);
    RefPtr<GLContext> gl = gl::GLContextProvider::CreateOffscreen(dummySize, caps, flags);
    if (!gl) {
        webgl->GenerateWarning("Error during native OpenGL init.");
        return nullptr;
    }

    if (gl->IsANGLE())
        return nullptr;

    return gl.forget();
}

////////////////////////////////////////

bool
WebGLContext::CreateAndInitGLWith(FnCreateGL_T fnCreateGL,
                                  const gl::SurfaceCaps& baseCaps,
                                  gl::CreateContextFlags flags)
{
    MOZ_ASSERT(!gl);

    std::queue<gl::SurfaceCaps> fallbackCaps;
    PopulateCapFallbackQueue(baseCaps, &fallbackCaps);

    gl = nullptr;
    while (!fallbackCaps.empty()) {
        gl::SurfaceCaps& caps = fallbackCaps.front();

        gl = fnCreateGL(caps, flags, this);
        if (gl)
            break;

        fallbackCaps.pop();
    }
    if (!gl)
        return false;

    if (!InitAndValidateGL()) {
        gl = nullptr;
        return false;
    }

    return true;
}

bool
WebGLContext::CreateAndInitGL(bool forceEnabled)
{
    bool preferEGL = PR_GetEnv("MOZ_WEBGL_PREFER_EGL");
    bool disableANGLE = gfxPrefs::WebGLDisableANGLE();

    if (PR_GetEnv("MOZ_WEBGL_FORCE_OPENGL"))
        disableANGLE = true;

    gl::CreateContextFlags flags = gl::CreateContextFlags::NONE;
    if (forceEnabled) flags |= gl::CreateContextFlags::FORCE_ENABLE_HARDWARE;
    if (!IsWebGL2())  flags |= gl::CreateContextFlags::REQUIRE_COMPAT_PROFILE;

    const gl::SurfaceCaps baseCaps = BaseCaps(mOptions, this);

    MOZ_ASSERT(!gl);

    if (preferEGL) {
        if (CreateAndInitGLWith(CreateGLWithEGL, baseCaps, flags))
            return true;
    }

    MOZ_ASSERT(!gl);

    if (!disableANGLE) {
        if (CreateAndInitGLWith(CreateGLWithANGLE, baseCaps, flags))
            return true;
    }

    MOZ_ASSERT(!gl);

    if (CreateAndInitGLWith(CreateGLWithDefault, baseCaps, flags))
        return true;

    MOZ_ASSERT(!gl);
    gl = nullptr;

    return false;
}

// Fallback for resizes:
bool
WebGLContext::ResizeBackbuffer(uint32_t requestedWidth,
                               uint32_t requestedHeight)
{
    uint32_t width = requestedWidth;
    uint32_t height = requestedHeight;

    bool resized = false;
    while (width || height) {
      width = width ? width : 1;
      height = height ? height : 1;

      gfx::IntSize curSize(width, height);
      if (gl->ResizeOffscreen(curSize)) {
          resized = true;
          break;
      }

      width /= 2;
      height /= 2;
    }

    if (!resized)
        return false;

    mWidth = gl->OffscreenSize().width;
    mHeight = gl->OffscreenSize().height;
    MOZ_ASSERT((uint32_t)mWidth == width);
    MOZ_ASSERT((uint32_t)mHeight == height);

    if (width != requestedWidth ||
        height != requestedHeight)
    {
        GenerateWarning("Requested size %dx%d was too large, but resize"
                          " to %dx%d succeeded.",
                        requestedWidth, requestedHeight,
                        width, height);
    }
    return true;
}

NS_IMETHODIMP
WebGLContext::SetDimensions(int32_t signedWidth, int32_t signedHeight)
{
    if (signedWidth < 0 || signedHeight < 0) {
        GenerateWarning("Canvas size is too large (seems like a negative value wrapped)");
        return NS_ERROR_OUT_OF_MEMORY;
    }

    uint32_t width = signedWidth;
    uint32_t height = signedHeight;

    // Early success return cases

    // May have a OffscreenCanvas instead of an HTMLCanvasElement
    if (GetCanvas())
        GetCanvas()->InvalidateCanvas();

    // Zero-sized surfaces can cause problems.
    if (width == 0)
        width = 1;

    if (height == 0)
        height = 1;

    // If we already have a gl context, then we just need to resize it
    if (gl) {
        if ((uint32_t)mWidth == width &&
            (uint32_t)mHeight == height)
        {
            return NS_OK;
        }

        if (IsContextLost())
            return NS_OK;

        MakeContextCurrent();

        // If we've already drawn, we should commit the current buffer.
        PresentScreenBuffer();

        if (IsContextLost()) {
            GenerateWarning("WebGL context was lost due to swap failure.");
            return NS_OK;
        }

        // ResizeOffscreen scraps the current prod buffer before making a new one.
        if (!ResizeBackbuffer(width, height)) {
            GenerateWarning("WebGL context failed to resize.");
            ForceLoseContext();
            return NS_OK;
        }

        // everything's good, we're done here
        mResetLayer = true;
        mBackbufferNeedsClear = true;

        return NS_OK;
    }

    // End of early return cases.
    // At this point we know that we're not just resizing an existing context,
    // we are initializing a new context.

    // if we exceeded either the global or the per-principal limit for WebGL contexts,
    // lose the oldest-used context now to free resources. Note that we can't do that
    // in the WebGLContext constructor as we don't have a canvas element yet there.
    // Here is the right place to do so, as we are about to create the OpenGL context
    // and that is what can fail if we already have too many.
    LoseOldestWebGLContextIfLimitExceeded();

    // We're going to create an entirely new context.  If our
    // generation is not 0 right now (that is, if this isn't the first
    // context we're creating), we may have to dispatch a context lost
    // event.

    // If incrementing the generation would cause overflow,
    // don't allow it.  Allowing this would allow us to use
    // resource handles created from older context generations.
    if (!(mGeneration + 1).isValid()) {
        GenerateWarning("Too many WebGL contexts created this run.");
        return NS_ERROR_FAILURE; // exit without changing the value of mGeneration
    }

    // increment the generation number - Do this early because later
    // in CreateOffscreenGL(), "default" objects are created that will
    // pick up the old generation.
    ++mGeneration;

    bool disabled = gfxPrefs::WebGLDisabled();

    // TODO: When we have software webgl support we should use that instead.
    disabled |= gfxPlatform::InSafeMode();

    if (disabled) {
        GenerateWarning("WebGL creation is disabled, and so disallowed here.");
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
    bool failIfMajorPerformanceCaveat =
                    !gfxPrefs::WebGLDisableFailIfMajorPerformanceCaveat() &&
                    !HasAcceleratedLayers(gfxInfo);
    if (failIfMajorPerformanceCaveat) {
        dom::Nullable<dom::WebGLContextAttributes> contextAttributes;
        this->GetContextAttributes(contextAttributes);
        if (contextAttributes.Value().mFailIfMajorPerformanceCaveat) {
            return NS_ERROR_FAILURE;
        }
    }

    // Alright, now let's start trying.
    bool forceEnabled = gfxPrefs::WebGLForceEnabled();
    ScopedGfxFeatureReporter reporter("WebGL", forceEnabled);

    MOZ_ASSERT(!gl);
    if (!CreateAndInitGL(forceEnabled)) {
        GenerateWarning("WebGL creation failed.");
        return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(gl);

    MOZ_ASSERT_IF(mOptions.alpha, gl->Caps().alpha);

    if (!ResizeBackbuffer(width, height)) {
        GenerateWarning("Initializing WebGL backbuffer failed.");
        return NS_ERROR_FAILURE;
    }

#ifdef DEBUG
    if (gl->DebugMode())
        printf_stderr("--- WebGL context created: %p\n", gl.get());
#endif

    mResetLayer = true;
    mOptionsFrozen = true;

    // Update our internal stuff:
    if (gl->WorkAroundDriverBugs()) {
        if (!mOptions.alpha && gl->Caps().alpha)
            mNeedsFakeNoAlpha = true;

        if (!mOptions.depth && gl->Caps().depth)
            mNeedsFakeNoDepth = true;

        if (!mOptions.stencil && gl->Caps().stencil)
            mNeedsFakeNoStencil = true;
    }

    // Update mOptions.
    if (!gl->Caps().depth)
        mOptions.depth = false;

    if (!gl->Caps().stencil)
        mOptions.stencil = false;

    mOptions.antialias = gl->Caps().antialias;

    MakeContextCurrent();

    gl->fViewport(0, 0, mWidth, mHeight);
    mViewportWidth = mWidth;
    mViewportHeight = mHeight;

    gl->fScissor(0, 0, mWidth, mHeight);

    // Make sure that we clear this out, otherwise
    // we'll end up displaying random memory
    gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

    AssertCachedBindings();
    AssertCachedState();

    // Clear immediately, because we need to present the cleared initial
    // buffer.
    mBackbufferNeedsClear = true;
    ClearBackbufferIfNeeded();

    mShouldPresent = true;

    MOZ_ASSERT(gl->Caps().color);

    MOZ_ASSERT_IF(!mNeedsFakeNoAlpha, gl->Caps().alpha == mOptions.alpha);
    MOZ_ASSERT_IF(mNeedsFakeNoAlpha, !mOptions.alpha && gl->Caps().alpha);

    MOZ_ASSERT_IF(!mNeedsFakeNoDepth, gl->Caps().depth == mOptions.depth);
    MOZ_ASSERT_IF(mNeedsFakeNoDepth, !mOptions.depth && gl->Caps().depth);

    MOZ_ASSERT_IF(!mNeedsFakeNoStencil, gl->Caps().stencil == mOptions.stencil);
    MOZ_ASSERT_IF(mNeedsFakeNoStencil, !mOptions.stencil && gl->Caps().stencil);

    MOZ_ASSERT(gl->Caps().antialias == mOptions.antialias);
    MOZ_ASSERT(gl->Caps().preserve == mOptions.preserveDrawingBuffer);

    AssertCachedBindings();
    AssertCachedState();

    reporter.SetSuccessful();
    return NS_OK;
}

void
WebGLContext::ClearBackbufferIfNeeded()
{
    if (!mBackbufferNeedsClear)
        return;

#ifdef DEBUG
    gl->MakeCurrent();

    GLuint fb = 0;
    gl->GetUIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, &fb);
    MOZ_ASSERT(fb == 0);
#endif

    ClearScreen();

    mBackbufferNeedsClear = false;
}

void
WebGLContext::LoseOldestWebGLContextIfLimitExceeded()
{
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    // some mobile devices can't have more than 8 GL contexts overall
    const size_t kMaxWebGLContextsPerPrincipal = 2;
    const size_t kMaxWebGLContexts             = 4;
#else
    const size_t kMaxWebGLContextsPerPrincipal = 16;
    const size_t kMaxWebGLContexts             = 32;
#endif
    MOZ_ASSERT(kMaxWebGLContextsPerPrincipal < kMaxWebGLContexts);

    if (!NS_IsMainThread()) {
        // XXX mtseng: bug 709490, WebGLMemoryTracker is not thread safe.
        return;
    }

    // it's important to update the index on a new context before losing old contexts,
    // otherwise new unused contexts would all have index 0 and we couldn't distinguish older ones
    // when choosing which one to lose first.
    UpdateLastUseIndex();

    WebGLMemoryTracker::ContextsArrayType& contexts = WebGLMemoryTracker::Contexts();

    // quick exit path, should cover a majority of cases
    if (contexts.Length() <= kMaxWebGLContextsPerPrincipal)
        return;

    // note that here by "context" we mean "non-lost context". See the check for
    // IsContextLost() below. Indeed, the point of this function is to maybe lose
    // some currently non-lost context.

    uint64_t oldestIndex = UINT64_MAX;
    uint64_t oldestIndexThisPrincipal = UINT64_MAX;
    const WebGLContext* oldestContext = nullptr;
    const WebGLContext* oldestContextThisPrincipal = nullptr;
    size_t numContexts = 0;
    size_t numContextsThisPrincipal = 0;

    for(size_t i = 0; i < contexts.Length(); ++i) {
        // don't want to lose ourselves.
        if (contexts[i] == this)
            continue;

        if (contexts[i]->IsContextLost())
            continue;

        if (!contexts[i]->GetCanvas()) {
            // Zombie context: the canvas is already destroyed, but something else
            // (typically the compositor) is still holding on to the context.
            // Killing zombies is a no-brainer.
            const_cast<WebGLContext*>(contexts[i])->LoseContext();
            continue;
        }

        numContexts++;
        if (contexts[i]->mLastUseIndex < oldestIndex) {
            oldestIndex = contexts[i]->mLastUseIndex;
            oldestContext = contexts[i];
        }

        nsIPrincipal* ourPrincipal = GetCanvas()->NodePrincipal();
        nsIPrincipal* theirPrincipal = contexts[i]->GetCanvas()->NodePrincipal();
        bool samePrincipal;
        nsresult rv = ourPrincipal->Equals(theirPrincipal, &samePrincipal);
        if (NS_SUCCEEDED(rv) && samePrincipal) {
            numContextsThisPrincipal++;
            if (contexts[i]->mLastUseIndex < oldestIndexThisPrincipal) {
                oldestIndexThisPrincipal = contexts[i]->mLastUseIndex;
                oldestContextThisPrincipal = contexts[i];
            }
        }
    }

    if (numContextsThisPrincipal > kMaxWebGLContextsPerPrincipal) {
        GenerateWarning("Exceeded %d live WebGL contexts for this principal, losing the "
                        "least recently used one.", kMaxWebGLContextsPerPrincipal);
        MOZ_ASSERT(oldestContextThisPrincipal); // if we reach this point, this can't be null
        const_cast<WebGLContext*>(oldestContextThisPrincipal)->LoseContext();
    } else if (numContexts > kMaxWebGLContexts) {
        GenerateWarning("Exceeded %d live WebGL contexts, losing the least recently used one.",
                        kMaxWebGLContexts);
        MOZ_ASSERT(oldestContext); // if we reach this point, this can't be null
        const_cast<WebGLContext*>(oldestContext)->LoseContext();
    }
}

UniquePtr<uint8_t[]>
WebGLContext::GetImageBuffer(int32_t* out_format)
{
    *out_format = 0;

    // Use GetSurfaceSnapshot() to make sure that appropriate y-flip gets applied
    bool premult;
    RefPtr<SourceSurface> snapshot =
      GetSurfaceSnapshot(mOptions.premultipliedAlpha ? nullptr : &premult);
    if (!snapshot) {
        return nullptr;
    }

    MOZ_ASSERT(mOptions.premultipliedAlpha || !premult, "We must get unpremult when we ask for it!");

    RefPtr<DataSourceSurface> dataSurface = snapshot->GetDataSurface();

    return gfxUtils::GetImageBuffer(dataSurface, mOptions.premultipliedAlpha,
                                    out_format);
}

NS_IMETHODIMP
WebGLContext::GetInputStream(const char* mimeType,
                             const char16_t* encoderOptions,
                             nsIInputStream** out_stream)
{
    NS_ASSERTION(gl, "GetInputStream on invalid context?");
    if (!gl)
        return NS_ERROR_FAILURE;

    // Use GetSurfaceSnapshot() to make sure that appropriate y-flip gets applied
    bool premult;
    RefPtr<SourceSurface> snapshot =
      GetSurfaceSnapshot(mOptions.premultipliedAlpha ? nullptr : &premult);
    if (!snapshot)
        return NS_ERROR_FAILURE;

    MOZ_ASSERT(mOptions.premultipliedAlpha || !premult, "We must get unpremult when we ask for it!");

    RefPtr<DataSourceSurface> dataSurface = snapshot->GetDataSurface();
    return gfxUtils::GetInputStream(dataSurface, mOptions.premultipliedAlpha, mimeType,
                                    encoderOptions, out_stream);
}

void
WebGLContext::UpdateLastUseIndex()
{
    static CheckedInt<uint64_t> sIndex = 0;

    sIndex++;

    // should never happen with 64-bit; trying to handle this would be riskier than
    // not handling it as the handler code would never get exercised.
    if (!sIndex.isValid())
        NS_RUNTIMEABORT("Can't believe it's been 2^64 transactions already!");

    mLastUseIndex = sIndex.value();
}

static uint8_t gWebGLLayerUserData;

class WebGLContextUserData : public LayerUserData
{
public:
    explicit WebGLContextUserData(HTMLCanvasElement* canvas)
        : mCanvas(canvas)
    {}

    /* PreTransactionCallback gets called by the Layers code every time the
     * WebGL canvas is going to be composited.
     */
    static void PreTransactionCallback(void* data) {
        WebGLContextUserData* userdata = static_cast<WebGLContextUserData*>(data);
        HTMLCanvasElement* canvas = userdata->mCanvas;
        WebGLContext* webgl = static_cast<WebGLContext*>(canvas->GetContextAtIndex(0));

        // Present our screenbuffer, if needed.
        webgl->PresentScreenBuffer();
        webgl->mDrawCallsSinceLastFlush = 0;
    }

    /** DidTransactionCallback gets called by the Layers code everytime the WebGL canvas gets composite,
      * so it really is the right place to put actions that have to be performed upon compositing
      */
    static void DidTransactionCallback(void* data) {
        WebGLContextUserData* userdata = static_cast<WebGLContextUserData*>(data);
        HTMLCanvasElement* canvas = userdata->mCanvas;
        WebGLContext* webgl = static_cast<WebGLContext*>(canvas->GetContextAtIndex(0));

        // Mark ourselves as no longer invalidated.
        webgl->MarkContextClean();

        webgl->UpdateLastUseIndex();
    }

private:
    RefPtr<HTMLCanvasElement> mCanvas;
};

already_AddRefed<layers::CanvasLayer>
WebGLContext::GetCanvasLayer(nsDisplayListBuilder* builder,
                             CanvasLayer* oldLayer,
                             LayerManager* manager)
{
    if (IsContextLost())
        return nullptr;

    if (!mResetLayer && oldLayer &&
        oldLayer->HasUserData(&gWebGLLayerUserData)) {
        RefPtr<layers::CanvasLayer> ret = oldLayer;
        return ret.forget();
    }

    RefPtr<CanvasLayer> canvasLayer = manager->CreateCanvasLayer();
    if (!canvasLayer) {
        NS_WARNING("CreateCanvasLayer returned null!");
        return nullptr;
    }

    WebGLContextUserData* userData = nullptr;
    if (builder->IsPaintingToWindow() && mCanvasElement) {
        // Make the layer tell us whenever a transaction finishes (including
        // the current transaction), so we can clear our invalidation state and
        // start invalidating again. We need to do this for the layer that is
        // being painted to a window (there shouldn't be more than one at a time,
        // and if there is, flushing the invalidation state more often than
        // necessary is harmless).

        // The layer will be destroyed when we tear down the presentation
        // (at the latest), at which time this userData will be destroyed,
        // releasing the reference to the element.
        // The userData will receive DidTransactionCallbacks, which flush the
        // the invalidation state to indicate that the canvas is up to date.
        userData = new WebGLContextUserData(mCanvasElement);
        canvasLayer->SetDidTransactionCallback(
            WebGLContextUserData::DidTransactionCallback, userData);
        canvasLayer->SetPreTransactionCallback(
            WebGLContextUserData::PreTransactionCallback, userData);
    }

    canvasLayer->SetUserData(&gWebGLLayerUserData, userData);

    CanvasLayer::Data data;
    data.mGLContext = gl;
    data.mSize = nsIntSize(mWidth, mHeight);
    data.mHasAlpha = gl->Caps().alpha;
    data.mIsGLAlphaPremult = IsPremultAlpha() || !data.mHasAlpha;

    canvasLayer->Initialize(data);
    uint32_t flags = gl->Caps().alpha ? 0 : Layer::CONTENT_OPAQUE;
    canvasLayer->SetContentFlags(flags);
    canvasLayer->Updated();

    mResetLayer = false;

    return canvasLayer.forget();
}

layers::LayersBackend
WebGLContext::GetCompositorBackendType() const
{
    if (mCanvasElement) {
        return mCanvasElement->GetCompositorBackendType();
    } else if (mOffscreenCanvas) {
        return mOffscreenCanvas->GetCompositorBackendType();
    }

    return LayersBackend::LAYERS_NONE;
}

void
WebGLContext::Commit()
{
    if (mOffscreenCanvas) {
        mOffscreenCanvas->CommitFrameToCompositor();
    }
}

void
WebGLContext::GetCanvas(Nullable<dom::OwningHTMLCanvasElementOrOffscreenCanvas>& retval)
{
    if (mCanvasElement) {
        MOZ_RELEASE_ASSERT(!mOffscreenCanvas);
        retval.SetValue().SetAsHTMLCanvasElement() = mCanvasElement;
    } else if (mOffscreenCanvas) {
        retval.SetValue().SetAsOffscreenCanvas() = mOffscreenCanvas;
    } else {
        retval.SetNull();
    }
}

void
WebGLContext::GetContextAttributes(dom::Nullable<dom::WebGLContextAttributes>& retval)
{
    retval.SetNull();
    if (IsContextLost())
        return;

    dom::WebGLContextAttributes& result = retval.SetValue();

    result.mAlpha.Construct(mOptions.alpha);
    result.mDepth = mOptions.depth;
    result.mStencil = mOptions.stencil;
    result.mAntialias = mOptions.antialias;
    result.mPremultipliedAlpha = mOptions.premultipliedAlpha;
    result.mPreserveDrawingBuffer = mOptions.preserveDrawingBuffer;
    result.mFailIfMajorPerformanceCaveat = mOptions.failIfMajorPerformanceCaveat;
}

NS_IMETHODIMP
WebGLContext::MozGetUnderlyingParamString(uint32_t pname, nsAString& retval)
{
    if (IsContextLost())
        return NS_OK;

    retval.SetIsVoid(true);

    MakeContextCurrent();

    switch (pname) {
    case LOCAL_GL_VENDOR:
    case LOCAL_GL_RENDERER:
    case LOCAL_GL_VERSION:
    case LOCAL_GL_SHADING_LANGUAGE_VERSION:
    case LOCAL_GL_EXTENSIONS:
        {
            const char* s = (const char*)gl->fGetString(pname);
            retval.Assign(NS_ConvertASCIItoUTF16(nsDependentCString(s)));
            break;
        }

    default:
        return NS_ERROR_INVALID_ARG;
    }

    return NS_OK;
}

void
WebGLContext::ClearScreen()
{
    bool colorAttachmentsMask[WebGLContext::kMaxColorAttachments] = {false};

    MakeContextCurrent();
    ScopedBindFramebuffer autoFB(gl, 0);

    GLbitfield clearMask = LOCAL_GL_COLOR_BUFFER_BIT;
    if (mOptions.depth)
        clearMask |= LOCAL_GL_DEPTH_BUFFER_BIT;
    if (mOptions.stencil)
        clearMask |= LOCAL_GL_STENCIL_BUFFER_BIT;

    colorAttachmentsMask[0] = true;

    ForceClearFramebufferWithDefaultValues(mNeedsFakeNoAlpha, clearMask,
                                           colorAttachmentsMask);
}

void
WebGLContext::ForceClearFramebufferWithDefaultValues(bool fakeNoAlpha, GLbitfield mask,
                                                     const bool colorAttachmentsMask[kMaxColorAttachments])
{
    MakeContextCurrent();

    bool initializeColorBuffer = 0 != (mask & LOCAL_GL_COLOR_BUFFER_BIT);
    bool initializeDepthBuffer = 0 != (mask & LOCAL_GL_DEPTH_BUFFER_BIT);
    bool initializeStencilBuffer = 0 != (mask & LOCAL_GL_STENCIL_BUFFER_BIT);
    bool drawBuffersIsEnabled = IsExtensionEnabled(WebGLExtensionID::WEBGL_draw_buffers);
    bool shouldOverrideDrawBuffers = false;
    bool usingDefaultFrameBuffer = !mBoundDrawFramebuffer;

    GLenum currentDrawBuffers[WebGLContext::kMaxColorAttachments];

    // Fun GL fact: No need to worry about the viewport here, glViewport is just
    // setting up a coordinates transformation, it doesn't affect glClear at all.
    AssertCachedState(); // Can't check cached bindings, as we could
                         // have a different FB bound temporarily.

    // Prepare GL state for clearing.
    gl->fDisable(LOCAL_GL_SCISSOR_TEST);

    if (initializeColorBuffer) {

        if (drawBuffersIsEnabled) {

            GLenum drawBuffersCommand[WebGLContext::kMaxColorAttachments] = { LOCAL_GL_NONE };

            for (int32_t i = 0; i < mGLMaxDrawBuffers; i++) {
                GLint temp;
                gl->fGetIntegerv(LOCAL_GL_DRAW_BUFFER0 + i, &temp);
                currentDrawBuffers[i] = temp;

                if (colorAttachmentsMask[i]) {
                    drawBuffersCommand[i] = LOCAL_GL_COLOR_ATTACHMENT0 + i;
                }
                if (currentDrawBuffers[i] != drawBuffersCommand[i])
                    shouldOverrideDrawBuffers = true;
            }

            // When clearing the default framebuffer, we must be clearing only
            // GL_BACK, and nothing else, or else gl may return an error. We will
            // only use the first element of currentDrawBuffers in this case.
            if (usingDefaultFrameBuffer) {
                gl->Screen()->SetDrawBuffer(LOCAL_GL_BACK);
                if (currentDrawBuffers[0] == LOCAL_GL_COLOR_ATTACHMENT0)
                    currentDrawBuffers[0] = LOCAL_GL_BACK;
                shouldOverrideDrawBuffers = false;
            }
            // calling draw buffers can cause resolves on adreno drivers so
            // we try to avoid calling it
            if (shouldOverrideDrawBuffers)
                gl->fDrawBuffers(mGLMaxDrawBuffers, drawBuffersCommand);
        }

        gl->fColorMask(1, 1, 1, 1);

        if (fakeNoAlpha) {
            gl->fClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        } else {
            gl->fClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        }
    }

    if (initializeDepthBuffer) {
        gl->fDepthMask(1);
        gl->fClearDepth(1.0f);
    }

    if (initializeStencilBuffer) {
        // "The clear operation always uses the front stencil write mask
        //  when clearing the stencil buffer."
        gl->fStencilMaskSeparate(LOCAL_GL_FRONT, 0xffffffff);
        gl->fStencilMaskSeparate(LOCAL_GL_BACK,  0xffffffff);
        gl->fClearStencil(0);
    }

    if (mRasterizerDiscardEnabled) {
        gl->fDisable(LOCAL_GL_RASTERIZER_DISCARD);
    }

    // Do the clear!
    gl->fClear(mask);

    // And reset!
    if (mScissorTestEnabled)
        gl->fEnable(LOCAL_GL_SCISSOR_TEST);

    if (mRasterizerDiscardEnabled) {
        gl->fEnable(LOCAL_GL_RASTERIZER_DISCARD);
    }

    // Restore GL state after clearing.
    if (initializeColorBuffer) {

        if (drawBuffersIsEnabled) {
            if (usingDefaultFrameBuffer) {
                gl->Screen()->SetDrawBuffer(currentDrawBuffers[0]);
            } else if (shouldOverrideDrawBuffers) {
                gl->fDrawBuffers(mGLMaxDrawBuffers, currentDrawBuffers);
            }
        }

        gl->fColorMask(mColorWriteMask[0],
                       mColorWriteMask[1],
                       mColorWriteMask[2],
                       mColorWriteMask[3]);
        gl->fClearColor(mColorClearValue[0],
                        mColorClearValue[1],
                        mColorClearValue[2],
                        mColorClearValue[3]);
    }

    if (initializeDepthBuffer) {
        gl->fDepthMask(mDepthWriteMask);
        gl->fClearDepth(mDepthClearValue);
    }

    if (initializeStencilBuffer) {
        gl->fStencilMaskSeparate(LOCAL_GL_FRONT, mStencilWriteMaskFront);
        gl->fStencilMaskSeparate(LOCAL_GL_BACK,  mStencilWriteMaskBack);
        gl->fClearStencil(mStencilClearValue);
    }
}

// For an overview of how WebGL compositing works, see:
// https://wiki.mozilla.org/Platform/GFX/WebGL/Compositing
bool
WebGLContext::PresentScreenBuffer()
{
    if (IsContextLost()) {
        return false;
    }

    if (!mShouldPresent) {
        return false;
    }
    MOZ_ASSERT(!mBackbufferNeedsClear);

    gl->MakeCurrent();

    GLScreenBuffer* screen = gl->Screen();
    MOZ_ASSERT(screen);

    if (!screen->PublishFrame(screen->Size())) {
        ForceLoseContext();
        return false;
    }

    if (!mOptions.preserveDrawingBuffer) {
        mBackbufferNeedsClear = true;
    }

    mShouldPresent = false;

    return true;
}

void
WebGLContext::DummyFramebufferOperation(const char* funcName)
{
    FBStatus status = CheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
        ErrorInvalidFramebufferOperation("%s: incomplete framebuffer",
                                         funcName);
    }
}

static bool
CheckContextLost(GLContext* gl, bool* const out_isGuilty)
{
    MOZ_ASSERT(gl);
    MOZ_ASSERT(out_isGuilty);

    bool isEGL = gl->GetContextType() == gl::GLContextType::EGL;

    GLenum resetStatus = LOCAL_GL_NO_ERROR;
    if (gl->HasRobustness()) {
        gl->MakeCurrent();
        resetStatus = gl->fGetGraphicsResetStatus();
    } else if (isEGL) {
        // Simulate a ARB_robustness guilty context loss for when we
        // get an EGL_CONTEXT_LOST error. It may not actually be guilty,
        // but we can't make any distinction.
        if (!gl->MakeCurrent(true) && gl->IsContextLost()) {
            resetStatus = LOCAL_GL_UNKNOWN_CONTEXT_RESET_ARB;
        }
    }

    if (resetStatus == LOCAL_GL_NO_ERROR) {
        *out_isGuilty = false;
        return false;
    }

    // Assume guilty unless we find otherwise!
    bool isGuilty = true;
    switch (resetStatus) {
    case LOCAL_GL_INNOCENT_CONTEXT_RESET_ARB:
        // Either nothing wrong, or not our fault.
        isGuilty = false;
        break;
    case LOCAL_GL_GUILTY_CONTEXT_RESET_ARB:
        NS_WARNING("WebGL content on the page definitely caused the graphics"
                   " card to reset.");
        break;
    case LOCAL_GL_UNKNOWN_CONTEXT_RESET_ARB:
        NS_WARNING("WebGL content on the page might have caused the graphics"
                   " card to reset");
        // If we can't tell, assume guilty.
        break;
    default:
        MOZ_ASSERT(false, "Unreachable.");
        // If we do get here, let's pretend to be guilty as an escape plan.
        break;
    }

    if (isGuilty) {
        NS_WARNING("WebGL context on this page is considered guilty, and will"
                   " not be restored.");
    }

    *out_isGuilty = isGuilty;
    return true;
}

bool
WebGLContext::TryToRestoreContext()
{
    if (NS_FAILED(SetDimensions(mWidth, mHeight)))
        return false;

    return true;
}

void
WebGLContext::RunContextLossTimer()
{
    mContextLossHandler->RunTimer();
}

class UpdateContextLossStatusTask : public nsCancelableRunnable
{
    RefPtr<WebGLContext> mWebGL;

public:
    explicit UpdateContextLossStatusTask(WebGLContext* webgl)
        : mWebGL(webgl)
    {
    }

    NS_IMETHOD Run() {
        if (mWebGL)
            mWebGL->UpdateContextLossStatus();

        return NS_OK;
    }

    NS_IMETHOD Cancel() {
        mWebGL = nullptr;
        return NS_OK;
    }
};

void
WebGLContext::EnqueueUpdateContextLossStatus()
{
    nsCOMPtr<nsIRunnable> task = new UpdateContextLossStatusTask(this);
    NS_DispatchToCurrentThread(task);
}

// We use this timer for many things. Here are the things that it is activated for:
// 1) If a script is using the MOZ_WEBGL_lose_context extension.
// 2) If we are using EGL and _NOT ANGLE_, we query periodically to see if the
//    CONTEXT_LOST_WEBGL error has been triggered.
// 3) If we are using ANGLE, or anything that supports ARB_robustness, query the
//    GPU periodically to see if the reset status bit has been set.
// In all of these situations, we use this timer to send the script context lost
// and restored events asynchronously. For example, if it triggers a context loss,
// the webglcontextlost event will be sent to it the next time the robustness timer
// fires.
// Note that this timer mechanism is not used unless one of these 3 criteria
// are met.
// At a bare minimum, from context lost to context restores, it would take 3
// full timer iterations: detection, webglcontextlost, webglcontextrestored.
void
WebGLContext::UpdateContextLossStatus()
{
    if (!mCanvasElement && !mOffscreenCanvas) {
        // the canvas is gone. That happens when the page was closed before we got
        // this timer event. In this case, there's nothing to do here, just don't crash.
        return;
    }
    if (mContextStatus == ContextNotLost) {
        // We don't know that we're lost, but we might be, so we need to
        // check. If we're guilty, don't allow restores, though.

        bool isGuilty = true;
        MOZ_ASSERT(gl); // Shouldn't be missing gl if we're NotLost.
        bool isContextLost = CheckContextLost(gl, &isGuilty);

        if (isContextLost) {
            if (isGuilty)
                mAllowContextRestore = false;

            ForceLoseContext();
        }

        // Fall through.
    }

    if (mContextStatus == ContextLostAwaitingEvent) {
        // The context has been lost and we haven't yet triggered the
        // callback, so do that now.

        bool useDefaultHandler;

        if (mCanvasElement) {
            nsContentUtils::DispatchTrustedEvent(
                mCanvasElement->OwnerDoc(),
                static_cast<nsIDOMHTMLCanvasElement*>(mCanvasElement),
                NS_LITERAL_STRING("webglcontextlost"),
                true,
                true,
                &useDefaultHandler);
        } else {
            // OffscreenCanvas case
            RefPtr<Event> event = new Event(mOffscreenCanvas, nullptr, nullptr);
            event->InitEvent(NS_LITERAL_STRING("webglcontextlost"), true, true);
            event->SetTrusted(true);
            mOffscreenCanvas->DispatchEvent(event, &useDefaultHandler);
        }

        // We sent the callback, so we're just 'regular lost' now.
        mContextStatus = ContextLost;
        // If we're told to use the default handler, it means the script
        // didn't bother to handle the event. In this case, we shouldn't
        // auto-restore the context.
        if (useDefaultHandler)
            mAllowContextRestore = false;

        // Fall through.
    }

    if (mContextStatus == ContextLost) {
        // Context is lost, and we've already sent the callback. We
        // should try to restore the context if we're both allowed to,
        // and supposed to.

        // Are we allowed to restore the context?
        if (!mAllowContextRestore)
            return;

        // If we're only simulated-lost, we shouldn't auto-restore, and
        // instead we should wait for restoreContext() to be called.
        if (mLastLossWasSimulated)
            return;

        // Restore when the app is visible
        if (mRestoreWhenVisible)
            return;

        ForceRestoreContext();
        return;
    }

    if (mContextStatus == ContextLostAwaitingRestore) {
        // Context is lost, but we should try to restore it.

        if (!mAllowContextRestore) {
            // We might decide this after thinking we'd be OK restoring
            // the context, so downgrade.
            mContextStatus = ContextLost;
            return;
        }

        if (!TryToRestoreContext()) {
            // Failed to restore. Try again later.
            mContextLossHandler->RunTimer();
            return;
        }

        // Revival!
        mContextStatus = ContextNotLost;

        if (mCanvasElement) {
            nsContentUtils::DispatchTrustedEvent(
                mCanvasElement->OwnerDoc(),
                static_cast<nsIDOMHTMLCanvasElement*>(mCanvasElement),
                NS_LITERAL_STRING("webglcontextrestored"),
                true,
                true);
        } else {
            RefPtr<Event> event = new Event(mOffscreenCanvas, nullptr, nullptr);
            event->InitEvent(NS_LITERAL_STRING("webglcontextrestored"), true, true);
            event->SetTrusted(true);
            bool unused;
            mOffscreenCanvas->DispatchEvent(event, &unused);
        }

        mEmitContextLostErrorOnce = true;
        return;
    }
}

void
WebGLContext::ForceLoseContext(bool simulateLosing)
{
    printf_stderr("WebGL(%p)::ForceLoseContext\n", this);
    MOZ_ASSERT(!IsContextLost());
    mContextStatus = ContextLostAwaitingEvent;
    mContextLostErrorSet = false;

    // Burn it all!
    DestroyResourcesAndContext();
    mLastLossWasSimulated = simulateLosing;

    // Queue up a task, since we know the status changed.
    EnqueueUpdateContextLossStatus();
}

void
WebGLContext::ForceRestoreContext()
{
    printf_stderr("WebGL(%p)::ForceRestoreContext\n", this);
    mContextStatus = ContextLostAwaitingRestore;
    mAllowContextRestore = true; // Hey, you did say 'force'.

    // Queue up a task, since we know the status changed.
    EnqueueUpdateContextLossStatus();
}

void
WebGLContext::MakeContextCurrent() const
{
    gl->MakeCurrent();
}

already_AddRefed<mozilla::gfx::SourceSurface>
WebGLContext::GetSurfaceSnapshot(bool* out_premultAlpha)
{
    if (!gl)
        return nullptr;

    bool hasAlpha = mOptions.alpha;
    SurfaceFormat surfFormat = hasAlpha ? SurfaceFormat::B8G8R8A8
                                        : SurfaceFormat::B8G8R8X8;
    RefPtr<DataSourceSurface> surf;
    surf = Factory::CreateDataSourceSurfaceWithStride(IntSize(mWidth, mHeight),
                                                      surfFormat,
                                                      mWidth * 4);
    if (NS_WARN_IF(!surf)) {
        return nullptr;
    }

    gl->MakeCurrent();
    {
        ScopedBindFramebuffer autoFB(gl, 0);
        ClearBackbufferIfNeeded();
        // TODO: Save, override, then restore glReadBuffer if present.
        ReadPixelsIntoDataSurface(gl, surf);
    }

    if (out_premultAlpha) {
        *out_premultAlpha = true;
    }
    bool srcPremultAlpha = mOptions.premultipliedAlpha;
    if (!srcPremultAlpha) {
        if (out_premultAlpha) {
            *out_premultAlpha = false;
        } else if(hasAlpha) {
            gfxUtils::PremultiplyDataSurface(surf, surf);
        }
    }

    RefPtr<DrawTarget> dt =
        Factory::CreateDrawTarget(BackendType::CAIRO,
                                  IntSize(mWidth, mHeight),
                                  SurfaceFormat::B8G8R8A8);

    if (!dt) {
        return nullptr;
    }

    dt->SetTransform(Matrix::Translation(0.0, mHeight).PreScale(1.0, -1.0));

    dt->DrawSurface(surf,
                    Rect(0, 0, mWidth, mHeight),
                    Rect(0, 0, mWidth, mHeight),
                    DrawSurfaceOptions(),
                    DrawOptions(1.0f, CompositionOp::OP_SOURCE));

    return dt->Snapshot();
}

void
WebGLContext::DidRefresh()
{
    if (gl) {
        gl->FlushIfHeavyGLCallsSinceLastFlush();
    }
}

size_t
RoundUpToMultipleOf(size_t value, size_t multiple)
{
    size_t overshoot = value + multiple - 1;
    return overshoot - (overshoot % multiple);
}

CheckedUint32
RoundedToNextMultipleOf(CheckedUint32 x, CheckedUint32 y)
{
    return ((x + y - 1) / y) * y;
}

////////////////////////////////////////////////////////////////////////////////

WebGLContext::ScopedMaskWorkaround::ScopedMaskWorkaround(WebGLContext& webgl)
    : mWebGL(webgl)
    , mFakeNoAlpha(ShouldFakeNoAlpha(webgl))
    , mFakeNoDepth(ShouldFakeNoDepth(webgl))
    , mFakeNoStencil(ShouldFakeNoStencil(webgl))
{
    if (mFakeNoAlpha) {
        mWebGL.gl->fColorMask(mWebGL.mColorWriteMask[0],
                              mWebGL.mColorWriteMask[1],
                              mWebGL.mColorWriteMask[2],
                              false);
    }
    if (mFakeNoDepth) {
        mWebGL.gl->fDisable(LOCAL_GL_DEPTH_TEST);
    }
    if (mFakeNoStencil) {
        mWebGL.gl->fDisable(LOCAL_GL_STENCIL_TEST);
    }
}

WebGLContext::ScopedMaskWorkaround::~ScopedMaskWorkaround()
{
    if (mFakeNoAlpha) {
        mWebGL.gl->fColorMask(mWebGL.mColorWriteMask[0],
                              mWebGL.mColorWriteMask[1],
                              mWebGL.mColorWriteMask[2],
                              mWebGL.mColorWriteMask[3]);
    }
    if (mFakeNoDepth) {
        mWebGL.gl->fEnable(LOCAL_GL_DEPTH_TEST);
    }
    if (mFakeNoStencil) {
        mWebGL.gl->fEnable(LOCAL_GL_STENCIL_TEST);
    }
}

////////////////////////////////////////////////////////////////////////////////
// XPCOM goop

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebGLContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebGLContext)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebGLContext,
  mCanvasElement,
  mOffscreenCanvas,
  mExtensions,
  mBound2DTextures,
  mBoundCubeMapTextures,
  mBound3DTextures,
  mBoundSamplers,
  mBoundArrayBuffer,
  mBoundCopyReadBuffer,
  mBoundCopyWriteBuffer,
  mBoundPixelPackBuffer,
  mBoundPixelUnpackBuffer,
  mBoundTransformFeedbackBuffer,
  mBoundUniformBuffer,
  mCurrentProgram,
  mBoundDrawFramebuffer,
  mBoundReadFramebuffer,
  mBoundRenderbuffer,
  mBoundVertexArray,
  mDefaultVertexArray,
  mActiveOcclusionQuery,
  mActiveTransformFeedbackQuery)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebGLContext)
    NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
    NS_INTERFACE_MAP_ENTRY(nsIDOMWebGLRenderingContext)
    NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
    // If the exact way we cast to nsISupports here ever changes, fix our
    // ToSupports() method.
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMWebGLRenderingContext)
NS_INTERFACE_MAP_END

} // namespace mozilla
