#pragma once

#include "IImeModule.h"
#include "ime/ImeManager.h"
#include "ui/Settings.h"
#include "ui/TaskQueue.h"

#include <atomic>

namespace Ime
{

class ImeWnd;

struct SettingsConfig;

class ImeController final
{
public:
    void ApplySettings();

    void SaveSettings(Settings &settings) const
    {
        if (IsReady())
        {
            settings.enableMod = m_fEnabledMod.load();
        }
    }

    void SyncImeStateIfDirty()
    {
        if (m_fDirty.load())
        {
            m_fDirty.store(false);
            SyncImeState();
        }
    }

    void MarkDirty() { m_fDirty.store(true); }

    [[nodiscard]] auto IsDirty() const -> bool { return m_fDirty.load(); }

    auto IsReady() const -> bool { return m_fInited && (m_imeWnd != nullptr); }

    auto IsModEnabled() const -> bool { return m_fEnabledMod.load(); }

    /**
     * notify @c ImeWnd activate a @c LangProfile by specify guid.
     * @param guidProfile the @c LangProfile guid
     */
    void ActivateLangProfile(const GUID &guidProfile) const;
    auto CommitCandidate(DWORD index) const -> void;
    auto SetConversionMode(DWORD conversionMode) const -> void;

    //////////////////////////////////////////////////

    auto EnableIme(bool enable) const -> void;
    auto ForceFocusIme() const -> void;
    auto SyncImeState() -> void;
    auto TryFocusIme() const -> void;
    auto EnableMod(bool enable) -> void;

    [[nodiscard]] auto IsInited() const -> bool { return m_fInited; }

    static auto GetInstance() -> ImeController *
    {
        static ImeController g_instance;
        return &g_instance;
    }

    void Init(ImeWnd *imeWnd, HWND gameHwnd, Settings &settings);
    void Shutdown();

private:
    Settings                   *m_settings    = nullptr;
    std::unique_ptr<ImeManager> m_delegate    = nullptr;
    ImeWnd                     *m_imeWnd      = nullptr;
    HWND                        m_gameHwnd    = nullptr;
    HIMC                        m_gameHIMC    = nullptr;
    std::atomic_bool            m_fDirty      = false;
    std::atomic_bool            m_fEnabledMod = false;
    std::atomic_bool            m_fInited     = false;

    auto DoEnableMod(bool enable) -> IImeModule::Result;
    auto DoEnableIme(bool enable, bool honorKeepImeOpen = true) const -> IImeModule::Result;
    auto DoForceFocusIme() const -> IImeModule::Result;
    auto DoSyncImeState() -> IImeModule::Result;
    auto DoTryFocusIme() const -> IImeModule::Result;

    auto UnlockKeyboard() const -> bool;
    auto RestoreKeyboard() const -> bool;

    void AddTask(TaskQueue::Task &&task) const;
};
} // namespace Ime
