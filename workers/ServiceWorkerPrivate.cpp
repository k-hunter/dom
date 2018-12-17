/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerPrivate.h"
#include "ServiceWorkerManager.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "mozilla/dom/FetchUtil.h"

using namespace mozilla;
using namespace mozilla::dom;

BEGIN_WORKERS_NAMESPACE

NS_IMPL_CYCLE_COLLECTING_ADDREF(ServiceWorkerPrivate)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ServiceWorkerPrivate)
NS_IMPL_CYCLE_COLLECTION(ServiceWorkerPrivate, mSupportsArray)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServiceWorkerPrivate)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// Tracks the "dom.disable_open_click_delay" preference.  Modified on main
// thread, read on worker threads.
// It is updated every time a "notificationclick" event is dispatched. While
// this is done without synchronization, at the worst, the thread will just get
// an older value within which a popup is allowed to be displayed, which will
// still be a valid value since it was set prior to dispatching the runnable.
Atomic<uint32_t> gDOMDisableOpenClickDelay(0);

// Used to keep track of pending waitUntil as well as in-flight extendable events.
// When the last token is released, we attempt to terminate the worker.
class KeepAliveToken final : public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  explicit KeepAliveToken(ServiceWorkerPrivate* aPrivate)
    : mPrivate(aPrivate)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aPrivate);
    mPrivate->AddToken();
  }

private:
  ~KeepAliveToken()
  {
    AssertIsOnMainThread();
    mPrivate->ReleaseToken();
  }

  RefPtr<ServiceWorkerPrivate> mPrivate;
};

NS_IMPL_ISUPPORTS0(KeepAliveToken)

ServiceWorkerPrivate::ServiceWorkerPrivate(ServiceWorkerInfo* aInfo)
  : mInfo(aInfo)
  , mIsPushWorker(false)
  , mTokenCount(0)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aInfo);

  mIdleWorkerTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  MOZ_ASSERT(mIdleWorkerTimer);
}

ServiceWorkerPrivate::~ServiceWorkerPrivate()
{
  MOZ_ASSERT(!mWorkerPrivate);
  MOZ_ASSERT(!mTokenCount);
  MOZ_ASSERT(!mInfo);
  MOZ_ASSERT(mSupportsArray.IsEmpty());

  mIdleWorkerTimer->Cancel();
}

nsresult
ServiceWorkerPrivate::SendMessageEvent(JSContext* aCx,
                                       JS::Handle<JS::Value> aMessage,
                                       const Optional<Sequence<JS::Value>>& aTransferable,
                                       UniquePtr<ServiceWorkerClientInfo>&& aClientInfo)
{
  ErrorResult rv(SpawnWorkerIfNeeded(MessageEvent, nullptr));
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  // FIXME(catalinb): Bug 1143717
  // Keep the worker alive when dispatching a message event.
  mWorkerPrivate->PostMessageToServiceWorker(aCx, aMessage, aTransferable,
                                             Move(aClientInfo), rv);
  return rv.StealNSResult();
}

namespace {

class CheckScriptEvaluationWithCallback final : public WorkerRunnable
{
  nsMainThreadPtrHandle<KeepAliveToken> mKeepAliveToken;
  RefPtr<LifeCycleEventCallback> mCallback;
  DebugOnly<bool> mDone;

public:
  CheckScriptEvaluationWithCallback(WorkerPrivate* aWorkerPrivate,
                                    KeepAliveToken* aKeepAliveToken,
                                    LifeCycleEventCallback* aCallback)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount)
    , mKeepAliveToken(new nsMainThreadPtrHolder<KeepAliveToken>(aKeepAliveToken))
    , mCallback(aCallback)
    , mDone(false)
  {
    AssertIsOnMainThread();
  }

  ~CheckScriptEvaluationWithCallback()
  {
    MOZ_ASSERT(mDone);
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
    Done(aWorkerPrivate->WorkerScriptExecutedSuccessfully());

    return true;
  }

  NS_IMETHOD
  Cancel() override
  {
    Done(false);
    return WorkerRunnable::Cancel();
  }

private:
  void
  Done(bool aResult)
  {
    mDone = true;
    mCallback->SetResult(aResult);
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(mCallback)));
  }
};

} // anonymous namespace

nsresult
ServiceWorkerPrivate::CheckScriptEvaluation(LifeCycleEventCallback* aCallback)
{
  nsresult rv = SpawnWorkerIfNeeded(LifeCycleEvent, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(mKeepAliveToken);
  RefPtr<WorkerRunnable> r = new CheckScriptEvaluationWithCallback(mWorkerPrivate,
                                                                   mKeepAliveToken,
                                                                   aCallback);
  AutoJSAPI jsapi;
  jsapi.Init();
  if (NS_WARN_IF(!r->Dispatch(jsapi.cx()))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

namespace {

// Holds the worker alive until the waitUntil promise is resolved or
// rejected.
class KeepAliveHandler final : public PromiseNativeHandler
{
  nsMainThreadPtrHandle<KeepAliveToken> mKeepAliveToken;

  virtual ~KeepAliveHandler()
  {}

public:
  NS_DECL_ISUPPORTS

  explicit KeepAliveHandler(const nsMainThreadPtrHandle<KeepAliveToken>& aKeepAliveToken)
    : mKeepAliveToken(aKeepAliveToken)
  { }

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
#ifdef DEBUG
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    workerPrivate->AssertIsOnWorkerThread();
#endif
  }

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
#ifdef DEBUG
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    workerPrivate->AssertIsOnWorkerThread();
#endif
  }
};

NS_IMPL_ISUPPORTS0(KeepAliveHandler)

class SoftUpdateRequest : public nsRunnable
{
protected:
  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> mRegistration;
public:
  explicit SoftUpdateRequest(nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration)
    : mRegistration(aRegistration)
  {
    MOZ_ASSERT(aRegistration);
  }

  NS_IMETHOD Run()
  {
    AssertIsOnMainThread();

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    MOZ_ASSERT(swm);

    OriginAttributes attrs =
      mozilla::BasePrincipal::Cast(mRegistration->mPrincipal)->OriginAttributesRef();

    swm->PropagateSoftUpdate(attrs,
                             NS_ConvertUTF8toUTF16(mRegistration->mScope));
    return NS_OK;
  }
};

class CheckLastUpdateTimeRequest final : public SoftUpdateRequest
{
public:
  explicit CheckLastUpdateTimeRequest(
    nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration)
    : SoftUpdateRequest(aRegistration)
  {}

  NS_IMETHOD Run()
  {
    AssertIsOnMainThread();
    if (mRegistration->IsLastUpdateCheckTimeOverOneDay()) {
      SoftUpdateRequest::Run();
    }
    return NS_OK;
  }
};

class ExtendableEventWorkerRunnable : public WorkerRunnable
{
protected:
  nsMainThreadPtrHandle<KeepAliveToken> mKeepAliveToken;

public:
  ExtendableEventWorkerRunnable(WorkerPrivate* aWorkerPrivate,
                                KeepAliveToken* aKeepAliveToken)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aKeepAliveToken);

    mKeepAliveToken =
      new nsMainThreadPtrHolder<KeepAliveToken>(aKeepAliveToken);
  }

  void
  DispatchExtendableEventOnWorkerScope(JSContext* aCx,
                                       WorkerGlobalScope* aWorkerScope,
                                       ExtendableEvent* aEvent,
                                       Promise** aWaitUntilPromise)
  {
    MOZ_ASSERT(aWorkerScope);
    MOZ_ASSERT(aEvent);
    nsCOMPtr<nsIGlobalObject> sgo = aWorkerScope;
    WidgetEvent* internalEvent = aEvent->GetInternalNSEvent();

    ErrorResult result;
    result = aWorkerScope->DispatchDOMEvent(nullptr, aEvent, nullptr, nullptr);
    if (NS_WARN_IF(result.Failed()) || internalEvent->mFlags.mExceptionHasBeenRisen) {
      result.SuppressException();
      return;
    }

    RefPtr<Promise> waitUntilPromise = aEvent->GetPromise();
    if (!waitUntilPromise) {
      waitUntilPromise =
        Promise::Resolve(sgo, aCx, JS::UndefinedHandleValue, result);
      if (NS_WARN_IF(result.Failed())) {
        result.SuppressException();
        return;
      }
    }

    MOZ_ASSERT(waitUntilPromise);
    RefPtr<KeepAliveHandler> keepAliveHandler =
      new KeepAliveHandler(mKeepAliveToken);
    waitUntilPromise->AppendNativeHandler(keepAliveHandler);

    if (aWaitUntilPromise) {
      waitUntilPromise.forget(aWaitUntilPromise);
    }
  }
};

// Handle functional event
// 9.9.7 If the time difference in seconds calculated by the current time minus
// registration's last update check time is greater than 86400, invoke Soft Update
// algorithm.
class ExtendableFunctionalEventWorkerRunnable : public ExtendableEventWorkerRunnable
{
protected:
  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> mRegistration;
public:
  ExtendableFunctionalEventWorkerRunnable(WorkerPrivate* aWorkerPrivate,
                                          KeepAliveToken* aKeepAliveToken,
                                          nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration)
    : ExtendableEventWorkerRunnable(aWorkerPrivate, aKeepAliveToken)
    , mRegistration(aRegistration)
  {
    MOZ_ASSERT(aRegistration);
  }

  void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aRunResult)
  {
    nsCOMPtr<nsIRunnable> runnable = new CheckLastUpdateTimeRequest(mRegistration);

    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(runnable.forget())));
  }
};

/*
 * Fires 'install' event on the ServiceWorkerGlobalScope. Modifies busy count
 * since it fires the event. This is ok since there can't be nested
 * ServiceWorkers, so the parent thread -> worker thread requirement for
 * runnables is satisfied.
 */
class LifecycleEventWorkerRunnable : public ExtendableEventWorkerRunnable
{
  nsString mEventName;
  RefPtr<LifeCycleEventCallback> mCallback;

public:
  LifecycleEventWorkerRunnable(WorkerPrivate* aWorkerPrivate,
                               KeepAliveToken* aToken,
                               const nsAString& aEventName,
                               LifeCycleEventCallback* aCallback)
      : ExtendableEventWorkerRunnable(aWorkerPrivate, aToken)
      , mEventName(aEventName)
      , mCallback(aCallback)
  {
    AssertIsOnMainThread();
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    return DispatchLifecycleEvent(aCx, aWorkerPrivate);
  }

  NS_IMETHOD
  Cancel() override
  {
    mCallback->SetResult(false);
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(mCallback)));

    return WorkerRunnable::Cancel();
  }

private:
  bool
  DispatchLifecycleEvent(JSContext* aCx, WorkerPrivate* aWorkerPrivate);

};

/*
 * Used to handle ExtendableEvent::waitUntil() and catch abnormal worker
 * termination during the execution of life cycle events. It is responsible
 * with advancing the job queue for install/activate tasks.
 */
class LifeCycleEventWatcher final : public PromiseNativeHandler,
                                    public WorkerFeature
{
  WorkerPrivate* mWorkerPrivate;
  RefPtr<LifeCycleEventCallback> mCallback;
  bool mDone;

  ~LifeCycleEventWatcher()
  {
    if (mDone) {
      return;
    }

    MOZ_ASSERT(GetCurrentThreadWorkerPrivate() == mWorkerPrivate);
    // XXXcatalinb: If all the promises passed to waitUntil go out of scope,
    // the resulting Promise.all will be cycle collected and it will drop its
    // native handlers (including this object). Instead of waiting for a timeout
    // we report the failure now.
    JSContext* cx = mWorkerPrivate->GetJSContext();
    ReportResult(cx, false);
  }

public:
  NS_DECL_ISUPPORTS

  LifeCycleEventWatcher(WorkerPrivate* aWorkerPrivate,
                        LifeCycleEventCallback* aCallback)
    : mWorkerPrivate(aWorkerPrivate)
    , mCallback(aCallback)
    , mDone(false)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool
  Init()
  {
    MOZ_ASSERT(mWorkerPrivate);
    mWorkerPrivate->AssertIsOnWorkerThread();
    JSContext* cx = mWorkerPrivate->GetJSContext();

    // We need to listen for worker termination in case the event handler
    // never completes or never resolves the waitUntil promise. There are
    // two possible scenarios:
    // 1. The keepAlive token expires and the worker is terminated, in which
    //    case the registration/update promise will be rejected
    // 2. A new service worker is registered which will terminate the current
    //    installing worker.
    if (NS_WARN_IF(!mWorkerPrivate->AddFeature(cx, this))) {
      NS_WARNING("LifeCycleEventWatcher failed to add feature.");
      ReportResult(cx, false);
      return false;
    }

    return true;
  }

  bool
  Notify(JSContext* aCx, Status aStatus) override
  {
    if (aStatus < Terminating) {
      return true;
    }

    MOZ_ASSERT(GetCurrentThreadWorkerPrivate() == mWorkerPrivate);
    ReportResult(aCx, false);

    return true;
  }

  void
  ReportResult(JSContext* aCx, bool aResult)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();

    if (mDone) {
      return;
    }
    mDone = true;

    mCallback->SetResult(aResult);
    nsresult rv = NS_DispatchToMainThread(mCallback);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      NS_RUNTIMEABORT("Failed to dispatch life cycle event handler.");
    }

    mWorkerPrivate->RemoveFeature(aCx, this);
  }

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    MOZ_ASSERT(GetCurrentThreadWorkerPrivate() == mWorkerPrivate);
    mWorkerPrivate->AssertIsOnWorkerThread();

    ReportResult(aCx, true);
  }

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    MOZ_ASSERT(GetCurrentThreadWorkerPrivate() == mWorkerPrivate);
    mWorkerPrivate->AssertIsOnWorkerThread();

    ReportResult(aCx, false);

    // Note, all WaitUntil() rejections are reported to client consoles
    // by the WaitUntilHandler in ServiceWorkerEvents.  This ensures that
    // errors in non-lifecycle events like FetchEvent and PushEvent are
    // reported properly.
  }
};

NS_IMPL_ISUPPORTS0(LifeCycleEventWatcher)

bool
LifecycleEventWorkerRunnable::DispatchLifecycleEvent(JSContext* aCx,
                                                     WorkerPrivate* aWorkerPrivate)
{
  aWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());

  RefPtr<ExtendableEvent> event;
  RefPtr<EventTarget> target = aWorkerPrivate->GlobalScope();

  if (mEventName.EqualsASCII("install") || mEventName.EqualsASCII("activate")) {
    ExtendableEventInit init;
    init.mBubbles = false;
    init.mCancelable = false;
    event = ExtendableEvent::Constructor(target, mEventName, init);
  } else {
    MOZ_CRASH("Unexpected lifecycle event");
  }

  event->SetTrusted(true);

  // It is important to initialize the watcher before actually dispatching
  // the event in order to catch worker termination while the event handler
  // is still executing. This can happen with infinite loops, for example.
  RefPtr<LifeCycleEventWatcher> watcher =
    new LifeCycleEventWatcher(aWorkerPrivate, mCallback);

  if (!watcher->Init()) {
    return true;
  }

  RefPtr<Promise> waitUntil;
  DispatchExtendableEventOnWorkerScope(aCx, aWorkerPrivate->GlobalScope(),
                                       event, getter_AddRefs(waitUntil));
  if (waitUntil) {
    waitUntil->AppendNativeHandler(watcher);
  } else {
    watcher->ReportResult(aCx, false);
  }

  return true;
}

} // anonymous namespace

nsresult
ServiceWorkerPrivate::SendLifeCycleEvent(const nsAString& aEventType,
                                         LifeCycleEventCallback* aCallback,
                                         nsIRunnable* aLoadFailure)
{
  nsresult rv = SpawnWorkerIfNeeded(LifeCycleEvent, aLoadFailure);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(mKeepAliveToken);
  RefPtr<WorkerRunnable> r = new LifecycleEventWorkerRunnable(mWorkerPrivate,
                                                                mKeepAliveToken,
                                                                aEventType,
                                                                aCallback);
  AutoJSAPI jsapi;
  jsapi.Init();
  if (NS_WARN_IF(!r->Dispatch(jsapi.cx()))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

#ifndef MOZ_SIMPLEPUSH
namespace {

class SendPushEventRunnable final : public ExtendableFunctionalEventWorkerRunnable
{
  Maybe<nsTArray<uint8_t>> mData;

public:
  SendPushEventRunnable(WorkerPrivate* aWorkerPrivate,
                        KeepAliveToken* aKeepAliveToken,
                        const Maybe<nsTArray<uint8_t>>& aData,
                        nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> aRegistration)
      : ExtendableFunctionalEventWorkerRunnable(
          aWorkerPrivate, aKeepAliveToken, aRegistration)
      , mData(aData)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    GlobalObject globalObj(aCx, aWorkerPrivate->GlobalScope()->GetWrapper());

    PushEventInit pei;
    if (mData) {
      const nsTArray<uint8_t>& bytes = mData.ref();
      JSObject* data = Uint8Array::Create(aCx, bytes.Length(), bytes.Elements());
      if (!data) {
        return false;
      }
      pei.mData.Construct().SetAsArrayBufferView().Init(data);
    }
    pei.mBubbles = false;
    pei.mCancelable = false;

    ErrorResult result;
    RefPtr<PushEvent> event =
      PushEvent::Constructor(globalObj, NS_LITERAL_STRING("push"), pei, result);
    if (NS_WARN_IF(result.Failed())) {
      result.SuppressException();
      return false;
    }
    event->SetTrusted(true);

    DispatchExtendableEventOnWorkerScope(aCx, aWorkerPrivate->GlobalScope(),
                                         event, nullptr);

    return true;
  }
};

class SendPushSubscriptionChangeEventRunnable final : public ExtendableEventWorkerRunnable
{

public:
  explicit SendPushSubscriptionChangeEventRunnable(
    WorkerPrivate* aWorkerPrivate, KeepAliveToken* aKeepAliveToken)
      : ExtendableEventWorkerRunnable(aWorkerPrivate, aKeepAliveToken)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);

    RefPtr<EventTarget> target = aWorkerPrivate->GlobalScope();

    ExtendableEventInit init;
    init.mBubbles = false;
    init.mCancelable = false;

    RefPtr<ExtendableEvent> event =
      ExtendableEvent::Constructor(target,
                                   NS_LITERAL_STRING("pushsubscriptionchange"),
                                   init);

    event->SetTrusted(true);

    DispatchExtendableEventOnWorkerScope(aCx, aWorkerPrivate->GlobalScope(),
                                         event, nullptr);

    return true;
  }
};

} // anonymous namespace
#endif // !MOZ_SIMPLEPUSH

nsresult
ServiceWorkerPrivate::SendPushEvent(const Maybe<nsTArray<uint8_t>>& aData,
                                    ServiceWorkerRegistrationInfo* aRegistration)
{
#ifdef MOZ_SIMPLEPUSH
  return NS_ERROR_NOT_AVAILABLE;
#else
  nsresult rv = SpawnWorkerIfNeeded(PushEvent, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(mKeepAliveToken);

  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> regInfo(
    new nsMainThreadPtrHolder<ServiceWorkerRegistrationInfo>(aRegistration, false));

  RefPtr<WorkerRunnable> r = new SendPushEventRunnable(mWorkerPrivate,
                                                         mKeepAliveToken,
                                                         aData,
                                                         regInfo);

  if (mInfo->State() == ServiceWorkerState::Activating) {
    mPendingFunctionalEvents.AppendElement(r.forget());
    return NS_OK;
  }

  MOZ_ASSERT(mInfo->State() == ServiceWorkerState::Activated);

  AutoJSAPI jsapi;
  jsapi.Init();
  if (NS_WARN_IF(!r->Dispatch(jsapi.cx()))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
#endif // MOZ_SIMPLEPUSH
}

nsresult
ServiceWorkerPrivate::SendPushSubscriptionChangeEvent()
{
#ifdef MOZ_SIMPLEPUSH
  return NS_ERROR_NOT_AVAILABLE;
#else
  nsresult rv = SpawnWorkerIfNeeded(PushSubscriptionChangeEvent, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(mKeepAliveToken);
  RefPtr<WorkerRunnable> r =
    new SendPushSubscriptionChangeEventRunnable(mWorkerPrivate, mKeepAliveToken);
  AutoJSAPI jsapi;
  jsapi.Init();
  if (NS_WARN_IF(!r->Dispatch(jsapi.cx()))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
#endif // MOZ_SIMPLEPUSH
}

namespace {

static void
DummyNotificationTimerCallback(nsITimer* aTimer, void* aClosure)
{
  // Nothing.
}

class AllowWindowInteractionHandler;

class ClearWindowAllowedRunnable final : public WorkerRunnable
{
public:
  ClearWindowAllowedRunnable(WorkerPrivate* aWorkerPrivate,
                             AllowWindowInteractionHandler* aHandler)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  , mHandler(aHandler)
  { }

private:
  bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    // WorkerRunnable asserts that the dispatch is from parent thread if
    // the busy count modification is WorkerThreadUnchangedBusyCount.
    // Since this runnable will be dispatched from the timer thread, we override
    // PreDispatch and PostDispatch to skip the check.
    return true;
  }

  void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) override
  {
    // Silence bad assertions.
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override;

  RefPtr<AllowWindowInteractionHandler> mHandler;
};

class AllowWindowInteractionHandler final : public PromiseNativeHandler
{
  friend class ClearWindowAllowedRunnable;
  nsCOMPtr<nsITimer> mTimer;

  ~AllowWindowInteractionHandler()
  {
  }

  void
  ClearWindowAllowed(WorkerPrivate* aWorkerPrivate)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    if (!mTimer) {
      return;
    }

    // XXXcatalinb: This *might* be executed after the global was unrooted, in
    // which case GlobalScope() will return null. Making the check here just
    // to be safe.
    WorkerGlobalScope* globalScope = aWorkerPrivate->GlobalScope();
    if (!globalScope) {
      return;
    }

    globalScope->ConsumeWindowInteraction();
    mTimer->Cancel();
    mTimer = nullptr;
    MOZ_ALWAYS_TRUE(aWorkerPrivate->ModifyBusyCountFromWorker(aWorkerPrivate->GetJSContext(),
                                                              false));
  }

  void
  StartClearWindowTimer(WorkerPrivate* aWorkerPrivate)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(!mTimer);

    nsresult rv;
    nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    RefPtr<ClearWindowAllowedRunnable> r =
      new ClearWindowAllowedRunnable(aWorkerPrivate, this);

    RefPtr<TimerThreadEventTarget> target =
      new TimerThreadEventTarget(aWorkerPrivate, r);

    rv = timer->SetTarget(target);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    // The important stuff that *has* to be reversed.
    if (NS_WARN_IF(!aWorkerPrivate->ModifyBusyCountFromWorker(aWorkerPrivate->GetJSContext(), true))) {
      return;
    }
    aWorkerPrivate->GlobalScope()->AllowWindowInteraction();
    timer.swap(mTimer);

    // We swap first and then initialize the timer so that even if initializing
    // fails, we still clean the busy count and interaction count correctly.
    // The timer can't be initialized before modifying the busy count since the
    // timer thread could run and call the timeout but the worker may
    // already be terminating and modifying the busy count could fail.
    rv = mTimer->InitWithFuncCallback(DummyNotificationTimerCallback, nullptr,
                                      gDOMDisableOpenClickDelay,
                                      nsITimer::TYPE_ONE_SHOT);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ClearWindowAllowed(aWorkerPrivate);
      return;
    }
  }

public:
  NS_DECL_ISUPPORTS

  explicit AllowWindowInteractionHandler(WorkerPrivate* aWorkerPrivate)
  {
    StartClearWindowTimer(aWorkerPrivate);
  }

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
    ClearWindowAllowed(workerPrivate);
  }

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
    ClearWindowAllowed(workerPrivate);
  }
};

NS_IMPL_ISUPPORTS0(AllowWindowInteractionHandler)

bool
ClearWindowAllowedRunnable::WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
{
  mHandler->ClearWindowAllowed(aWorkerPrivate);
  return true;
}

class SendNotificationClickEventRunnable final : public ExtendableEventWorkerRunnable
{
  const nsString mID;
  const nsString mTitle;
  const nsString mDir;
  const nsString mLang;
  const nsString mBody;
  const nsString mTag;
  const nsString mIcon;
  const nsString mData;
  const nsString mBehavior;
  const nsString mScope;

public:
  SendNotificationClickEventRunnable(WorkerPrivate* aWorkerPrivate,
                                     KeepAliveToken* aKeepAliveToken,
                                     const nsAString& aID,
                                     const nsAString& aTitle,
                                     const nsAString& aDir,
                                     const nsAString& aLang,
                                     const nsAString& aBody,
                                     const nsAString& aTag,
                                     const nsAString& aIcon,
                                     const nsAString& aData,
                                     const nsAString& aBehavior,
                                     const nsAString& aScope)
      : ExtendableEventWorkerRunnable(aWorkerPrivate, aKeepAliveToken)
      , mID(aID)
      , mTitle(aTitle)
      , mDir(aDir)
      , mLang(aLang)
      , mBody(aBody)
      , mTag(aTag)
      , mIcon(aIcon)
      , mData(aData)
      , mBehavior(aBehavior)
      , mScope(aScope)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);

    RefPtr<EventTarget> target = do_QueryObject(aWorkerPrivate->GlobalScope());

    ErrorResult result;
    RefPtr<Notification> notification =
      Notification::ConstructFromFields(aWorkerPrivate->GlobalScope(), mID,
                                        mTitle, mDir, mLang, mBody, mTag, mIcon,
                                        mData, mScope, result);
    if (NS_WARN_IF(result.Failed())) {
      return false;
    }

    NotificationEventInit nei;
    nei.mNotification = notification;
    nei.mBubbles = false;
    nei.mCancelable = false;

    RefPtr<NotificationEvent> event =
      NotificationEvent::Constructor(target,
                                     NS_LITERAL_STRING("notificationclick"),
                                     nei, result);
    if (NS_WARN_IF(result.Failed())) {
      return false;
    }

    event->SetTrusted(true);
    RefPtr<Promise> waitUntil;
    aWorkerPrivate->GlobalScope()->AllowWindowInteraction();
    DispatchExtendableEventOnWorkerScope(aCx, aWorkerPrivate->GlobalScope(),
                                         event, getter_AddRefs(waitUntil));
      aWorkerPrivate->GlobalScope()->ConsumeWindowInteraction();
    if (waitUntil) {
      RefPtr<AllowWindowInteractionHandler> allowWindowInteraction =
        new AllowWindowInteractionHandler(aWorkerPrivate);
      waitUntil->AppendNativeHandler(allowWindowInteraction);
    }

    return true;
  }
};

} // namespace anonymous

nsresult
ServiceWorkerPrivate::SendNotificationClickEvent(const nsAString& aID,
                                                 const nsAString& aTitle,
                                                 const nsAString& aDir,
                                                 const nsAString& aLang,
                                                 const nsAString& aBody,
                                                 const nsAString& aTag,
                                                 const nsAString& aIcon,
                                                 const nsAString& aData,
                                                 const nsAString& aBehavior,
                                                 const nsAString& aScope)
{
  nsresult rv = SpawnWorkerIfNeeded(NotificationClickEvent, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  gDOMDisableOpenClickDelay = Preferences::GetInt("dom.disable_open_click_delay");

  RefPtr<WorkerRunnable> r =
    new SendNotificationClickEventRunnable(mWorkerPrivate, mKeepAliveToken,
                                           aID, aTitle, aDir, aLang,
                                           aBody, aTag, aIcon, aData,
                                           aBehavior, aScope);
  AutoJSAPI jsapi;
  jsapi.Init();
  if (NS_WARN_IF(!r->Dispatch(jsapi.cx()))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

namespace {

// Inheriting ExtendableEventWorkerRunnable so that the worker is not terminated
// while handling the fetch event, though that's very unlikely.
class FetchEventRunnable : public ExtendableFunctionalEventWorkerRunnable
                         , public nsIHttpHeaderVisitor {
  nsMainThreadPtrHandle<nsIInterceptedChannel> mInterceptedChannel;
  const nsCString mScriptSpec;
  nsTArray<nsCString> mHeaderNames;
  nsTArray<nsCString> mHeaderValues;
  nsCString mSpec;
  nsCString mMethod;
  bool mIsReload;
  DebugOnly<bool> mIsHttpChannel;
  RequestMode mRequestMode;
  RequestRedirect mRequestRedirect;
  RequestCredentials mRequestCredentials;
  nsContentPolicyType mContentPolicyType;
  nsCOMPtr<nsIInputStream> mUploadStream;
  nsCString mReferrer;
public:
  FetchEventRunnable(WorkerPrivate* aWorkerPrivate,
                     KeepAliveToken* aKeepAliveToken,
                     nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                     // CSP checks might require the worker script spec
                     // later on.
                     const nsACString& aScriptSpec,
                     nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration,
                     UniquePtr<ServiceWorkerClientInfo>&& aClientInfo,
                     bool aIsReload)
    : ExtendableFunctionalEventWorkerRunnable(
        aWorkerPrivate, aKeepAliveToken, aRegistration)
    , mInterceptedChannel(aChannel)
    , mScriptSpec(aScriptSpec)
    , mIsReload(aIsReload)
    , mIsHttpChannel(false)
    , mRequestMode(RequestMode::No_cors)
    , mRequestRedirect(RequestRedirect::Follow)
    // By default we set it to same-origin since normal HTTP fetches always
    // send credentials to same-origin websites unless explicitly forbidden.
    , mRequestCredentials(RequestCredentials::Same_origin)
    , mContentPolicyType(nsIContentPolicy::TYPE_INVALID)
    , mReferrer(kFETCH_CLIENT_REFERRER_STR)
  {
    MOZ_ASSERT(aWorkerPrivate);
  }

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD
  VisitHeader(const nsACString& aHeader, const nsACString& aValue) override
  {
    mHeaderNames.AppendElement(aHeader);
    mHeaderValues.AppendElement(aValue);
    return NS_OK;
  }

  nsresult
  Init()
  {
    AssertIsOnMainThread();
    nsCOMPtr<nsIChannel> channel;
    nsresult rv = mInterceptedChannel->GetChannel(getter_AddRefs(channel));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> uri;
    rv = channel->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = uri->GetSpec(mSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t loadFlags;
    rv = channel->GetLoadFlags(&loadFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILoadInfo> loadInfo;
    rv = channel->GetLoadInfo(getter_AddRefs(loadInfo));
    NS_ENSURE_SUCCESS(rv, rv);

    mContentPolicyType = loadInfo->InternalContentPolicyType();

    nsCOMPtr<nsIURI> referrerURI;
    rv = NS_GetReferrerFromChannel(channel, getter_AddRefs(referrerURI));
    // We can't bail on failure since certain non-http channels like JAR
    // channels are intercepted but don't have referrers.
    if (NS_SUCCEEDED(rv) && referrerURI) {
      rv = referrerURI->GetSpec(mReferrer);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
    if (httpChannel) {
      mIsHttpChannel = true;

      rv = httpChannel->GetRequestMethod(mMethod);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIHttpChannelInternal> internalChannel = do_QueryInterface(httpChannel);
      NS_ENSURE_TRUE(internalChannel, NS_ERROR_NOT_AVAILABLE);

      mRequestMode = InternalRequest::MapChannelToRequestMode(channel);

      // This is safe due to static_asserts at top of file.
      uint32_t redirectMode;
      internalChannel->GetRedirectMode(&redirectMode);
      mRequestRedirect = static_cast<RequestRedirect>(redirectMode);

      if (loadFlags & nsIRequest::LOAD_ANONYMOUS) {
        mRequestCredentials = RequestCredentials::Omit;
      } else {
        bool includeCrossOrigin;
        internalChannel->GetCorsIncludeCredentials(&includeCrossOrigin);
        if (includeCrossOrigin) {
          mRequestCredentials = RequestCredentials::Include;
        }
      }

      rv = httpChannel->VisitNonDefaultRequestHeaders(this);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(httpChannel);
      if (uploadChannel) {
        MOZ_ASSERT(!mUploadStream);
        bool bodyHasHeaders = false;
        rv = uploadChannel->GetUploadStreamHasHeaders(&bodyHasHeaders);
        NS_ENSURE_SUCCESS(rv, rv);
        nsCOMPtr<nsIInputStream> uploadStream;
        rv = uploadChannel->CloneUploadStream(getter_AddRefs(uploadStream));
        NS_ENSURE_SUCCESS(rv, rv);
        if (bodyHasHeaders) {
          HandleBodyWithHeaders(uploadStream);
        } else {
          mUploadStream = uploadStream;
        }
      }
    } else {
      nsCOMPtr<nsIJARChannel> jarChannel = do_QueryInterface(channel);
      // If it is not an HTTP channel it must be a JAR one.
      NS_ENSURE_TRUE(jarChannel, NS_ERROR_NOT_AVAILABLE);

      mMethod = "GET";

      mRequestMode = InternalRequest::MapChannelToRequestMode(channel);

      if (loadFlags & nsIRequest::LOAD_ANONYMOUS) {
        mRequestCredentials = RequestCredentials::Omit;
      }
    }

    return NS_OK;
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    return DispatchFetchEvent(aCx, aWorkerPrivate);
  }

  NS_IMETHOD
  Cancel() override
  {
    nsCOMPtr<nsIRunnable> runnable = new ResumeRequest(mInterceptedChannel);
    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      NS_WARNING("Failed to resume channel on FetchEventRunnable::Cancel()!\n");
    }
    WorkerRunnable::Cancel();
    return NS_OK;
  }

private:
  ~FetchEventRunnable() {}

  class ResumeRequest final : public nsRunnable {
    nsMainThreadPtrHandle<nsIInterceptedChannel> mChannel;
  public:
    explicit ResumeRequest(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel)
      : mChannel(aChannel)
    {
    }

    NS_IMETHOD Run()
    {
      AssertIsOnMainThread();
      nsresult rv = mChannel->ResetInterception();
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to resume intercepted network request");
      return rv;
    }
  };

  bool
  DispatchFetchEvent(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    MOZ_ASSERT(aCx);
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
    GlobalObject globalObj(aCx, aWorkerPrivate->GlobalScope()->GetWrapper());

    NS_ConvertUTF8toUTF16 local(mSpec);
    RequestOrUSVString requestInfo;
    requestInfo.SetAsUSVString().Rebind(local.Data(), local.Length());

    RootedDictionary<RequestInit> reqInit(aCx);
    reqInit.mMethod.Construct(mMethod);

    RefPtr<InternalHeaders> internalHeaders = new InternalHeaders(HeadersGuardEnum::Request);
    MOZ_ASSERT(mHeaderNames.Length() == mHeaderValues.Length());
    for (uint32_t i = 0; i < mHeaderNames.Length(); i++) {
      ErrorResult result;
      internalHeaders->Set(mHeaderNames[i], mHeaderValues[i], result);
      if (NS_WARN_IF(result.Failed())) {
        result.SuppressException();
        return false;
      }
    }

    RefPtr<Headers> headers = new Headers(globalObj.GetAsSupports(), internalHeaders);
    reqInit.mHeaders.Construct();
    reqInit.mHeaders.Value().SetAsHeaders() = headers;

    reqInit.mMode.Construct(mRequestMode);
    reqInit.mRedirect.Construct(mRequestRedirect);
    reqInit.mCredentials.Construct(mRequestCredentials);

    ErrorResult result;
    RefPtr<Request> request = Request::Constructor(globalObj, requestInfo, reqInit, result);
    if (NS_WARN_IF(result.Failed())) {
      result.SuppressException();
      return false;
    }
    // For Telemetry, note that this Request object was created by a Fetch event.
    RefPtr<InternalRequest> internalReq = request->GetInternalRequest();
    MOZ_ASSERT(internalReq);
    internalReq->SetCreatedByFetchEvent();

    internalReq->SetBody(mUploadStream);
    internalReq->SetReferrer(NS_ConvertUTF8toUTF16(mReferrer));

    request->SetContentPolicyType(mContentPolicyType);

    request->GetInternalHeaders()->SetGuard(HeadersGuardEnum::Immutable, result);
    if (NS_WARN_IF(result.Failed())) {
      result.SuppressException();
      return false;
    }

    // TODO: remove conditional on http here once app protocol support is
    //       removed from service worker interception
    MOZ_ASSERT_IF(mIsHttpChannel && internalReq->IsNavigationRequest(),
                  request->Redirect() == RequestRedirect::Manual);

    RootedDictionary<FetchEventInit> init(aCx);
    init.mRequest.Construct();
    init.mRequest.Value() = request;
    init.mBubbles = false;
    init.mCancelable = true;
    init.mIsReload = mIsReload;
    RefPtr<FetchEvent> event =
      FetchEvent::Constructor(globalObj, NS_LITERAL_STRING("fetch"), init, result);
    if (NS_WARN_IF(result.Failed())) {
      result.SuppressException();
      return false;
    }

    event->PostInit(mInterceptedChannel, mScriptSpec);
    event->SetTrusted(true);

    RefPtr<EventTarget> target = do_QueryObject(aWorkerPrivate->GlobalScope());
    nsresult rv2 = target->DispatchDOMEvent(nullptr, event, nullptr, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv2)) || !event->WaitToRespond()) {
      nsCOMPtr<nsIRunnable> runnable;
      if (event->DefaultPrevented(aCx)) {
        event->ReportCanceled();
        runnable = new CancelChannelRunnable(mInterceptedChannel,
                                             NS_ERROR_INTERCEPTION_FAILED);
      } else if (event->GetInternalNSEvent()->mFlags.mExceptionHasBeenRisen) {
        // Exception logged via the WorkerPrivate ErrorReporter
        runnable = new CancelChannelRunnable(mInterceptedChannel,
                                             NS_ERROR_INTERCEPTION_FAILED);
      } else {
        runnable = new ResumeRequest(mInterceptedChannel);
      }

      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(runnable)));
    }

    RefPtr<Promise> waitUntilPromise = event->GetPromise();
    if (waitUntilPromise) {
      RefPtr<KeepAliveHandler> keepAliveHandler =
        new KeepAliveHandler(mKeepAliveToken);
      waitUntilPromise->AppendNativeHandler(keepAliveHandler);
    }

    // 9.8.22 If request is a non-subresource request, then: Invoke Soft Update algorithm
    if (internalReq->IsNavigationRequest()) {
      nsCOMPtr<nsIRunnable> runnable= new SoftUpdateRequest(mRegistration);

      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(runnable.forget())));
    }
    return true;
  }

  nsresult
  HandleBodyWithHeaders(nsIInputStream* aUploadStream)
  {
    // We are dealing with an nsMIMEInputStream which uses string input streams
    // under the hood, so all of the data is available synchronously.
    bool nonBlocking = false;
    nsresult rv = aUploadStream->IsNonBlocking(&nonBlocking);
    NS_ENSURE_SUCCESS(rv, rv);
    if (NS_WARN_IF(!nonBlocking)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    nsAutoCString body;
    rv = NS_ConsumeStream(aUploadStream, UINT32_MAX, body);
    NS_ENSURE_SUCCESS(rv, rv);

    // Extract the headers in the beginning of the buffer
    nsAutoCString::const_iterator begin, end;
    body.BeginReading(begin);
    body.EndReading(end);
    const nsAutoCString::const_iterator body_end = end;
    nsAutoCString headerName, headerValue;
    bool emptyHeader = false;
    while (FetchUtil::ExtractHeader(begin, end, headerName,
                                    headerValue, &emptyHeader) &&
           !emptyHeader) {
      mHeaderNames.AppendElement(headerName);
      mHeaderValues.AppendElement(headerValue);
      headerName.Truncate();
      headerValue.Truncate();
    }

    // Replace the upload stream with one only containing the body text.
    nsCOMPtr<nsIStringInputStream> strStream =
      do_CreateInstance(NS_STRINGINPUTSTREAM_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    // Skip past the "\r\n" that separates the headers and the body.
    ++begin;
    ++begin;
    body.Assign(Substring(begin, body_end));
    rv = strStream->SetData(body.BeginReading(), body.Length());
    NS_ENSURE_SUCCESS(rv, rv);
    mUploadStream = strStream;

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS_INHERITED(FetchEventRunnable, WorkerRunnable, nsIHttpHeaderVisitor)

} // anonymous namespace

nsresult
ServiceWorkerPrivate::SendFetchEvent(nsIInterceptedChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     UniquePtr<ServiceWorkerClientInfo>&& aClientInfo,
                                     bool aIsReload)
{
  AssertIsOnMainThread();

  // if the ServiceWorker script fails to load for some reason, just resume
  // the original channel.
  nsCOMPtr<nsIRunnable> failRunnable =
    NS_NewRunnableMethod(aChannel, &nsIInterceptedChannel::ResetInterception);

  nsresult rv = SpawnWorkerIfNeeded(FetchEvent, failRunnable, aLoadGroup);
  NS_ENSURE_SUCCESS(rv, rv);

  nsMainThreadPtrHandle<nsIInterceptedChannel> handle(
    new nsMainThreadPtrHolder<nsIInterceptedChannel>(aChannel, false));

  if (NS_WARN_IF(!mInfo)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  MOZ_ASSERT(swm);

  RefPtr<ServiceWorkerRegistrationInfo> registration =
    swm->GetRegistration(mInfo->GetPrincipal(), mInfo->Scope());

  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> regInfo(
    new nsMainThreadPtrHolder<ServiceWorkerRegistrationInfo>(registration, false));

  RefPtr<FetchEventRunnable> r =
    new FetchEventRunnable(mWorkerPrivate, mKeepAliveToken, handle,
                           mInfo->ScriptSpec(), regInfo,
                           Move(aClientInfo), aIsReload);
  rv = r->Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mInfo->State() == ServiceWorkerState::Activating) {
    mPendingFunctionalEvents.AppendElement(r.forget());
    return NS_OK;
  }

  MOZ_ASSERT(mInfo->State() == ServiceWorkerState::Activated);

  AutoJSAPI jsapi;
  jsapi.Init();
  if (NS_WARN_IF(!r->Dispatch(jsapi.cx()))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
ServiceWorkerPrivate::SpawnWorkerIfNeeded(WakeUpReason aWhy,
                                          nsIRunnable* aLoadFailedRunnable,
                                          nsILoadGroup* aLoadGroup)
{
  AssertIsOnMainThread();

  // XXXcatalinb: We need to have a separate load group that's linked to
  // an existing tab child to pass security checks on b2g.
  // This should be fixed in bug 1125961, but for now we enforce updating
  // the overriden load group when intercepting a fetch.
  MOZ_ASSERT_IF(aWhy == FetchEvent, aLoadGroup);

  if (mWorkerPrivate) {
    mWorkerPrivate->UpdateOverridenLoadGroup(aLoadGroup);
    ResetIdleTimeout(aWhy);

    return NS_OK;
  }

  // Sanity check: mSupportsArray should be empty if we're about to
  // spin up a new worker.
  MOZ_ASSERT(mSupportsArray.IsEmpty());

  if (NS_WARN_IF(!mInfo)) {
    NS_WARNING("Trying to wake up a dead service worker.");
    return NS_ERROR_FAILURE;
  }

  // TODO(catalinb): Bug 1192138 - Add telemetry for service worker wake-ups.

  // Ensure that the IndexedDatabaseManager is initialized
  NS_WARN_IF(!indexedDB::IndexedDatabaseManager::GetOrCreate());

  WorkerLoadInfo info;
  nsresult rv = NS_NewURI(getter_AddRefs(info.mBaseURI), mInfo->ScriptSpec(),
                          nullptr, nullptr);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  info.mResolvedScriptURI = info.mBaseURI;
  MOZ_ASSERT(!mInfo->CacheName().IsEmpty());
  info.mServiceWorkerCacheName = mInfo->CacheName();
  info.mServiceWorkerID = mInfo->ID();
  info.mLoadGroup = aLoadGroup;
  info.mLoadFailedAsyncRunnable = aLoadFailedRunnable;

  rv = info.mBaseURI->GetHost(info.mDomain);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  info.mPrincipal = mInfo->GetPrincipal();

  nsContentUtils::StorageAccess access =
    nsContentUtils::StorageAllowedForPrincipal(info.mPrincipal);
  info.mStorageAllowed = access > nsContentUtils::StorageAccess::ePrivateBrowsing;
  info.mPrivateBrowsing = false;

  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = info.mPrincipal->GetCsp(getter_AddRefs(csp));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  info.mCSP = csp;
  if (info.mCSP) {
    rv = info.mCSP->GetAllowsEval(&info.mReportCSPViolations,
                                  &info.mEvalAllowed);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    info.mEvalAllowed = true;
    info.mReportCSPViolations = false;
  }

  WorkerPrivate::OverrideLoadInfoLoadGroup(info);

  AutoJSAPI jsapi;
  jsapi.Init();
  ErrorResult error;
  NS_ConvertUTF8toUTF16 scriptSpec(mInfo->ScriptSpec());

  mWorkerPrivate = WorkerPrivate::Constructor(jsapi.cx(),
                                              scriptSpec,
                                              false, WorkerTypeService,
                                              mInfo->Scope(), &info, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  mIsPushWorker = false;
  ResetIdleTimeout(aWhy);

  return NS_OK;
}

void
ServiceWorkerPrivate::StoreISupports(nsISupports* aSupports)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mWorkerPrivate);
  MOZ_ASSERT(!mSupportsArray.Contains(aSupports));

  mSupportsArray.AppendElement(aSupports);
}

void
ServiceWorkerPrivate::RemoveISupports(nsISupports* aSupports)
{
  AssertIsOnMainThread();
  mSupportsArray.RemoveElement(aSupports);
}

void
ServiceWorkerPrivate::TerminateWorker()
{
  AssertIsOnMainThread();

  mIdleWorkerTimer->Cancel();
  mKeepAliveToken = nullptr;
  if (mWorkerPrivate) {
    if (Preferences::GetBool("dom.serviceWorkers.testing.enabled")) {
      nsCOMPtr<nsIObserverService> os = services::GetObserverService();
      if (os) {
        os->NotifyObservers(this, "service-worker-shutdown", nullptr);
      }
    }

    AutoJSAPI jsapi;
    jsapi.Init();
    NS_WARN_IF(!mWorkerPrivate->Terminate(jsapi.cx()));
    mWorkerPrivate = nullptr;
    mSupportsArray.Clear();

    // Any pending events are never going to fire on this worker.  Cancel
    // them so that intercepted channels can be reset and other resources
    // cleaned up.
    nsTArray<RefPtr<WorkerRunnable>> pendingEvents;
    mPendingFunctionalEvents.SwapElements(pendingEvents);
    for (uint32_t i = 0; i < pendingEvents.Length(); ++i) {
      pendingEvents[i]->Cancel();
    }
  }
}

void
ServiceWorkerPrivate::NoteDeadServiceWorkerInfo()
{
  AssertIsOnMainThread();
  mInfo = nullptr;
  TerminateWorker();
}

void
ServiceWorkerPrivate::NoteStoppedControllingDocuments()
{
  AssertIsOnMainThread();
  if (mIsPushWorker) {
    return;
  }

  TerminateWorker();
}

void
ServiceWorkerPrivate::Activated()
{
  AssertIsOnMainThread();

  // If we had to queue up events due to the worker activating, that means
  // the worker must be currently running.  We should be called synchronously
  // when the worker becomes activated.
  MOZ_ASSERT_IF(!mPendingFunctionalEvents.IsEmpty(), mWorkerPrivate);

  nsTArray<RefPtr<WorkerRunnable>> pendingEvents;
  mPendingFunctionalEvents.SwapElements(pendingEvents);

  for (uint32_t i = 0; i < pendingEvents.Length(); ++i) {
    RefPtr<WorkerRunnable> r = pendingEvents[i].forget();
    AutoJSAPI jsapi;
    jsapi.Init();
    if (NS_WARN_IF(!r->Dispatch(jsapi.cx()))) {
      NS_WARNING("Failed to dispatch pending functional event!");
    }
  }
}

/* static */ void
ServiceWorkerPrivate::NoteIdleWorkerCallback(nsITimer* aTimer, void* aPrivate)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrivate);

  RefPtr<ServiceWorkerPrivate> swp = static_cast<ServiceWorkerPrivate*>(aPrivate);

  MOZ_ASSERT(aTimer == swp->mIdleWorkerTimer, "Invalid timer!");

  // Release ServiceWorkerPrivate's token, since the grace period has ended.
  swp->mKeepAliveToken = nullptr;

  if (swp->mWorkerPrivate) {
    // If we still have a workerPrivate at this point it means there are pending
    // waitUntil promises. Wait a bit more until we forcibly terminate the
    // worker.
    uint32_t timeout = Preferences::GetInt("dom.serviceWorkers.idle_extended_timeout");
    DebugOnly<nsresult> rv =
      swp->mIdleWorkerTimer->InitWithFuncCallback(ServiceWorkerPrivate::TerminateWorkerCallback,
                                                  aPrivate,
                                                  timeout,
                                                  nsITimer::TYPE_ONE_SHOT);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
}

/* static */ void
ServiceWorkerPrivate::TerminateWorkerCallback(nsITimer* aTimer, void *aPrivate)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrivate);

  RefPtr<ServiceWorkerPrivate> serviceWorkerPrivate =
    static_cast<ServiceWorkerPrivate*>(aPrivate);

  MOZ_ASSERT(aTimer == serviceWorkerPrivate->mIdleWorkerTimer,
      "Invalid timer!");

  serviceWorkerPrivate->TerminateWorker();
}

void
ServiceWorkerPrivate::ResetIdleTimeout(WakeUpReason aWhy)
{
  // We should have an active worker if we're reseting the idle timeout
  MOZ_ASSERT(mWorkerPrivate);

  if (aWhy == PushEvent || aWhy == PushSubscriptionChangeEvent) {
    mIsPushWorker = true;
  }

  uint32_t timeout = Preferences::GetInt("dom.serviceWorkers.idle_timeout");
  DebugOnly<nsresult> rv =
    mIdleWorkerTimer->InitWithFuncCallback(ServiceWorkerPrivate::NoteIdleWorkerCallback,
                                           this, timeout,
                                           nsITimer::TYPE_ONE_SHOT);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (!mKeepAliveToken) {
    mKeepAliveToken = new KeepAliveToken(this);
  }
}

void
ServiceWorkerPrivate::AddToken()
{
  AssertIsOnMainThread();
  ++mTokenCount;
}

void
ServiceWorkerPrivate::ReleaseToken()
{
  AssertIsOnMainThread();

  MOZ_ASSERT(mTokenCount > 0);
  --mTokenCount;
  if (!mTokenCount) {
    TerminateWorker();
  }
}

END_WORKERS_NAMESPACE
