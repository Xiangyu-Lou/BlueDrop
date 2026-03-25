#include "SystemChecker.h"

#include <Windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <bluetoothapis.h>
#include <comdef.h>
#include <wrl/client.h>

#pragma comment(lib, "Bthprops.lib")

using Microsoft::WRL::ComPtr;

namespace BlueDrop {

// Use RtlGetVersion to get accurate OS version (GetVersionEx lies on newer Windows)
using RtlGetVersionFunc = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);

bool SystemChecker::checkOsVersion(SystemCheckResult& result)
{
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) {
        result.osVersionString = "Unknown (ntdll.dll not found)";
        result.failureMessage = QString::fromUtf8(u8"无法检测操作系统版本");
        return false;
    }

    auto rtlGetVersion = reinterpret_cast<RtlGetVersionFunc>(
        GetProcAddress(ntdll, "RtlGetVersion"));
    if (!rtlGetVersion) {
        result.osVersionString = "Unknown (RtlGetVersion not found)";
        result.failureMessage = QString::fromUtf8(u8"无法检测操作系统版本");
        return false;
    }

    RTL_OSVERSIONINFOW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    if (rtlGetVersion(&osvi) != 0) {
        result.osVersionString = "Unknown (RtlGetVersion failed)";
        result.failureMessage = QString::fromUtf8(u8"无法检测操作系统版本");
        return false;
    }

    result.osBuildNumber = osvi.dwBuildNumber;
    result.osVersionString = QString("Windows %1.%2 Build %3")
        .arg(osvi.dwMajorVersion)
        .arg(osvi.dwMinorVersion)
        .arg(osvi.dwBuildNumber);

    result.osVersionOk = (osvi.dwMajorVersion >= 10 && osvi.dwBuildNumber >= MIN_BUILD_NUMBER);

    if (!result.osVersionOk) {
        result.failureMessage = QString::fromUtf8(
            u8"系统版本过低 (当前: Build %1)，需要 Windows 10 2004 (Build 19041) 或更高版本")
            .arg(osvi.dwBuildNumber);
    }

    return result.osVersionOk;
}

bool SystemChecker::checkBluetoothAdapter(SystemCheckResult& result)
{
    BLUETOOTH_FIND_RADIO_PARAMS params{};
    params.dwSize = sizeof(params);

    HANDLE hRadio = nullptr;
    HBLUETOOTH_RADIO_FIND hFind = BluetoothFindFirstRadio(&params, &hRadio);

    if (hFind == nullptr) {
        result.bluetoothAvailable = false;
        if (result.failureMessage.isEmpty()) {
            result.failureMessage = QString::fromUtf8(
                u8"未检测到蓝牙适配器，请插入蓝牙适配器或启用蓝牙功能");
        }
        return false;
    }

    // Get adapter info
    BLUETOOTH_RADIO_INFO radioInfo{};
    radioInfo.dwSize = sizeof(radioInfo);
    if (BluetoothGetRadioInfo(hRadio, &radioInfo) == ERROR_SUCCESS) {
        result.bluetoothAdapterName = QString::fromWCharArray(radioInfo.szName);
    }

    CloseHandle(hRadio);
    BluetoothFindRadioClose(hFind);

    result.bluetoothAvailable = true;
    return true;
}

bool SystemChecker::checkVBCable(SystemCheckResult& result)
{
    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(enumerator.GetAddressOf()));

    if (FAILED(hr)) {
        result.vbCableInstalled = false;
        if (result.failureMessage.isEmpty()) {
            result.failureMessage = QString::fromUtf8(u8"无法枚举音频设备");
        }
        return false;
    }

    // Search both render and capture endpoints for VB-Cable
    EDataFlow flows[] = { eRender, eCapture };
    for (auto flow : flows) {
        ComPtr<IMMDeviceCollection> collection;
        hr = enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, collection.GetAddressOf());
        if (FAILED(hr)) continue;

        UINT count = 0;
        collection->GetCount(&count);

        for (UINT i = 0; i < count; i++) {
            ComPtr<IMMDevice> device;
            hr = collection->Item(i, device.GetAddressOf());
            if (FAILED(hr)) continue;

            ComPtr<IPropertyStore> props;
            hr = device->OpenPropertyStore(STGM_READ, props.GetAddressOf());
            if (FAILED(hr)) continue;

            PROPVARIANT varName;
            PropVariantInit(&varName);
            hr = props->GetValue(PKEY_Device_FriendlyName, &varName);
            if (SUCCEEDED(hr) && varName.vt == VT_LPWSTR) {
                QString name = QString::fromWCharArray(varName.pwszVal);
                if (name.contains("CABLE", Qt::CaseInsensitive)) {
                    result.vbCableInstalled = true;
                    result.vbCableDeviceName = name;
                    PropVariantClear(&varName);
                    return true;
                }
            }
            PropVariantClear(&varName);
        }
    }

    result.vbCableInstalled = false;
    if (result.failureMessage.isEmpty()) {
        result.failureMessage = QString::fromUtf8(
            u8"未检测到 VB-Audio Virtual Cable，请先安装：https://vb-audio.com/Cable/");
    }
    return false;
}

SystemCheckResult SystemChecker::runAllChecks()
{
    SystemCheckResult result;

    checkOsVersion(result);
    checkBluetoothAdapter(result);
    checkVBCable(result);

    return result;
}

} // namespace BlueDrop
