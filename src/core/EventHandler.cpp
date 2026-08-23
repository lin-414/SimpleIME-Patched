#include "core/EventHandler.h"

#include "ImeApp.h"
#include "ImeWnd.hpp"
#include "RE/ControlMap.h"
#include "ime/ImeController.h"
#include "log.h"
#include "menu/MenuNames.h"
#include "utils/InputFocusAnchor.h"
#include "utils/Utils.h"

#include <RE/B/BSInputDeviceManager.h>
#include <RE/B/BSTEvent.h>
#include <RE/B/ButtonEvent.h>
#include <RE/C/CursorMenu.h>
#include <RE/I/InputDevices.h>
#include <RE/I/InputEvent.h>
#include <RE/M/MainMenu.h>
#include <RE/U/UI.h>

namespace Ime::Events
{
class MenuOpenCloseEventSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
    using Event = RE::MenuOpenCloseEvent;

public:
    auto ProcessEvent(const Event *a_event, RE::BSTEventSource<Event> * /*a_eventSource*/) -> RE::BSEventNotifyControl override;

private:
    static void FixInconsistentTextEntryCount(const Event *event);
};

namespace
{
auto GetMenuOpenCloseEventSink() -> std::unique_ptr<MenuOpenCloseEventSink> &
{
    static std::unique_ptr<MenuOpenCloseEventSink> instance = nullptr;
    return instance;
}

void OnFirstOpenMainMenu()
{
    Skyrim::ShowMenu(ImeMenuName);
    ImeController::GetInstance()->ApplySettings();
}

void OnLoadingMenuClose()
{
    Skyrim::ShowMenu(ImeMenuName);
}

void OnSteamOverlayMenu(bool open)
{
    static bool prevModEnabled = false;

    auto *manager = ImeController::GetInstance();
    if (open)
    {
        prevModEnabled = manager->IsModEnabled();
        manager->EnableMod(false);
    }
    else if (prevModEnabled)
    {
        manager->EnableMod(true);
    }
}

} // namespace

void InstallEventSinks()
{
    if (auto &menuSink = GetMenuOpenCloseEventSink(); menuSink == nullptr)
    {
        if (auto *ui = RE::UI::GetSingleton(); ui)
        {
            menuSink = std::make_unique<MenuOpenCloseEventSink>();
            ui->AddEventSink(menuSink.get());
        }
    }
}

void UnInstallEventSinks()
{
    if (auto &menuSink = GetMenuOpenCloseEventSink(); menuSink != nullptr)
    {
        if (const auto &ui = RE::UI::GetSingleton(); ui)
        {
            ui->RemoveEventSink(menuSink.get());
        }
        menuSink.reset();
    }
}

//////////////////////////////////////////////////////////////////////////
// MenuOpenCloseEventSink
//////////////////////////////////////////////////////////////////////////

// Called from main thread(UI thread).
auto MenuOpenCloseEventSink::ProcessEvent(const Event *event, RE::BSTEventSource<Event> * /*eventSource*/) -> RE::BSEventNotifyControl
{
    logger::debug("Menu {} open {}", event->menuName.c_str(), event->opening);
    InputFocusAnchor::GetInstance().InvalidateCachedMenuIndex();

    static bool firstOpenMainMenu = true;
    // before game load, all menus will be closed;
    if (event->menuName == RE::LoadingMenu::MENU_NAME)
    {
        if (!event->opening)
        {
            OnLoadingMenuClose();
        }
    }
    else if (event->menuName == RE::MainMenu::MENU_NAME)
    {
        if (firstOpenMainMenu && event->opening)
        {
            OnFirstOpenMainMenu();
            firstOpenMainMenu = false;
        }
    }
    else if (event->menuName == RE::ConsoleNativeUIMenu::MENU_NAME) // Steam Overlay
    {
        OnSteamOverlayMenu(event->opening);
    }
    else
    {
        // If some 3rd menu call RE::ControlMap::AllowTextInput but `CursorMenu` hided, this fix may cause IME accident disable.
        if (ImeApp::GetInstance().GetSettings().fixInconsistentTextEntryCount)
        {
            FixInconsistentTextEntryCount(event);
        }
    }
    return RE::BSEventNotifyControl::kContinue;
}

void MenuOpenCloseEventSink::FixInconsistentTextEntryCount(const Event *event)
{
    // fix: if a menu hides but the text-entry counter is still > 0 while no
    // other real (non always-open) menu remains open, some 3rd-party menu
    // called ControlMap::AllowTextInput(true) without the matching false.
    // Treat that leaked count as stale and disable the IME, otherwise the game
    // keeps swallowing keys ("keyboard still in typing mode").
    // The counter field itself is left untouched (avoid modifying game state
    // directly); the IME state is what we control.
    if (event->opening || !ControlMap::GetSingleton()->HasTextEntry())
    {
        return;
    }
    if (auto *ui = RE::UI::GetSingleton(); ui != nullptr)
    {
        // The menu that just closed may still be on the stack when the close
        // event is dispatched (the engine pops it right after), so skip it:
        // otherwise this check bails on the very menu we are reacting to and
        // the leaked text-entry count is never fixed.
        const auto closingMenu = ui->GetMenu(event->menuName).get();
        for (auto &menu : ui->menuStack)
        {
            // ImeMenu / CursorMenu etc. are always-open; a real menu (other
            // than the one that is closing) still on the stack means the
            // counter may be legitimately owned by it.
            if (menu && menu.get() != closingMenu && !menu->AlwaysOpen())
            {
                return;
            }
        }
    }
    logger::info("No real menu left on the stack but text-entry count is still > 0, forcing IME off");
    const ImeController *manager = ImeController::GetInstance();
    manager->EnableIme(false);
}
} // namespace Ime::Events
