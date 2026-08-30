# SimpleIME (Patched)

A fork of [cyfewlp/SimpleIME](https://github.com/cyfewlp/SimpleIME) (native IME support for
Skyrim SE/AE — type Chinese, Japanese, Korean and other multi-byte languages in the game
console and any text field).

**Base:** upstream `v2.2.1` (`a2cd39f`) · **License:** MIT

> This README only describes what this fork changes. For features, build instructions,
> configuration and architecture, see the [upstream README](https://github.com/cyfewlp/SimpleIME#readme).

## What's different from upstream

### Fixes

1. **Keyboard no longer stuck in typing mode after closing a mod menu**
   (`ImeWnd::AbortIme` → `CM_ABORT_IME` → `ITextService::AbortIme`)

   Previously `AbortIme()` only returned keyboard focus and never terminated the active
   TSF/IMM32 composition nor cleared the `IN_COMPOSING` / `IN_CAND_CHOOSING` state flags —
   so `ImeMenu::OnKeyEvent` kept swallowing every keystroke and the game never received
   input. Now the composition is genuinely cancelled (TSF: `TerminateComposition`; IMM32:
   `NI_COMPOSITIONSTR/CPS_CANCEL`) with the editor cleared *before* termination so no
   partial composition text is committed, and both state flags are dropped.

2. **`EnableIme(false)` returns Win32 keyboard focus to the game window**

   The enable path focused the hidden 0×0 `ImeWnd`, but the disable path never focused
   back, leaving `WM_CHAR` eaten by the invisible window. Now symmetric:
   `Focus(m_gameHwnd)` runs after the text service detaches.

3. **`keepImeOpen` no longer inverts disable requests**

   `DoEnableIme` previously computed `keepImeOpen || enable`, turning a `false` request
   into `true`. Disable is now unconditional (Steam overlay / unchecking "Enable Mod" truly
   disables), while text-entry closes may still keep the IME open per the setting.

4. **Text-entry counter baseline synced at plugin load**

   The `textEntryCount` change hook started with a baseline of 0; if a text entry was
   already open when the plugin loaded, the first 1→0 transition was swallowed by the
   `count == old` guard. `Install()` now seeds the baseline from the current counter.

5. **Inconsistent text-entry counter check generalized**

   Previously only a `CursorMenu` close triggered the consistency repair. Now any menu
   close that leaves a nonzero text-entry counter with no non-`AlwaysOpen` menu remaining
   re-syncs the IME state (`FixInconsistentTextEntryCount`, still opt-in).

6. **`DoSyncImeState()` keeps the dirty flag on failure**

   The dirty flag was cleared *before* the delegate call; a failed sync silently forgot the
   pending state. It is now only cleared on success, so the next sync trigger retries.

### v2.2.2 — "still in IME state after leaving a mod window" (fixed & verified)

The root cause was a **leaked text-entry counter**: mod menus call `AllowTextInput(true)`
without a matching `false` — sometimes landing *after* the menu-close event, where no
further event would ever repair it. The leaked counter kept the IME, the language bar and
the pausing tool window on indefinitely (and blinded the repair, since SimpleIME's own
`ToolWindowMenu` sits on the menu stack whenever the bar is visible — a self-sustaining
stuck state). Counter leaks are now healed (event repair + a stability-grace poll), the
hook's cached count re-syncs, and `ToolWindowMenu` no longer counts as a counter owner.
Also in this round:

- Language-bar show/hide is driven from the single `ImeManager::EnableIme` funnel, so
  every disable path hides the bar (some paths bypassed the old request site).
- The English keyboard layout is re-asserted by a `WM_INPUTLANGCHANGE` watchdog whenever
  the game thread drifts back to a non-English layout while the IME is disabled.
- The language-bar pin button toggles (it previously could never unpin), and
  `INPUT_PROCESSOR_ACTIVATED` semantics were restored — it gates `WM_CHAR` forwarding;
  the status light checks the profile type at the display site instead.

### v2.3.0 — focus-steal hardening + robustness audit (crash/UAF/hang class)

- **Focus steal during composition** (the old `WM_KILLFOCUS` FIXME, e.g. Win+Shift+S at
  the wrong moment): `ClearInputKeys` no longer runs on the IME thread against the render
  thread's ImGui frame; an aborted composition no longer injects its partial text mid
  focus transition; and the composition is terminated on the IME thread under our control.
- **Robustness audit fixes**: `ErrorNotifier` (JamieMods submodule, now a fork) is
  mutex-guarded — its deque was mutated from the IME thread while the render thread
  iterated it (use-after-free); TSF composition/UIElement sinks lock the editor and
  candidate state against the render thread's copies (with a document-lock predicate so
  sinks inside `OnLockGranted` do not self-deadlock); queued tasks can no longer run
  against a torn-down `ImeController`; the IME thread shuts down in an orderly way
  (`WM_QUIT` to the thread, teardown on the thread owning the COM apartment, no
  cross-thread COM teardown in static destruction); a malformed `shortcut` config no
  longer hangs the game at boot; one leaked TSF reference per composition session gone;
  the theme hex-RGB input applies all three channels.

### Tuning

- `autoToggleKeyboard` default aligned to `true` (was `false` in the struct but `true` in
  `GetDefaultSettings()`).

## Installing the fixed build

The built mod layout (for MO2 or manual install) is the same as upstream:

```
SimpleIME/
├── SKSE/Plugins/SimpleIME.dll
└── interface/SimpleIME/ (config, translations, icon font)
```

Grab a build from **Releases** of this repo, or build from source following the
[upstream build guide](https://github.com/cyfewlp/SimpleIME#readme).

## Sync with upstream

```
git fetch upstream && git merge upstream/main
```

(the `upstream` remote should point to `https://github.com/cyfewlp/SimpleIME.git`)

## License

MIT — see [LICENSE](LICENSE). Copyright (c) 2026 cyfewlp. Fork maintenance: lin-414.