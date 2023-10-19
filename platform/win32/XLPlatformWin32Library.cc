/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XLPlatformWin32Library.h"

#define interface __STRUCT__
#include <netlistmgr.h>

#include <windows.h>
#include <winhttp.h>
#include <netlistmgr.h>
#include <wil/result_macros.h>
#include <wil/stl.h>
#include <wil/resource.h>
#include <wil/com.h>
#include <wrl.h>
#include <string>
#include <iostream>
#include <sstream>
#include <stdlib.h>

#undef interface

namespace Utility {

enum class CostGuidance {
	Normal,
	Roaming,
	Metered,
	Conservative
};

enum class ConnectivityType {
	Disconnected,
	Normal,
	Local,
	CaptivePortal,
};

ConnectivityType ShouldAttemptToConnectToInternet(NLM_CONNECTIVITY connectivity,
		INetworkListManager *networkListManager) {
	// check internet connectivity
	if (WI_IsAnyFlagSet(connectivity, NLM_CONNECTIVITY_IPV4_INTERNET | NLM_CONNECTIVITY_IPV6_INTERNET)) {
		return ConnectivityType::Normal;
	} else if (WI_IsAnyFlagSet(connectivity, NLM_CONNECTIVITY_IPV4_LOCALNETWORK | NLM_CONNECTIVITY_IPV6_LOCALNETWORK)) {
		// we are local connected, check if we're behind a captive portal before attempting to connect to the Internet.
		//
		// note: being behind a captive portal means connectivity is local and there is at least one interface(network)
		// behind a captive portal.

		bool localConnectedBehindCaptivePortal = false;
		wil::com_ptr<IEnumNetworks> enumConnectedNetworks;
		THROW_IF_FAILED(
				networkListManager->GetNetworks(NLM_ENUM_NETWORK_CONNECTED,
						enumConnectedNetworks.put()));

		// Enumeration returns S_FALSE when there are no more items.
		wil::com_ptr<INetwork> networkConnection;
		while (THROW_IF_FAILED(
				enumConnectedNetworks->Next(1, networkConnection.put(),
						nullptr)) == S_OK) {
			wil::com_ptr<IPropertyBag> networkProperties =
					networkConnection.query<IPropertyBag>();

			// these might fail if there's no value
			wil::unique_variant variantInternetConnectivityV4;
			networkProperties->Read(NA_InternetConnectivityV4,
					variantInternetConnectivityV4.addressof(), nullptr);
			wil::unique_variant variantInternetConnectivityV6;
			networkProperties->Read(NA_InternetConnectivityV6,
					variantInternetConnectivityV6.addressof(), nullptr);

			// read the VT_UI4 from the VARIANT and cast it to a NLM_INTERNET_CONNECTIVITY
			// If there is no value, then assume no special treatment.
			NLM_INTERNET_CONNECTIVITY v4Connectivity =
					static_cast<NLM_INTERNET_CONNECTIVITY>(
							variantInternetConnectivityV6.vt == VT_UI4 ?
									variantInternetConnectivityV4.ulVal : 0);
			NLM_INTERNET_CONNECTIVITY v6Connectivity =
					static_cast<NLM_INTERNET_CONNECTIVITY>(
							variantInternetConnectivityV6.vt == VT_UI4 ?
									variantInternetConnectivityV6.ulVal : 0);

			if (WI_IsFlagSet(v4Connectivity,
					NLM_INTERNET_CONNECTIVITY_WEBHIJACK)
					|| WI_IsFlagSet(v6Connectivity,
							NLM_INTERNET_CONNECTIVITY_WEBHIJACK)) {
				// at least one connected interface is behind a captive portal
				// we should assume that the device is behind it
				localConnectedBehindCaptivePortal = true;
			}
		}

		if (!localConnectedBehindCaptivePortal) {
			return ConnectivityType::Local;
		} else {
			return ConnectivityType::CaptivePortal;
		}
	}
	return ConnectivityType::Disconnected;
}

bool SendHttpGetRequest() {
	wil::unique_winhttp_hinternet session(
			WinHttpOpen(L"NetworkListManagerSample.exe",
					WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
					WINHTTP_NO_PROXY_BYPASS, 0));

	if (!session) {
		return false;
	}

	wil::unique_winhttp_hinternet connect(
			WinHttpConnect(session.get(), L"www.msftconnecttest.com",
					INTERNET_DEFAULT_HTTP_PORT, 0));
	if (!connect) {
		return false;
	}

	wil::unique_winhttp_hinternet request(
			WinHttpOpenRequest(connect.get(), L"GET", L"/connecttest.txt",
					nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
					0));
	if (!request) {
		return false;
	}

	if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
		return false;
	}

	if (!WinHttpReceiveResponse(request.get(), nullptr)) {
		return false;
	}

	DWORD statusCode { 0 };
	DWORD headerBytes = sizeof(statusCode);
	if (WinHttpQueryHeaders(request.get(),
			WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL,
			&statusCode, &headerBytes, WINHTTP_NO_HEADER_INDEX)) {
		if (statusCode >= 200 && statusCode < 300) {
			DWORD bytesRead { 0 };
			if (WinHttpQueryDataAvailable(request.get(), &bytesRead)
					&& bytesRead > 0) {
				std::unique_ptr<BYTE[]> readBuffer = std::make_unique<BYTE[]>(
						bytesRead);
				if (WinHttpReadData(request.get(), readBuffer.get(), bytesRead,
						&bytesRead)) {
					std::wcout << "Received " << bytesRead
							<< " bytes in response." << std::endl;
					return true;
				} else {
					return false;
				}
			} else {
				return false;
			}
		} else {
			return true;
		}
	} else {
		return false;
	}
}

void EvaluateCostAndConnect(stappler::xenolith::platform::NetworkCapabilities &ret, INetworkListManager *networkListManager) {
	wil::com_ptr<INetworkCostManager> netCostManager = wil::com_query < INetworkCostManager > (networkListManager);

	DWORD cost { 0 };
	THROW_IF_FAILED(netCostManager->GetCost(&cost, nullptr));
	const auto nlmConnectionCost = static_cast<NLM_CONNECTION_COST>(cost);

	ret |= stappler::xenolith::platform::NetworkCapabilities::NotCongested;
	ret |= stappler::xenolith::platform::NetworkCapabilities::NotMetered;
	ret |= stappler::xenolith::platform::NetworkCapabilities::NotRoaming;
	ret |= stappler::xenolith::platform::NetworkCapabilities::NotSuspended;

	if ((nlmConnectionCost & NLM_CONNECTION_COST_UNRESTRICTED)) {
		ret |= stappler::xenolith::platform::NetworkCapabilities::NotRestricted;
	}

	if ((nlmConnectionCost & NLM_CONNECTION_COST_FIXED)) {
		ret |= stappler::xenolith::platform::NetworkCapabilities::TemporarilyNotMetered;
	}

	if ((nlmConnectionCost & NLM_CONNECTION_COST_VARIABLE)) {
		ret &= ~stappler::xenolith::platform::NetworkCapabilities::NotMetered;
	}

	if ((nlmConnectionCost & NLM_CONNECTION_COST_OVERDATALIMIT)) {
		ret &= ~stappler::xenolith::platform::NetworkCapabilities::NotMetered;
		ret &= ~stappler::xenolith::platform::NetworkCapabilities::NotSuspended;
	}

	if ((nlmConnectionCost & NLM_CONNECTION_COST_CONGESTED)) {
		ret &= ~stappler::xenolith::platform::NetworkCapabilities::NotCongested;
	}

	if ((nlmConnectionCost & NLM_CONNECTION_COST_ROAMING)) {
		ret &= ~stappler::xenolith::platform::NetworkCapabilities::NotRoaming;
	}

	if ((nlmConnectionCost & NLM_CONNECTION_COST_APPROACHINGDATALIMIT)) {
		ret |= stappler::xenolith::platform::NetworkCapabilities::TemporarilyNotMetered;
	}

	if (!(nlmConnectionCost & NLM_CONNECTION_COST_OVERDATALIMIT)) {
		if (SendHttpGetRequest()) {
			ret |= stappler::xenolith::platform::NetworkCapabilities::Validated;
		}
	}
}

stappler::xenolith::platform::NetworkCapabilities EvaluateAndReportConnectivity(NLM_CONNECTIVITY connectivity, INetworkListManager *networkListManager) {
	stappler::xenolith::platform::NetworkCapabilities ret = stappler::xenolith::platform::NetworkCapabilities::None;
	ConnectivityType connectivityType = Utility::ShouldAttemptToConnectToInternet(connectivity, networkListManager);

	switch (connectivityType) {
	case ConnectivityType::Disconnected:
		break;
	case ConnectivityType::Normal:
		ret |= stappler::xenolith::platform::NetworkCapabilities::Internet;
		break;
	case ConnectivityType::Local:
		ret |= stappler::xenolith::platform::NetworkCapabilities::Local;
		break;
	case ConnectivityType::CaptivePortal:
		ret |= stappler::xenolith::platform::NetworkCapabilities::Internet;
		ret |= stappler::xenolith::platform::NetworkCapabilities::CaptivePortal;
		break;
	}

	if (connectivityType == ConnectivityType::Disconnected || connectivityType == ConnectivityType::CaptivePortal) {
		Utility::EvaluateCostAndConnect(ret, networkListManager);
	} else {
		std::wcout << "Not attempting to connect to the Internet." << std::endl;
	}

	return ret;
}

using unique_connectionpoint_token = wil::unique_com_token<IConnectionPoint, DWORD, decltype(&IConnectionPoint::Unadvise), &IConnectionPoint::Unadvise>;

unique_connectionpoint_token FindConnectionPointAndAdvise(REFIID itf, IUnknown *source, IUnknown *sink) {
	wil::com_ptr<IConnectionPointContainer> container = wil::com_query<
			IConnectionPointContainer>(source);
	wil::com_ptr<IConnectionPoint> connectionPoint;
	THROW_IF_FAILED(container->FindConnectionPoint(itf, connectionPoint.put()));

	unique_connectionpoint_token token { connectionPoint.get() };
	THROW_IF_FAILED(connectionPoint->Advise(sink, token.put()));
	return token;
}

// typename T is the connection point interface we are connecting to.
template<typename T>
unique_connectionpoint_token FindConnectionPointAndAdvise(IUnknown *source, IUnknown *sink) {
	return FindConnectionPointAndAdvise(__uuidof(T), source, sink);
}

class NetworkConnectivityListener final : public Microsoft::WRL::RuntimeClass<
		Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
		INetworkListManagerEvents> {
public:
	NetworkConnectivityListener(std::function<void(stappler::xenolith::platform::NetworkCapabilities)> &&cb,
			INetworkListManager *networkListManager) :
			m_networkListManager(networkListManager), m_callback(move(cb)) {
	}

	NetworkConnectivityListener(const NetworkConnectivityListener&) = delete;
	NetworkConnectivityListener& operator=(const NetworkConnectivityListener&) = delete;

	IFACEMETHODIMP ConnectivityChanged(NLM_CONNECTIVITY connectivity) noexcept
			override
	try {
		m_callback(Utility::EvaluateAndReportConnectivity(connectivity, m_networkListManager.get()));
		return S_OK;
	}
	CATCH_RETURN()
	;

private:
	wil::com_ptr<INetworkListManager> m_networkListManager;
	std::function<void(stappler::xenolith::platform::NetworkCapabilities)> m_callback;
};

}

namespace stappler::xenolith::platform {

struct Win32Library::Data {
	std::mutex _mutex;
	const wil::com_ptr<INetworkListManager> networkListManager;
	Utility::unique_connectionpoint_token token;
	NetworkCapabilities capabilities;

    struct StateCallback {
    	Function<void(const NetworkCapabilities &)> callback;
    	Rc<Ref> ref;
    };

    Map<void *, StateCallback> networkCallbacks;

	Data() : networkListManager(wil::CoCreateInstance<NetworkListManager, INetworkListManager>()) {
		token = FindConnectionPointAndAdvise < INetworkListManagerEvents
					> (networkListManager.get(),
							Microsoft::WRL::Make<Utility::NetworkConnectivityListener>(
									[this] (NetworkCapabilities caps) {
												updateCapabilities(caps);
											},
									networkListManager.get()).Get());

		NLM_CONNECTIVITY connectivity { NLM_CONNECTIVITY_DISCONNECTED };
		THROW_IF_FAILED(networkListManager->GetConnectivity(&connectivity));

		capabilities = Utility::EvaluateAndReportConnectivity(connectivity, networkListManager.get());
	}

	void updateCapabilities(NetworkCapabilities caps) {
		std::unique_lock lock(_mutex);
		capabilities = caps;
		std::wcout << L"INetworkListManagerEvents::ConnectivityChanged" << std::endl;

		for (auto &it : networkCallbacks) {
			it.second.callback(capabilities);
		}
	}
};

static Win32Library *s_Win32Library = nullptr;

Win32Library *Win32Library::getInstance() {
	return s_Win32Library;
}

Win32Library::~Win32Library() {
	if (_data) {
		delete _data;
		_data = nullptr;
	}
	if (s_Win32Library == this) {
		s_Win32Library = nullptr;
	}
}

bool Win32Library::init() {
	s_Win32Library = this;
	loadKeycodes();

	_data = new Data;

	return true;
}

// based on https://github.com/glfw/glfw/blob/master/src/win32_monitor.c

static Win32Display createMonitor(DISPLAY_DEVICEW *adapter, DISPLAY_DEVICEW *display) {
	Win32Display ret;
	if (display) {
		ret.name = WideString((const char16_t *)display->DeviceString);
	} else {
		ret.name = WideString((const char16_t *)adapter->DeviceString);
	}
	if (ret.name.empty())
		return ret;

	ZeroMemory(&ret.dm, sizeof(ret.dm));
	ret.dm.dmSize = sizeof(ret.dm);
	EnumDisplaySettingsW(adapter->DeviceName, ENUM_CURRENT_SETTINGS, &ret.dm);

	auto dc = CreateDCW(L"DISPLAY", adapter->DeviceName, NULL, NULL);

	ret.widthMM = GetDeviceCaps(dc, HORZSIZE);
	ret.heightMM = GetDeviceCaps(dc, VERTSIZE);

	DeleteDC(dc);

	ret.modesPruned = (adapter->StateFlags & DISPLAY_DEVICE_MODESPRUNED);
	ret.isPrimary = (adapter->StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE);

	ret.adapterName = WideString((const char16_t *)adapter->DeviceString);
	if (display) {
		ret.displayName = WideString((const char16_t *)display->DeviceString);
	}

	ret.rect.left = ret.dm.dmPosition.x;
	ret.rect.top = ret.dm.dmPosition.y;
	ret.rect.right = ret.dm.dmPosition.x + ret.dm.dmPelsWidth;
	ret.rect.bottom = ret.dm.dmPosition.y + ret.dm.dmPelsHeight;

	//EnumDisplayMonitors(NULL, &ret.rect, monitorCallback, (LPARAM) &ret);
	return ret;
}

Vector<Win32Display> Win32Library::pollMonitors() {
	Vector<Win32Display> ret;
	DWORD adapterIndex, displayIndex;
	DISPLAY_DEVICEW adapter, display;

	for (adapterIndex = 0;; adapterIndex++) {
		ZeroMemory(&adapter, sizeof(adapter));
		adapter.cb = sizeof(adapter);

		if (!EnumDisplayDevicesW(NULL, adapterIndex, &adapter, 0))
			break;

		if (!(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE))
			continue;

		for (displayIndex = 0;; displayIndex++) {
			ZeroMemory(&display, sizeof(display));
			display.cb = sizeof(display);

			if (!EnumDisplayDevicesW(adapter.DeviceName, displayIndex, &display, 0))
				break;

			if (!(display.StateFlags & DISPLAY_DEVICE_ACTIVE))
				continue;

			auto monitor = createMonitor(&adapter, &display);
			if (!monitor.name.empty()) {
				if ((adapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)) {
					ret.emplace(ret.begin(), move(monitor));
				} else {
					ret.emplace_back(move(monitor));
				}
			}
		}

		// HACK: If an active adapter does not have any display devices
		//       (as sometimes happens), add it directly as a monitor
		if (displayIndex == 0) {
			auto monitor = createMonitor(&adapter, NULL);
			if (!monitor.name.empty()) {
				if ((adapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)) {
					ret.emplace(ret.begin(), move(monitor));
				} else {
					ret.emplace_back(move(monitor));
				}
			}
		}
	}
	return ret;
}

void Win32Library::addNetworkConnectionCallback(void *key, Function<void(const NetworkCapabilities &)> &&cb) {
	std::unique_lock lock(_data->_mutex);
	cb(_data->capabilities);
	_data->networkCallbacks.emplace(key, Data::StateCallback{move(cb), this});
}

void Win32Library::removeNetworkConnectionCallback(void *key) {
	std::unique_lock lock(_data->_mutex);
	auto refId = retain();
	_data->networkCallbacks.erase(key);
	release(refId);
}

void Win32Library::loadKeycodes() {
	memset(_keycodes, 0, sizeof(_keycodes));
	memset(_scancodes, 0, sizeof(_scancodes));

	_keycodes[0x00B] = core::InputKeyCode::_0;
	_keycodes[0x002] = core::InputKeyCode::_1;
	_keycodes[0x003] = core::InputKeyCode::_2;
	_keycodes[0x004] = core::InputKeyCode::_3;
	_keycodes[0x005] = core::InputKeyCode::_4;
	_keycodes[0x006] = core::InputKeyCode::_5;
	_keycodes[0x007] = core::InputKeyCode::_6;
	_keycodes[0x008] = core::InputKeyCode::_7;
	_keycodes[0x009] = core::InputKeyCode::_8;
	_keycodes[0x00A] = core::InputKeyCode::_9;
	_keycodes[0x01E] = core::InputKeyCode::A;
	_keycodes[0x030] = core::InputKeyCode::B;
	_keycodes[0x02E] = core::InputKeyCode::C;
	_keycodes[0x020] = core::InputKeyCode::D;
	_keycodes[0x012] = core::InputKeyCode::E;
	_keycodes[0x021] = core::InputKeyCode::F;
	_keycodes[0x022] = core::InputKeyCode::G;
	_keycodes[0x023] = core::InputKeyCode::H;
	_keycodes[0x017] = core::InputKeyCode::I;
	_keycodes[0x024] = core::InputKeyCode::J;
	_keycodes[0x025] = core::InputKeyCode::K;
	_keycodes[0x026] = core::InputKeyCode::L;
	_keycodes[0x032] = core::InputKeyCode::M;
	_keycodes[0x031] = core::InputKeyCode::N;
	_keycodes[0x018] = core::InputKeyCode::O;
	_keycodes[0x019] = core::InputKeyCode::P;
	_keycodes[0x010] = core::InputKeyCode::Q;
	_keycodes[0x013] = core::InputKeyCode::R;
	_keycodes[0x01F] = core::InputKeyCode::S;
	_keycodes[0x014] = core::InputKeyCode::T;
	_keycodes[0x016] = core::InputKeyCode::U;
	_keycodes[0x02F] = core::InputKeyCode::V;
	_keycodes[0x011] = core::InputKeyCode::W;
	_keycodes[0x02D] = core::InputKeyCode::X;
	_keycodes[0x015] = core::InputKeyCode::Y;
	_keycodes[0x02C] = core::InputKeyCode::Z;

	_keycodes[0x028] = core::InputKeyCode::APOSTROPHE;
	_keycodes[0x02B] = core::InputKeyCode::BACKSLASH;
	_keycodes[0x033] = core::InputKeyCode::COMMA;
	_keycodes[0x00D] = core::InputKeyCode::EQUAL;
	_keycodes[0x029] = core::InputKeyCode::GRAVE_ACCENT;
	_keycodes[0x01A] = core::InputKeyCode::LEFT_BRACKET;
	_keycodes[0x00C] = core::InputKeyCode::MINUS;
	_keycodes[0x034] = core::InputKeyCode::PERIOD;
	_keycodes[0x01B] = core::InputKeyCode::RIGHT_BRACKET;
	_keycodes[0x027] = core::InputKeyCode::SEMICOLON;
	_keycodes[0x035] = core::InputKeyCode::SLASH;
	_keycodes[0x056] = core::InputKeyCode::WORLD_2;

	_keycodes[0x00E] = core::InputKeyCode::BACKSPACE;
	_keycodes[0x153] = core::InputKeyCode::DELETE;
	_keycodes[0x14F] = core::InputKeyCode::END;
	_keycodes[0x01C] = core::InputKeyCode::ENTER;
	_keycodes[0x001] = core::InputKeyCode::ESCAPE;
	_keycodes[0x147] = core::InputKeyCode::HOME;
	_keycodes[0x152] = core::InputKeyCode::INSERT;
	_keycodes[0x15D] = core::InputKeyCode::MENU;
	_keycodes[0x151] = core::InputKeyCode::PAGE_DOWN;
	_keycodes[0x149] = core::InputKeyCode::PAGE_UP;
	_keycodes[0x045] = core::InputKeyCode::PAUSE;
	_keycodes[0x039] = core::InputKeyCode::SPACE;
	_keycodes[0x00F] = core::InputKeyCode::TAB;
	_keycodes[0x03A] = core::InputKeyCode::CAPS_LOCK;
	_keycodes[0x145] = core::InputKeyCode::NUM_LOCK;
	_keycodes[0x046] = core::InputKeyCode::SCROLL_LOCK;
	_keycodes[0x03B] = core::InputKeyCode::F1;
	_keycodes[0x03C] = core::InputKeyCode::F2;
	_keycodes[0x03D] = core::InputKeyCode::F3;
	_keycodes[0x03E] = core::InputKeyCode::F4;
	_keycodes[0x03F] = core::InputKeyCode::F5;
	_keycodes[0x040] = core::InputKeyCode::F6;
	_keycodes[0x041] = core::InputKeyCode::F7;
	_keycodes[0x042] = core::InputKeyCode::F8;
	_keycodes[0x043] = core::InputKeyCode::F9;
	_keycodes[0x044] = core::InputKeyCode::F10;
	_keycodes[0x057] = core::InputKeyCode::F11;
	_keycodes[0x058] = core::InputKeyCode::F12;
	_keycodes[0x064] = core::InputKeyCode::F13;
	_keycodes[0x065] = core::InputKeyCode::F14;
	_keycodes[0x066] = core::InputKeyCode::F15;
	_keycodes[0x067] = core::InputKeyCode::F16;
	_keycodes[0x068] = core::InputKeyCode::F17;
	_keycodes[0x069] = core::InputKeyCode::F18;
	_keycodes[0x06A] = core::InputKeyCode::F19;
	_keycodes[0x06B] = core::InputKeyCode::F20;
	_keycodes[0x06C] = core::InputKeyCode::F21;
	_keycodes[0x06D] = core::InputKeyCode::F22;
	_keycodes[0x06E] = core::InputKeyCode::F23;
	_keycodes[0x076] = core::InputKeyCode::F24;
	_keycodes[0x038] = core::InputKeyCode::LEFT_ALT;
	_keycodes[0x01D] = core::InputKeyCode::LEFT_CONTROL;
	_keycodes[0x02A] = core::InputKeyCode::LEFT_SHIFT;
	_keycodes[0x15B] = core::InputKeyCode::LEFT_SUPER;
	_keycodes[0x137] = core::InputKeyCode::PRINT_SCREEN;
	_keycodes[0x138] = core::InputKeyCode::RIGHT_ALT;
	_keycodes[0x11D] = core::InputKeyCode::RIGHT_CONTROL;
	_keycodes[0x036] = core::InputKeyCode::RIGHT_SHIFT;
	_keycodes[0x15C] = core::InputKeyCode::RIGHT_SUPER;
	_keycodes[0x150] = core::InputKeyCode::DOWN;
	_keycodes[0x14B] = core::InputKeyCode::LEFT;
	_keycodes[0x14D] = core::InputKeyCode::RIGHT;
	_keycodes[0x148] = core::InputKeyCode::UP;

	_keycodes[0x052] = core::InputKeyCode::KP_0;
	_keycodes[0x04F] = core::InputKeyCode::KP_1;
	_keycodes[0x050] = core::InputKeyCode::KP_2;
	_keycodes[0x051] = core::InputKeyCode::KP_3;
	_keycodes[0x04B] = core::InputKeyCode::KP_4;
	_keycodes[0x04C] = core::InputKeyCode::KP_5;
	_keycodes[0x04D] = core::InputKeyCode::KP_6;
	_keycodes[0x047] = core::InputKeyCode::KP_7;
	_keycodes[0x048] = core::InputKeyCode::KP_8;
	_keycodes[0x049] = core::InputKeyCode::KP_9;
	_keycodes[0x04E] = core::InputKeyCode::KP_ADD;
	_keycodes[0x053] = core::InputKeyCode::KP_DECIMAL;
	_keycodes[0x135] = core::InputKeyCode::KP_DIVIDE;
	_keycodes[0x11C] = core::InputKeyCode::KP_ENTER;
	_keycodes[0x059] = core::InputKeyCode::KP_EQUAL;
	_keycodes[0x037] = core::InputKeyCode::KP_MULTIPLY;
	_keycodes[0x04A] = core::InputKeyCode::KP_SUBTRACT;

	for (auto scancode = 0; scancode < 512; scancode++) {
		if (_keycodes[scancode] != core::InputKeyCode::Unknown) {
			_scancodes[toInt(_keycodes[scancode])] = scancode;
		}
	}
}

}
