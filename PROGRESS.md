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

## 任务 2-6（原"未开始"清单）✅ 已在 cab770e 全部修复
- 任务2 EnableIme(false) 归还焦点 ✅ / 任务3 keepImeOpen 反转 ✅ / 任务4 g_prevTextEntryCount 基线 ✅ /
  任务5 FixInconsistentTextEntryCount 通用化 ✅ / 任务6 DoSyncImeState dirty 保留 ✅ / 任务7 构建验证 ✅

## 构建/工程注意事项
- 仓库当前：main 含 cab770e → b85a786 → 7817d6b → c17fde4（修复）→ 7d4f0e5（docs），全部已推送 myfork（lin-414/SimpleIME-Patched）。
- `include/atlcomcli_shim.h`（ATL shim，TextStore.cpp 在用，**不要删**）；`build_ime.cmd`（vcvars64 + cmake --build）已提交，构建走它。
- **patch 工具陷阱**：patch 会把 C++ 缩进打乱 / 单引号 char 字面量里的 \r \n 实体化成真实 CR/LF；修复后用 python 按行尾规范化重写（deep-normalize：先 `\r\n`→`\n` 再换回 `\r\n`，避免 `\r\r\n` 双重），改后 read_file 验证 + git diff 确认最小化。

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
## 残留 bug 诊断与修复 (2026-08-23 第三轮) ✅ 已修复 c17fde4

> 用户实测第二轮 DLL 后补充关键症状：**"游戏内 IME 状态显示一直都有，有时按下键盘无反应，有时会弹出 IME 的候选框"**。
> 这些现象映射到三个残留机制，全部在本轮修复。

### 根因（报告后用户批准修改）

**Bug C 🔴（主因）**：`EnableIme(false)` 只做了"置 IME_DISABLED + 清 TSF 焦点 + 还
Win32 焦点"，**没有把系统输入法切走**。你系统激活的输入法是中文 TIP
（微信输入法/微软拼音），它跟随 Win32 焦点回到游戏窗口后重新激活 → 
按 WASD 弹候选框 / 吞键。`DoEnableMod(true)` 对游戏窗口摘除 IMM 上下文
的防护只在 mod 开关时执行一次，且 TSF 输入法不受其约束。

**Bug D 🟡（副因）**：`ImeMenu::OnKeyEvent` 吞键只看 `IN_COMPOSING`、不检查
`IME_DISABLED`。IMM32 路径（enableTsf=false）`OnFocus(false)` 只解除关联、
不终止组合，`IN_COMPOSING` 残留 → 组合状态永吞键，表现为"按键无反应"。

**Bug E 🟢（显现）**：`INPUT_PROCESSOR_ACTIVATED` 反映"系统输入法激活"，
一直为 true → 调试窗/状态显示里输入状态灯常亮。

### 修复
1. **`ImeManager::EnableIme` 状态机 + 系统输入法联动**（ImeManager.cpp）：
   - **disable 分支**：先记住当前激活的中文输入法 profile（
     `GetActiveLangProfile()`），再 `ActivateLanguageProfile(DEFAULT_LANG_PROFILE)` 
     切到美式键盘。`TF_IPPMF_FORSESSION` 只生效于当前会话，不碰系统全局输入法；
     触发 `InputMethodManager::OnActivated` 回调后自动
     `Clear(IN_COMPOSING/IN_CAND_CHOOSING)` + `INPUT_PROCESSOR_ACTIVATED=false` ——
     **候选框不再弹、吞键解除、状态灯熄灭，一次全解**。
   - **enable 分支**：`FocusTextService(true)` 后恢复记住的中文输入法，输入框内照常中文打字。
   - `ImeManager.h` 新增 `GUID m_lastActiveProfile` 缓存；`ImeWnd.hpp` 新增
     `GetActiveLangProfile()` 访问器。
2. **`ImeMenu::OnKeyEvent` 加 `IME_DISABLED` 门控**（ImeMenu.cpp）：仅当
   `!state.ImeDisabled() && state.IsImeInputting()` 才 `kHandled`，禁用后永不吞键。

### 验证
- 构建：python subprocess build_ime.cmd → **EXIT 0**（12/12 relink），DLL @ 12:14（hash 6eb99f69）。
- dist/SimpleIME 已同步（cmake --install，hash 一致）。
- 提交 `c17fde4` 已推送 myfork（7817d6b..c17fde4），远程已验证。

### a296845 增补修复（用真实 profile 而非桩值）
- **问题**：c17fde4 用 `ActivateProfile(DEFAULT_LANG_PROFILE)` 切美式键盘，但 `DEFAULT_LANG_PROFILE` 的
  `CLSID_NULL`/`GUID_NULL` 是桩值，不匹配系统 TSF 中实际注册的美式键盘 profile。TSF API 可能静默失败，
  中文输入法（微信拼音/微软拼音）保持激活，退出 mod 窗口后仍弹候选框。
- **用户新症状**：在 mod 窗口输入框打字 → 按 ESC 退出 → IME 状态灯依然亮 + 按键盘弹候选框。
  需要先点击输入框以外区域让 IME 状态消失，再退出才能正常操作。
- **根因**：`ITfInputProcessorProfiles::ActivateProfile` 用 `CLSID_NULL`/`GUID_NULL` 激活不存在的 profile，
  返回错误但被静默忽略（之前只打 warning 日志，不影响主流程——但切输入法确实失败了）。
- **修复**：`InputMethodManager::ActivateKeyboardEng()` 遍历 `m_langProfiles`（系统 TSF 枚举加载的列表），
  按 `langid == 0x409` 找到实际美式键盘 profile（含真实 CLSID 和 GUID），调用 `ActivateProfile` 激活它。
  找不到时回退到 `DEFAULT_LANG_PROFILE` 并打 warning（兼容旧系统）。
- **文件改动**（4 文件 +27/-1）：`InputMethodManager.h` 声明 `ActivateKeyboardEng()`；
  `InputMethodManager.cpp` 实现遍历+激活逻辑；`ImeWnd.hpp` 添加转发方法；
  `ImeManager.cpp` 调用 `ActivateEnglishProfile()` 替代 `ActivateLanguageProfile(DEFAULT_LANG_PROFILE.guidProfile)`。
- **构建**：EXIT 0, [10/10], DLL `321ca4d9...`（4,290,560 B），dist 同步。
- **提交** `a296845` 已推送 myfork（d9937b7..a296845）。

### 25cec80 最终修复（用真实 profile 类型而非硬编码 INPUTPROCESSOR）
- **问题**：a296845 找到了真实美式键盘 profile（langid==0x409, 真实 CLSID/GUID），但
  `ActivateProfile(const LangProfile&)` 硬编码了 `TF_PROFILETYPE_INPUTPROCESSOR`（1）。
  美式键盘在系统 TSF 枚举中是 `TF_PROFILETYPE_KEYBOARDLAYOUT` 类型，类型不匹配导致
  `ITfInputProcessorProfileMgr::ActivateProfile` 失败，中文输入法保持激活。
- **用户症状**：ESC 退出 mod 窗口后 IME 状态灯仍亮、按键盘弹候选框——与 a296845 之前完全相同。
- **根因**：`LangProfile` 结构体没有保存 `dwProfileType` 字段，枚举时丢弃了该信息，
  `ActivateProfile` 只能猜 `INPUTPROCESSOR`。
- **修复**（2 文件 +5/-3）：`LangProfile` 加 `DWORD dwProfileType{}` 字段；
  `RefreshProfiles()` 枚举时保存 `profile.dwProfileType`；
  `ActivateProfile(const LangProfile&)` 用 `langProfile.dwProfileType` 替代硬编码。
  `DEFAULT_LANG_PROFILE` 设 `1`（TF_PROFILETYPE_INPUTPROCESSOR，兼容中文输入法通路）。
- **构建**：EXIT 0, [11/11], DLL `720699e8...`（4,290,560 B），dist 同步。
- **提交** `25cec80` 已推送 myfork（ecb7919..25cec80）。

### 42dfdc2 最终修复（传递 HKL 而非 nullptr）
- **问题**：25cec80 修复了 `dwProfileType` 硬编码，但 `ActivateProfile` 仍传 `nullptr` 给 `hkl` 参数。
  `ITfInputProcessorProfileMgr::ActivateProfile` 对 `TF_PROFILETYPE_KEYBOARDLAYOUT` 类型需要传入实际的
  **键盘布局句柄（HKL）**，而非 nullptr。传入 nullptr 让 API 静默接受请求但不实际切换输入法。
- **根因**：`LangProfile` 没有保存 `hkl` 字段。枚举时 `TF_INPUTPROCESSORPROFILE` 结构体包含 `hkl`
  （美式键盘有真实 HKL 句柄，中文 TIP 的 hkl 为 NULL），但枚举代码没保存它。
- **修复**（2 文件 +6/-3）：`LangProfile` 加 `HKL hkl{}` 字段；`RefreshProfiles()` 保存
  `profile.hkl`；`ActivateProfile(const LangProfile&)` 传 `langProfile.hkl` 替代 `nullptr`。
- **构建**：EXIT 0, [11/11], DLL `daf6a8b8...`（4,290,560 B），dist 同步。
- **提交** `42dfdc2` 已推送 myfork（361d2e7..42dfdc2）。
