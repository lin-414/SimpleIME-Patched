#ifndef TSF_LANGPROFILEUTIL_H
#define TSF_LANGPROFILEUTIL_H

#pragma once

#include "LangProfile.h"
#include "TsfSupport.h"
#include "core/State.h"

#include <msctf.h>
#include <windows.h>

namespace Ime
{
class InputMethodManager : public ITfInputProcessorProfileActivationSink
{
    using State = Core::State;

    static constexpr auto DEFAULT_PROFILE_INDEX = 0;

public:
    InputMethodManager() = default;

    virtual ~InputMethodManager() { UnInitialize(); }

    InputMethodManager(const InputMethodManager &other)                         = delete;
    InputMethodManager(InputMethodManager &&other) noexcept                     = delete;
    auto operator=(const InputMethodManager &other) -> InputMethodManager &     = delete;
    auto operator=(InputMethodManager &&other) noexcept -> InputMethodManager & = delete;

    auto Initialize(ITfThreadMgr *threadMgr, TfClientId clientId) -> HRESULT;
    auto UnInitialize() -> void;

    auto RefreshProfiles() -> bool;
    auto UpdateActiveProfile() noexcept -> bool;
    auto ActivateProfile(const GUID &guidProfile) -> HRESULT;
    auto ActivateKeyboardEng() -> HRESULT;

private:
    auto ActivateProfile(const LangProfile &langProfile) -> HRESULT;

public:
    auto GetActiveLangProfile() -> const LangProfile &;

    /// GUID of the last real TIP (input processor, e.g. WeChat/Microsoft Pinyin)
    /// that activated in this process. Unlike the active-profile cache it is NOT
    /// overwritten when a plain keyboard layout activates, so it always answers
    /// "which IME did the user last use" — used to restore the TIP on re-enable.
    [[nodiscard]] auto GetLastTipProfileGuid() const -> const GUID & { return m_lastTipProfileGuid; }

    [[nodiscard]] auto GetLangProfiles() const -> const std::vector<LangProfile> & { return m_langProfiles; }

    auto QueryInterface(const IID &riid, void **ppvObject) -> HRESULT override;
    auto AddRef() -> ULONG override;
    auto Release() -> ULONG override;
    auto OnActivated(DWORD dwProfileType, LANGID langid, const IID &clsid, const GUID &catid, const GUID &guidProfile, HKL hkl, DWORD dwFlags)
        -> HRESULT override;

private:
    auto UpdateConversionAndKeyboard(State &state) -> void;

    std::vector<LangProfile>             m_langProfiles;
    CComPtr<ITfInputProcessorProfileMgr> m_tfProfileMgr = nullptr;
    CComPtr<ITfThreadMgr>                m_threadMgr    = nullptr;
    TfClientId                           m_clientId;
    DWORD                                m_refCount{};
    DWORD                                m_dwCookie{};
    uint32_t                             m_activatedProfile = 0;
    GUID                                 m_lastTipProfileGuid{GUID_NULL};
};
} // namespace Ime

#endif
