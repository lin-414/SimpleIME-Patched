#include "ime/ImeManager.h"

#include "FakeDirectInputDevice.h"
#include "ImeWnd.hpp"
#include "RE/ControlMap.h"
#include "imguiex/ErrorNotifier.h"
#include "log.h"

#include <processthreadsapi.h>
#include <windows.h>

namespace Ime
{

auto ImeManager::Focus(const HWND hwnd) -> bool
{
    auto        hwndThread      = GetWindowThreadProcessId(hwnd, nullptr);
    const DWORD currentThreadId = GetCurrentThreadId();
    bool        success         = true;
    if (hwndThread != currentThreadId)
    {
        success = AttachThreadInput(hwndThread, currentThreadId, TRUE) != FALSE;
        if (success)
        {
            SetFocus(hwnd);
            // Always detach, even if SetFocus throws or returns early.
            AttachThreadInput(hwndThread, currentThreadId, FALSE);
        }
    }
    else
    {
        SetFocus(hwnd);
    }
    return success;
}

auto ImeManager::EnableIme(bool enable) -> Result
{
    logger::debug("ImeManager::{} {}", __func__, enable ? "enable" : "disable");

    // Do nothing if caller want to disable IME.
    if (enable)
    {
        if (const auto result = TryFocusIme(); IImeModule::IsFailed(result))
        {
            ErrorNotifier::GetInstance().Error("Can't focus to IME thread. IME can't work!");
            return result;
        }
    }

    auto &state = State::GetInstance();
    if (m_isForceUpdate || (state.Has(State::IME_DISABLED) && enable) || (state.NotHas(State::IME_DISABLED) && !enable))
    {
        m_isForceUpdate = false;

        bool success = false;
        if (enable)
        {
            logger::info("IME enabled: clearing IME_DISABLED, focusing TSF");
            state.Clear(State::IME_DISABLED);
            success = m_imeWnd->FocusTextService(true);
            // Restore the input method that was active before IME was disabled.
            // Without this, the system TIP stays on the English keyboard after
            // EnableIme(false) switched it away, and typing in a text field
            // would be stuck in English mode.
            if (m_lastActiveProfile != GUID_NULL)
            {
                if (FAILED(m_imeWnd->ActivateLanguageProfile(m_lastActiveProfile)))
                {
                    logger::warn("Failed to restore input method profile after enabling IME");
                }
                m_lastActiveProfile = GUID_NULL;
            }
        }
        else
        {
            logger::info("IME disabled: setting IME_DISABLED, clearing TSF focus");
            state.Set(State::IME_DISABLED);
            success = m_imeWnd->FocusTextService(false);
            // Return the Win32 focus to the game window. EnableIme(true) always
            // focuses the ImeWnd via TryFocusIme(); the disable path must be
            // symmetric, otherwise the hidden 0x0 ImeWnd keeps the keyboard
            // focus and the game never receives WM_CHAR again ("keyboard stuck").
            Focus(m_gameHwnd);

            // The system input method (e.g. WeChat/Microsoft Pinyin) follows the
            // Win32 focus back to the game window and intercepts WASD/keys there,
            // popping its candidate window or swallowing input. Switch the system
            // TIP back to the English keyboard so the game receives raw keys. The
            // profile switch (TF_IPPMF_FORSESSION) only affects this session; the
            // user's global input method is untouched. InputMethodManager's
            // OnActivated callback will clear IN_COMPOSING/IN_CAND_CHOOSING as a
            // side effect, which also fixes a stuck typing state after closing a
            // mod window while composing. Uses the real English keyboard profile
            // from the TSF enumeration (with actual CLSID/GUID), not the stub
            // DEFAULT_LANG_PROFILE which may fail to activate.
            {
                // Remember the IME the user was using, so EnableIme(true) can restore it.
                const auto &activeLangProfile = m_imeWnd->GetActiveLangProfile();
                if (activeLangProfile.guidProfile != GUID_NULL)
                {
                    m_lastActiveProfile = activeLangProfile.guidProfile;
                }
                if (FAILED(m_imeWnd->ActivateLanguageProfile(DEFAULT_LANG_PROFILE.guidProfile)))
                {
                    logger::warn("Failed to switch back to the English keyboard after disabling IME");
                }
            }
        }
        if (m_settings.autoToggleKeyboard)
        {
            logger::debug("Toggle the keyboard state...");
            m_imeWnd->ToggleKeyboard(enable);
        }

        if (!success)
        {
            state.Set(State::IME_DISABLED);
            logger::error("Enable IME failed! last error {}", GetLastError());
        }

        return ToResult(success);
    }
    return Result::SUCCESS;
}

auto ImeManager::ForceFocusIme() -> Result
{
    logger::debug("ImeManager::{}", __func__);
    m_imeWnd->Focus();
    return Result::SUCCESS;
}

auto ImeManager::SyncImeState() -> Result
{
    logger::debug("ImeManager::{}", __func__);
    m_isForceUpdate = true;
    return EnableIme(IsShouldEnableIme());
}

auto ImeManager::TryFocusIme() -> Result
{
    logger::debug("ImeManager::{}", __func__);

    // FIX: promote the game's TOP-LEVEL window to foreground first.
    // SetCooperativeLevel(NONEXCLUSIVE|BACKGROUND) fails silently
    // (GetLastError() == 0) unless the target window belongs to the
    // foreground process. SetForegroundWindow promotes it, so the
    // AttachThreadInput / SetFocus in Focus() actually take effect.
    // (Focus() itself is static and only receives the hidden WS_CHILD ImeWnd
    // handle, so the foreground promotion must happen here.)
    // Note: SetForegroundWindow may legitimately return FALSE (e.g. when
    // another process owns the foreground lock) - that is not fatal by
    // itself, so we proceed but log it for diagnosability.
    if (FALSE == SetForegroundWindow(m_gameHwnd))
    {
        logger::debug("SetForegroundWindow(game {:p}) returned FALSE; continuing anyway", static_cast<void *>(m_gameHwnd));
    }

    return ToResult(Focus(m_imeWnd->GetHWND()));
}

auto ImeManager::IsShouldEnableIme() const -> bool
{
    return m_settings.input.keepImeOpen || ControlMap::GetSingleton()->HasTextEntry();
}

} // namespace Ime
