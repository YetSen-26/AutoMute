#include "AudioSessions.h"

#include <mmdeviceapi.h>
#include <algorithm>

namespace {
	std::wstring ToLower(std::wstring input) {
		std::transform(input.begin(), input.end(), input.begin(), ::towlower);
		return input;
	}

	std::wstring BaseNameFromPath(const std::wstring& path) {
		size_t pos = path.find_last_of(L"\\/");
		if (pos == std::wstring::npos) return path;
		return path.substr(pos + 1);
	}
}

namespace AudioSessions {
	ComInitGuard::ComInitGuard() {
		HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		if (hr == RPC_E_CHANGED_MODE) {
			hr = CoInitialize(nullptr);
		}
		m_ok = SUCCEEDED(hr);
		m_should_uninit = (hr == S_OK || hr == S_FALSE);
	}

	ComInitGuard::~ComInitGuard() {
		if (m_should_uninit) {
			CoUninitialize();
		}
	}

	bool GetAllAudioSessionManagers(std::vector<IAudioSessionManager2*>& sessionManagers) {
		IMMDeviceEnumerator* enumerator = nullptr;
		IMMDeviceCollection* collection = nullptr;

		HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
			__uuidof(IMMDeviceEnumerator), (void**)&enumerator);
		if (FAILED(hr) || !enumerator) return false;

		hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
		if (FAILED(hr) || !collection) {
			enumerator->Release();
			return false;
		}

		UINT count = 0;
		collection->GetCount(&count);
		for (UINT i = 0; i < count; ++i) {
			IMMDevice* device = nullptr;
			if (SUCCEEDED(collection->Item(i, &device)) && device) {
				IAudioSessionManager2* sessionManager = nullptr;
				if (SUCCEEDED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&sessionManager)) && sessionManager) {
					sessionManagers.push_back(sessionManager);
				}
				device->Release();
			}
		}

		collection->Release();
		enumerator->Release();
		return true;
	}

	void ReleaseAllAudioSessionManagers(std::vector<IAudioSessionManager2*>& sessionManagers) {
		for (auto* manager : sessionManagers) {
			if (manager) manager->Release();
		}
		sessionManagers.clear();
	}

	void EnumAllAudioSessions(const std::vector<IAudioSessionManager2*>& sessionManagers,
		std::function<void(ISimpleAudioVolume* audioVolume, DWORD processId)> callback) {
		for (auto* manager : sessionManagers) {
			if (!manager) continue;

			IAudioSessionEnumerator* sessionEnumerator = nullptr;
			if (FAILED(manager->GetSessionEnumerator(&sessionEnumerator)) || !sessionEnumerator) continue;

			int sessionCount = 0;
			sessionEnumerator->GetCount(&sessionCount);
			for (int i = 0; i < sessionCount; ++i) {
				IAudioSessionControl* sessionControl = nullptr;
				if (FAILED(sessionEnumerator->GetSession(i, &sessionControl)) || !sessionControl) continue;

				IAudioSessionControl2* sessionControl2 = nullptr;
				if (FAILED(sessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&sessionControl2)) || !sessionControl2) {
					sessionControl->Release();
					continue;
				}

				DWORD sessionProcessId = 0;
				if (FAILED(sessionControl2->GetProcessId(&sessionProcessId)) || sessionProcessId <= 4) {
					sessionControl2->Release();
					sessionControl->Release();
					continue;
				}

				ISimpleAudioVolume* audioVolume = nullptr;
				if (SUCCEEDED(sessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&audioVolume)) && audioVolume) {
					callback(audioVolume, sessionProcessId);
					audioVolume->Release();
				}

				sessionControl2->Release();
				sessionControl->Release();
			}

			sessionEnumerator->Release();
		}
	}

	std::wstring GetProcessImageNameLower(DWORD processId) {
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
		if (!hProcess) return L"";

		WCHAR buffer[MAX_PATH] = { 0 };
		DWORD size = MAX_PATH;
		std::wstring result;
		if (QueryFullProcessImageNameW(hProcess, 0, buffer, &size)) {
			result.assign(buffer, size);
			result = BaseNameFromPath(result);
			result = ToLower(result);
		}
		CloseHandle(hProcess);
		return result;
	}

	std::string WStringToUtf8(const std::wstring& wstr) {
		if (wstr.empty()) return std::string();
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
		std::string str(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, NULL, NULL);
		return str;
	}

	void UnmuteAllSessions(const std::vector<IAudioSessionManager2*>& sessionManagers) {
		EnumAllAudioSessions(sessionManagers, [](ISimpleAudioVolume* audioVolume, DWORD) {
			if (!audioVolume) return;
			audioVolume->SetMute(FALSE, nullptr);
		});
	}
}
