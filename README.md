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