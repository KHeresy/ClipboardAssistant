# TODO

## 高優先

### 模組架構

- [ ] 在應用程式關閉時，先停止所有執行中的 pipeline 與非同步模組工作，再卸載外部外掛。
- [ ] 在升級流程中，自動把舊的 host 管理模組設定從 `Modules/{module->name()}/Global` 遷移到 `Modules/{module->id()}/Global`。
- [ ] 記錄並顯示更完整的外部外掛載入診斷資訊，包括預期 API 版本、實際回報版本與 loader error 詳細內容。

### 主應用程式架構

- [ ] 強化 `PipelineExecutor` 的關閉與取消流程，避免已取消或正在卸載的模組仍然回呼到已關閉的 host 視窗。
- [ ] 在 `ClipboardAssistant::~ClipboardAssistant()` 卸載 plugin loader 前，先停止目前的 executor，並等待模組 abort / cleanup 完成。
- [ ] 更嚴格驗證匯入的 action set，包括缺少欄位、參數型別不正確、模組不相容與快捷鍵重複等情況。

## 中優先

### 模組架構

- [ ] 為 `ModuleInfo` 新增快取中繼資料，例如 `moduleId`、`displayName`、`version` 與標準化來源資訊，避免每次都直接讀 live module instance。
- [ ] 把 `isInternal` 改成更清楚的擁有權模型，例如 `enum class ModuleOwnership { HostOwned, LoaderOwned }`。
- [ ] 在執行前先用 `supportedInputs()` 與 `supportedOutputs()` 驗證 pipeline 步驟相容性。
- [ ] 統一 plugin 設定流程，避免 host-managed 設定與 plugin 自有的進階對話框拆成兩套。
- [ ] 評估帳號選擇參數是否應改存穩定的 account ID，而不是顯示名稱，避免改名後失效
- [ ] 讓宣告的 `supportedInputs()` 與實際實作對齊，尤其是 Gemmi / OpenAI 的 `File` 處理

### 主應用程式架構

- [ ] 在 `ActionSetSettings` 加上 host 端驗證，讓非法參數值、必填欄位空值與不相容的步驟鏈在儲存前就被阻擋。
- [ ] 改善 `PipelineExecutor` 在混合 mime data、累積串流文字與步驟間非文字輸出時的資料傳遞規則。
- [ ] 把 `Setting` 的設定 UI 建構與持久化邏輯拆開，降低 constructor 複雜度，也讓 plugin 診斷更容易擴充。
- [ ] 加強隱藏執行時的進度、取消與完成 UX，特別是 `startHidden`、`autoClose` 與 paste/copy 完成模式交互時。

## 低優先

### 模組架構

- [ ] 擴充 `IModuleCallback`，支援結構化進度、警告與更豐富的非文字結果。
- [ ] 為 `ParameterDefinition` 加上 helper builder 或 factory function，減少長串 aggregate initializer 並提高可讀性。
- [ ] 在設定詳細頁明確顯示 plugin API version 與 module version。
- [ ] 為動態參數 UI 產生新增測試，特別是 `Decimal`、進階設定區塊與失敗 plugin 顯示。

### 主應用程式架構

- [ ] 為 action-set 匯入 / 匯出、action reorder 持久化、快捷鍵註冊更新與 executor 取消流程新增測試。
- [ ] 考慮把 action-set 持久化與匯入 / 匯出拆成獨立 service，不要都放在主視窗類別中。
- [ ] 考慮把 plugin discovery / loading 拆成獨立 manager，讓 `ClipboardAssistant` 不再直接掌管 plugin 生命週期、診斷與執行協調。
