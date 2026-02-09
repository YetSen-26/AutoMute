#pragma once

#include <windows.h>
#include <audiopolicy.h>
#include <vector>
#include <functional>
#include <string>

namespace AudioSessions {
	class ComInitGuard {
	public:
		ComInitGuard();
		~ComInitGuard();
		bool ok() const { return m_ok; }
	private:
		bool m_ok = false;
		bool m_should_uninit = false;
	};

	bool GetAllAudioSessionManagers(std::vector<IAudioSessionManager2*>& sessionManagers);
	void ReleaseAllAudioSessionManagers(std::vector<IAudioSessionManager2*>& sessionManagers);

	void EnumAllAudioSessions(const std::vector<IAudioSessionManager2*>& sessionManagers,
		std::function<void(ISimpleAudioVolume* audioVolume, DWORD processId)> callback);

	std::wstring GetProcessImageNameLower(DWORD processId);
	std::string WStringToUtf8(const std::wstring& wstr);

	void UnmuteAllSessions(const std::vector<IAudioSessionManager2*>& sessionManagers);
}
