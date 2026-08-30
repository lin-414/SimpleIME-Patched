//
// Created by jamie on 25-1-22.
//
#pragma once

#include "ImeWnd.hpp"
#include "hook.h"
#include "ui/Settings.h"

namespace Ime
{
void D3DInit();

class ImeApp
{
public:
    struct State
    {
        enum class StateKey : std::uint8_t
        {
            UNINITIALIZED,
            INITIALIZING,
            INITIALIZED,
            INITIALIZE_FAILED,
            SHUTDOWN,
            DORMANCY
        };

    private:
        std::atomic<StateKey> m_stateKey = StateKey::UNINITIALIZED;

    public:
        static constexpr auto GetStateKetText(StateKey stateKey) -> std::string
        {
            switch (stateKey)
            {
                case StateKey::INITIALIZED:
                    return "Initialized";
                case StateKey::INITIALIZING:
                    return "Initializing";
                case StateKey::INITIALIZE_FAILED:
                    return "Initialization failed";
                case StateKey::UNINITIALIZED:
                    return "Uninitialized";
                case StateKey::SHUTDOWN:
                    return "Shutdown";
                case StateKey::DORMANCY:
                    return "Dormancy";
            }
            return "Unknown state";
        }

        constexpr auto GetStateKetText() const -> std::string { return GetStateKetText(m_stateKey); }

        void SetState(const StateKey stateKey)
        {
            if (stateKey <= m_stateKey)
            {
                logger::error("The state cannot be rolled back from [{}] to [{}]!", GetStateKetText(m_stateKey), GetStateKetText(stateKey));
                return;
            }
            m_stateKey.exchange(stateKey);
        }

        constexpr auto IsUnInitialized() const { return m_stateKey == StateKey::UNINITIALIZED; }

        constexpr auto IsInitializing() const { return m_stateKey == StateKey::INITIALIZING; }

        constexpr auto IsInitializeFailed() const { return m_stateKey == StateKey::INITIALIZE_FAILED; }

        constexpr auto IsInitialized() const { return m_stateKey == StateKey::INITIALIZED; }
    };

    explicit ImeApp();
    ~ImeApp() = default;

    ImeApp(const ImeApp &other)                   = delete;
    ImeApp(ImeApp &&other)                        = delete;
    auto operator=(const ImeApp &other) -> ImeApp = delete;
    auto operator=(ImeApp &&other) -> ImeApp      = delete;

    static auto GetInstance() -> ImeApp &;

    void OnInputLoaded();
    void Draw();
    void Uninitialize();
    void SaveSettings();

    constexpr auto GetGameHWND() const -> HWND { return m_hWnd; }

    constexpr auto GetImeWnd() -> ImeWnd & { return m_imeWnd; }

    constexpr auto GetState() const -> const State & { return m_state; }

    constexpr auto GetSettings() const -> const Settings & { return m_settings; }

    constexpr auto GetSettings() -> Settings & { return m_settings; }

private:
    void OnD3DInit();
    void Start(const RE::BSGraphics::RendererData &renderData);
    void Shutdown();

    static void InstallHooks();
    static void UninstallHooks();

    Settings m_settings;
    ImeWnd   m_imeWnd{};
    HWND     m_hWnd = nullptr;
    /// Thread id of the IME message-loop worker (set by the worker itself on
    /// start). ImeApp::Shutdown posts WM_QUIT to it — a window-targeted
    /// WM_QUIT reaches the WndProc instead, which ignores it, and the loop
    /// (which only exits on a thread-queue WM_QUIT) would keep running.
    std::atomic<DWORD> m_imeThreadId{0};
    /// Set by the IME worker when its full teardown (WM_DESTROY → OnDestroy →
    /// UnInitialize) completed. Shutdown waits for it (bounded) so the
    /// game-thread Uninitialize does not race the IME-thread teardown.
    std::atomic<bool> m_imeTeardownDone{false};
    State              m_state;

    friend void           Ime::D3DInit();
    void                  DoD3DInit();
    static auto           MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT;
    static inline WNDPROC RealWndProc;
};
} // namespace Ime
