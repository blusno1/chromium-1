// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/proxy_config_service_mojo.h"

namespace content {

ProxyConfigServiceMojo::ProxyConfigServiceMojo(
    mojom::ProxyConfigClientRequest proxy_config_client_request,
    base::Optional<net::ProxyConfig> initial_proxy_config,
    mojom::ProxyConfigPollerClientPtrInfo proxy_poller_client)
    : binding_(this) {
  DCHECK(initial_proxy_config || proxy_config_client_request.is_pending());

  if (initial_proxy_config)
    OnProxyConfigUpdated(*initial_proxy_config);

  if (proxy_config_client_request.is_pending()) {
    binding_.Bind(std::move(proxy_config_client_request));
    // Only use the |proxy_poller_client| if there's a
    // |proxy_config_client_request|.
    proxy_poller_client_.Bind(std::move(proxy_poller_client));
  }
}

ProxyConfigServiceMojo::~ProxyConfigServiceMojo() {}

void ProxyConfigServiceMojo::OnProxyConfigUpdated(
    const net::ProxyConfig& proxy_config) {
  // Do nothing if the proxy configuration is unchanged.
  if (!config_pending_ && config_.Equals(proxy_config))
    return;

  config_pending_ = false;
  config_ = proxy_config;

  for (auto& observer : observers_)
    observer.OnProxyConfigChanged(config_, CONFIG_VALID);
}

void ProxyConfigServiceMojo::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProxyConfigServiceMojo::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

net::ProxyConfigService::ConfigAvailability
ProxyConfigServiceMojo::GetLatestProxyConfig(net::ProxyConfig* config) {
  if (config_pending_) {
    *config = net::ProxyConfig();
    return CONFIG_PENDING;
  }
  *config = config_;
  return CONFIG_VALID;
}

void ProxyConfigServiceMojo::OnLazyPoll() {
  // TODO(mmenke): These should either be rate limited, or the other process
  // should use another signal of activity.
  if (proxy_poller_client_)
    proxy_poller_client_->OnLazyProxyConfigPoll();
}

}  // namespace content
