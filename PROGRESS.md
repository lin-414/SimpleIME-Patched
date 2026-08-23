# SimpleIME 修复进度 (2026-08-22)

> 用户报告 bug：**有时在 mod 界面输入完继续游戏时，点击键盘还是打字状态**
> （键盘输入被 IME 吞掉，无法正常操作游戏）
> 用户已批准"全改"，正在修复 6 个已报告的 bug。

## 任务概览

| # | 严重度 | 问题 | 状态 |
|---|--------|------|------|
| 1 | 🔴 | `AbortIme()` 只还焦点、不终止组合，IN_COMPOSING/IN_CAND_CHOOSING 残留 → 键盘被吞 | ✅ 已修复，构建通过 |
| 2 | 🔴 | `EnableIme(false)` 不归还 Win32 焦点（与 EnableIme(true) 不对称） | ✅ 已修复，构建通过 |
| 3 | 🟡 | `EnableMod(false)` 时 `keepImeOpen || enable` 反转禁用请求 | ✅ 已修复，构建通过 |
| 4 | 🟡 | `g_prevTextEntryCount` 初始为 0，若加载时已有输入框会漏掉首帧 | ✅ 已修复，构建通过 |
| 5 | 🟢 | `FixInconsistentTextEntryCount` 只覆盖 CursorMenu | ✅ 已修复，构建通过 |
| 6 | 🟢 | `DoSyncImeState()` 先清 dirty 再调 delegate，失败后状态不再重试 | ✅ 已修复，构建通过 |
| 7 | — | 构建验证（clang-cl + vcvars64） | ✅ 通过（EXIT 0，16/16） |

## 已完成（任务 1：AbortIme 真正终止组合）

### 1a. CM_ABORT_IME 消息定义 ✅
- `include/configs/CustomMessage.h`：enum 末尾加 `CM_ABORT_IME`。

### 1b. ITextService::AbortIme() 接口 + Imm32 实现 ✅
- `include/ime/ITextService.h`：ITextService 加虚方法 `virtual void AbortIme() {}`（默认空实现，保证不破坏其他继承者）；
  Imm32TextService 声明 `void AbortIme() override;`。**注意：此文件曾两次缩进被打乱，最后整文件重写修复，请 diff 确认无其他改动。**
- `src/ime/Imm32TextService.cpp`：实现 AbortIme —— 先锁内清空 TextEditor + 关闭候选窗（**不调用 OnEndCompositionCallback，避免把未完成的组合文本注入游戏**），再 `ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_CANCEL, 0)` 取消系统组合，最后清 IN_COMPOSING / IN_CAND_CHOOSING。

### 1c. TextStore::TerminateComposition() + TSF TextService::AbortIme() ✅
- `include/tsf/TextStore.h`：
  - TextStore 公开方法加 `[[nodiscard]] auto TerminateComposition() const -> HRESULT;`（在 ClearFocus 声明后）；
  - Tsf::TextService 声明 `void AbortIme() override;`（OnFocus 声明后）。
- `src/tsf/TextStore.cpp`：抽出 `TerminateComposition()`（m_context 非空时通过 `ITfContextOwnerCompositionServices::TerminateComposition(nullptr)` 终止组合），`ClearFocus()` 改为复用它。
- `src/tsf/TextService.cpp`：实现 `TextService::AbortIme()` —— 先 GetWriteLock 清空 TextEditor/关闭候选窗（**OnEndComposition 会回调 SendUiString，必须先清空避免注入**），再 `m_textStore->TerminateComposition()`，最后兜底清状态位。

### 1d. ImeWnd::AbortIme() → 发消息到 IME 线程 ✅
- `src/ImeWnd.cpp`：
  - `AbortIme()`（游戏 UI 线程调用）改为：若有 IN_CAND_CHOOSING/IN_COMPOSING，`SendNotifyMessageToIme(CM_ABORT_IME, 0, 0)`（在 IME 线程的 ImeWnd WndProc 中执行真正的组合终止，**不能直接从游戏线程碰 TSF 对象**），然后 `SetFocus(m_hWndParent)` 还焦点。
  - WndProc 加 `case CM_ABORT_IME:` → `pThis->m_textService->AbortIme(); return 0;`。
  - ⚠️ **此文件 CM_EXECUTE_TASK 附近缩进被 patch 打乱过两次，最后用 python 脚本按 LF 规范化替换修复。已重新读取确认缩进正确（361-375 行）。**

## 未开始（任务 2-6）—— 明天继续

### 任务 2 🔴 EnableIme(false) 归还焦点
- 位置：`src/ime/ImeManager.cpp` `ImeManager::EnableIme(bool enable)`（约 37-84 行）。
- 现状：enable=true 时走 `TryFocusIme()`（AttachThreadInput + SetFocus ImeWnd），enable=false 时 `FocusTextService(false)` 后**没有把焦点还给游戏窗口**，WM_CHAR 仍被 0×0 ImeWnd 吃掉。
- 方案：禁用分支成功后调用 `ImeManager::Focus(m_gameHwnd)`（Focus 是 static，已处理跨线程 AttachThreadInput + SetFocus），有成员 `m_gameHwnd` 可用。参考 EnableIme(true) 的 TryFocusIme 对称实现。

### 任务 3 🟡 keepImeOpen 反转
- 位置：`src/ime/ImeController.cpp` `DoEnableIme`（约 189-195 行）：`m_delegate->EnableIme(m_settings->input.keepImeOpen || enable)` —— enable=false 且 keepImeOpen=true 时收到 true，禁用失效。
- 方案：改为 `m_delegate->EnableIme(enable)`，把 keepImeOpen 覆盖逻辑收敛到 `ImeManager::IsShouldEnableIme()`（那里已有 `keepImeOpen || HasTextEntry()`）。并确认 `EnableMod(false)`（Steam overlay 打开，EventHandler.cpp:61-65 调用）总是真禁用。

### 任务 4 🟡 g_prevTextEntryCount 初始偏移
- 位置：`src/hooks/ScaleformHook.cpp`（约 148-168 行）—— file-scope static `g_prevTextEntryCount` 初始为 0；若插件加载时输入框已打开（1→0 首次关闭时 oldValue=1 vs prev=0 不相等），`entryCount == oldValue` 守卫会丢掉第一个 EnableIme(false)。
- 方案：Install 时用当前 `GetTextEntryCount()` 初始化基线；或第一次回调时无条件执行一次 SyncImeState。

### 任务 5 🟢 FixInconsistentTextEntryCount 扩展
- 位置：`src/core/EventHandler.cpp`。
- 方案：除 CursorMenu 关闭外，把一致性检查扩展到"菜单关闭后无暂停菜单/输入上下文仍在"的通用场景（留意不要误伤控制台等合理场景），或在 SyncImeState 路径加 textEntryCount vs 实际 IME 状态的一致性校验。

### 任务 6 🟢 DoSyncImeState dirty 丢了
- 位置：`src/ime/ImeController.cpp` `DoSyncImeState()`（约 243-257 行）—— `m_fDirty.store(false)` 在调 delegate 之前，失败后 dirty 丢失不再重试。
- 方案：先保存 dirty 值，成功后再清，失败时还原。

### 任务 7 构建验证
- SimpleIME 构建必须走 `.cmd` 先 `call vcvars64.bat`（BuildTools 无完整 VS），clang-cl 编译；否则 PCH 版本不匹配全 TU 报错。
- 先验证工具链：LLVM (C:\Program Files\LLVM\bin\clang-cl.exe)、FontForge (C:\Program Files\FontForgeBuilds\bin\fontforge.exe)。
- 构建目录：build/RelWithDebInfo-clangcl-ninja-vcpkg（已有旧产物）。
- 详见 skill: windows-cpp-build。

## 构建/工程注意事项
- 仓库当前未提交，HEAD = a2cd39f "bump version to v2.2.1"。
- **git 状态快照（2026-08-22 结束时）**：15 个已修改文件，3 个未跟踪文件。本次会话新增修改：`include/configs/CustomMessage.h`、`include/ime/ITextService.h`、`include/tsf/TextStore.h`、`src/ImeWnd.cpp`、`src/ime/Imm32TextService.cpp`、`src/tsf/TextService.cpp`、`src/tsf/TextStore.cpp`；其余 8 个修改文件是本会话前的改动（atomic_bool 转换、SetForegroundWindow 增强、FakeDirectInputDevice 容错、BOM/CRLF 噪音）。未跟踪：`include/atlcomcli_shim.h`（ATL shim，TextStore.cpp 在用，不要删）、`PROGRESS.md`（本文件）、`C：tmp_batbuild_out.log`（全角冒号杂散日志，可删）。
- 修改过 BOM/行尾的文件（ITextService.h、ImeWnd.cpp、TextStore.h、TextStore.cpp、TextService.cpp、Imm32TextService.cpp、CustomMessage.h）—— 注意 diff 时若看到 BOM/CRLF 噪音属正常。
- **patch 工具陷阱**：本会话中 patch 多次打乱 C#/C++ switch 块缩进（匹配到错误行导致嵌套错位），修复后用 python 按行尾规范化重写。后续编辑先读文件最新内容再 patch，patch 后立即 read_file 验证。

## 构建验证结果 (2026-08-23) ✅ 全部通过
- 工具链验证：LLVM clang-cl ✅ / FontForge ✅ / vcvars64 ✅
- 增量构建：`cmake --build build\RelWithDebInfo-clangcl-ninja-vcpkg --config RelWithDebInfo` → **EXIT 0，16/16 目标完成**，SimpleIME.dll 链接成功（2026-08-23 08:59）。
- 仅剩警告：`atlcomcli_shim.h` 的 `__uuidof` 语言扩展警告（预期内，shim 设计如此）。
- 新构建脚本：`build_ime.cmd`（vcvars64 + cmake --build，可复用，尚未提交）。
- 待办/待问用户：① `Settings.h` 中 `autoToggleKeyboard` 结构体默认值 `false`（line 51）vs `GetDefaultSettings()` 中 `true`（line 91）不一致，需确认是否对齐；② 是否提交（含既有未提交 baseline diff + atlcomcli_shim.h）；③ `C：tmp_batbuild_out.log` 全角冒号杂散日志是否删除；④ 5 个 python 改过的文件带 BOM（utf-8-sig）是否去 BOM。
## 残留 bug 诊断与修复 (2026-08-23 第二轮) ✅ 已修复 b85a786

> 用户实测修复后的 DLL：**"退出mod窗口后依然被识别为输入状态"**（与最初症状同源）。

### 根因（报告后用户批准修改）
**Bug A 🔴（根因）**：`FixInconsistentTextEntryCount`（EventHandler.cpp:137）存在时序缺陷——
菜单**关闭事件派发时，正在关闭的菜单仍在 `menuStack` 上**（引擎先派发事件、后出栈）。
原逻辑遍历栈时遇到该非 always-open 菜单直接 `return`，导致漏检的
`AllowTextInput(true)` 计数永远不被清理 → `EnableIme(false)` 永不触发 →
IME 保持激活、焦点留在 0×0 ImeWnd → 游戏吞键。这正是"有时"的成因：
取决于第三方 mod 是否调用 `AllowTextInput(false)`。

**修复**：遍历时用 `ui->GetMenu(event->menuName).get()`（按名字拿菜单指针，
BSFixedString 可隐式转 string_view）跳过**正在关闭的那个菜单**，其余非
always-open 菜单仍在栈上才 return；全部通过则 `EnableIme(false)` + info 日志。

**Bug B 🟡（排查后撤销）**：曾怀疑 `EnableIme(false)` 不清
`INPUT_PROCESSOR_ACTIVATED` 导致 OnCharEvent 持续吞字符。复查确认**误诊**：
吞键闸门是 `!ImeDisabled()`，`EnableIme(false)` 本来就会置 `IME_DISABLED`；
而 `INPUT_PROCESSOR_ACTIVATED` 的恢复只依赖系统 profile 激活事件
（InputMethodManager.cpp:207/310），`EnableIme(true)` 不经过它——若清除，
下次启用输入法将直接失效。**已撤销**，不做此改动。

### 日志增强
- `ImeManager::EnableIme` 两条 debug → **info**（真实状态切换才打，短路不打）。
- `ImeWnd::AbortIme` 触发时加 **info**。
- `FixInconsistentTextEntryCount` 兜底触发时加 **info**。
→ 下次再出问题，游戏 log 直接可见 EnableIme/Abort 切换轨迹。

### 验证
- 构建：python subprocess build_ime.cmd → **EXIT 0**，DLL @ 11:16（hash 3e5f6da1）。
- dist/SimpleIME 已同步（cmake --install，hash 一致）。
- 提交 `b85a786` 已推送 myfork（8b6d981..b85a786）。
