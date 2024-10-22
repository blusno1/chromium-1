// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_CONTEXT_ADAPTER_H_
#define COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_CONTEXT_ADAPTER_H_

#include <jni.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "components/cronet/cronet_url_request_context.h"
#include "components/prefs/json_pref_store.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/effective_connection_type_observer.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/network_quality_observation_source.h"
#include "net/nqe/rtt_throughput_estimates_observer.h"

namespace net {
class NetLog;
class URLRequestContext;
}  // namespace net

namespace cronet {
class TestUtil;

struct URLRequestContextConfig;

// Adapter between Java CronetUrlRequestContext and CronetURLRequestContext.
class CronetURLRequestContextAdapter
    : public CronetURLRequestContext::Callback {
 public:
  explicit CronetURLRequestContextAdapter(
      std::unique_ptr<URLRequestContextConfig> context_config);

  ~CronetURLRequestContextAdapter() override;

  // Called on init Java thread to initialize URLRequestContext.
  void InitRequestContextOnInitThread(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  // Releases all resources for the request context and deletes the object.
  // Blocks until network thread is destroyed after running all pending tasks.
  void Destroy(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& jcaller);

  // Posts a task that might depend on the context being initialized
  // to the network thread.
  void PostTaskToNetworkThread(const base::Location& posted_from,
                               const base::Closure& callback);

  bool IsOnNetworkThread() const;

  net::URLRequestContext* GetURLRequestContext();

  // TODO(xunjieli): Keep only one version of StartNetLog().

  // Starts NetLog logging to file. This can be called on any thread.
  // Return false if |jfile_name| cannot be opened.
  bool StartNetLogToFile(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jcaller,
                         const base::android::JavaParamRef<jstring>& jfile_name,
                         jboolean jlog_all);

  // Starts NetLog logging to disk with a bounded amount of disk space. This
  // can be called on any thread.
  void StartNetLogToDisk(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jcaller,
                         const base::android::JavaParamRef<jstring>& jdir_name,
                         jboolean jlog_all,
                         jint jmax_size);

  // Stops NetLog logging to file. This can be called on any thread. This will
  // flush any remaining writes to disk.
  void StopNetLog(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jcaller);

  // Posts a task to Network thread to get serialized results of certificate
  // verifications of |context_|'s |cert_verifier|.
  void GetCertVerifierData(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jcaller);

  // Default net::LOAD flags used to create requests.
  int default_load_flags() const;

  // Called on init Java thread to initialize URLRequestContext.
  void InitRequestContextOnInitThread();

  // Configures the network quality estimator to observe requests to localhost,
  // to use smaller responses when estimating throughput, and to disable the
  // device offline checks when computing the effective connection type or when
  // writing the prefs. This should only be used for testing. This can be
  // called only after the network quality estimator has been enabled.
  void ConfigureNetworkQualityEstimatorForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean use_local_host_requests,
      jboolean use_smaller_responses,
      jboolean disable_offline_check);

  // Request that RTT and/or throughput observations should or should not be
  // provided by the network quality estimator.
  void ProvideRTTObservations(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      bool should);
  void ProvideThroughputObservations(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      bool should);

  // CronetURLRequestContext::Callback
  void OnInitNetworkThread() override;
  void OnDestroyNetworkThread() override;
  void OnInitCertVerifierData(net::CertVerifier* cert_verifier,
                              const std::string& cert_verifier_data) override;
  void OnSaveCertVerifierData(net::CertVerifier* cert_verifier) override;
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType effective_connection_type) override;
  void OnRTTOrThroughputEstimatesComputed(
      int32_t http_rtt_ms,
      int32_t transport_rtt_ms,
      int32_t downstream_throughput_kbps) override;
  void OnRTTObservation(int32_t rtt_ms,
                        int32_t timestamp_ms,
                        net::NetworkQualityObservationSource source) override;
  void OnThroughputObservation(
      int32_t throughput_kbps,
      int32_t timestamp_ms,
      net::NetworkQualityObservationSource source) override;
  void OnStopNetLogCompleted() override;

 private:
  friend class TestUtil;

  // Native Cronet URL Request Context.
  CronetURLRequestContext* context_;

  // Java object that owns this CronetURLRequestContextAdapter.
  base::android::ScopedJavaGlobalRef<jobject> jcronet_url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(CronetURLRequestContextAdapter);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_CONTEXT_ADAPTER_H_
