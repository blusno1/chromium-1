// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals_handler.h"

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "url/gurl.h"

BluetoothInternalsHandler::BluetoothInternalsHandler(
    mojom::BluetoothInternalsHandlerRequest request)
    : binding_(this, std::move(request)) {}

BluetoothInternalsHandler::~BluetoothInternalsHandler() {}

void BluetoothInternalsHandler::GetBluetoothAdapter(
    GetBluetoothAdapterCallback& callback) {
  callback.Run(false, "BLUETOOTH service unavailable.");
}

/*

void BluetoothInternalsHandler::AddDeviceForTesting(
    const std::string& name,
    const std::string& serial_number,
    const std::string& landing_page,
    const AddDeviceForTestingCallback& callback) {
  device::BluetoothService* service =
device::DeviceClient::Get()->GetBluetoothService(); if (service) { GURL
landing_page_url(landing_page); if (!landing_page_url.is_valid()) {
      callback.Run(false, "Landing page URL is invalid.");
      return;
    }

    service->AddDeviceForTesting(
        new TestBluetoothDevice(name, serial_number, landing_page_url));
    callback.Run(true, "Added.");
  } else {
    callback.Run(false, "BLUETOOTH service unavailable.");
  }
}

void BluetoothInternalsHandler::RemoveDeviceForTesting(
    const std::string& guid,
    const RemoveDeviceForTestingCallback& callback) {
  device::BluetoothService* service =
device::DeviceClient::Get()->GetBluetoothService(); if (service)
    service->RemoveDeviceForTesting(guid);
  callback.Run();
}

void BluetoothInternalsHandler::GetTestDevices(
    const GetTestDevicesCallback& callback) {
  std::vector<scoped_refptr<device::BluetoothDevice>> devices;
  device::BluetoothService* service =
device::DeviceClient::Get()->GetBluetoothService(); if (service)
    service->GetTestDevices(&devices);
  std::vector<mojom::TestDeviceInfoPtr> result;
  result.reserve(devices.size());
  for (const auto& device : devices) {
    auto device_info = mojom::TestDeviceInfo::New();
    device_info->guid = device->guid();
    device_info->name = base::UTF16ToUTF8(device->product_string());
    device_info->serial_number = base::UTF16ToUTF8(device->serial_number());
    device_info->landing_page = device->webbluetooth_landing_page();
    result.push_back(std::move(device_info));
  }
  callback.Run(std::move(result));
}
*/
