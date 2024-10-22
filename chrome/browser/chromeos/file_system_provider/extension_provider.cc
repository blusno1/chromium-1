// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/extension_provider.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/throttled_file_system.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/permissions/permissions_data.h"

namespace chromeos {
namespace file_system_provider {
namespace {

// Returns boolean indicating success. result->capabilities contains the
// capabilites of the extension.
bool GetProvidingExtensionInfo(const extensions::ExtensionId& extension_id,
                               ProvidingExtensionInfo* result,
                               extensions::ExtensionRegistry* registry) {
  DCHECK(result);
  DCHECK(registry);

  const extensions::Extension* const extension = registry->GetExtensionById(
      extension_id, extensions::ExtensionRegistry::ENABLED);
  if (!extension || !extension->permissions_data()->HasAPIPermission(
                        extensions::APIPermission::kFileSystemProvider)) {
    return false;
  }

  result->extension_id = extension->id();
  result->name = extension->name();
  const extensions::FileSystemProviderCapabilities* const capabilities =
      extensions::FileSystemProviderCapabilities::Get(extension);
  DCHECK(capabilities);
  result->capabilities = *capabilities;

  return true;
}

}  // namespace

ProvidingExtensionInfo::ProvidingExtensionInfo() = default;

ProvidingExtensionInfo::~ProvidingExtensionInfo() = default;

// static
std::unique_ptr<ProviderInterface> ExtensionProvider::Create(
    extensions::ExtensionRegistry* registry,
    const extensions::ExtensionId& extension_id) {
  ProvidingExtensionInfo info;
  if (!GetProvidingExtensionInfo(extension_id, &info, registry))
    return nullptr;

  return std::unique_ptr<ProviderInterface>(
      new ExtensionProvider(extension_id, info));
}

std::unique_ptr<ProvidedFileSystemInterface>
ExtensionProvider::CreateProvidedFileSystem(
    Profile* profile,
    const ProvidedFileSystemInfo& file_system_info) {
  DCHECK(profile);
  return std::make_unique<ThrottledFileSystem>(
      std::make_unique<ProvidedFileSystem>(profile, file_system_info));
}

const Capabilities& ExtensionProvider::GetCapabilities() const {
  return capabilities_;
}

const ProviderId& ExtensionProvider::GetId() const {
  return provider_id_;
}

const std::string& ExtensionProvider::GetName() const {
  return name_;
}

ExtensionProvider::ExtensionProvider(
    const extensions::ExtensionId& extension_id,
    const ProvidingExtensionInfo& info)
    : provider_id_(ProviderId::CreateFromExtensionId(extension_id)) {
  capabilities_.configurable = info.capabilities.configurable();
  capabilities_.watchable = info.capabilities.watchable();
  capabilities_.multiple_mounts = info.capabilities.multiple_mounts();
  capabilities_.source = info.capabilities.source();
  name_ = info.name;
}

}  // namespace file_system_provider
}  // namespace chromeos
