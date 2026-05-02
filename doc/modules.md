# 模組

## 權責區隔

此專案使用共用的外掛介面：`Common/IClipboardModule.h` 裡的 `IClipboardModule`。

每個模組負責：
- 提供識別與版本資訊
- 宣告支援的輸入 / 輸出資料型別
- 定義 host UI 需要的 action 參數
- 執行 `process(...)` 處理剪貼簿資料
- 視需要支援 `abort()` 取消
- 視需要提供 `showConfiguration(...)` 設定 UI

Host 看起來負責外掛探索、UI 建構、action 選擇與生命週期管理。模組則負責請求組裝、各 API 的特定行為與回應解析。

## 共用模組契約

從 `IClipboardModule` 來看：
- `id()`, `name()`, `version()` 用來識別模組
- `actionParameterDefinitions()` 定義每個 action 的設定
- `globalParameterDefinitions()` 定義 host / 全域設定；此處大多數模組都回傳空值
- `actionTemplates()` 定義「新增步驟」選單的預設 action
- `supportedInputs()` / `supportedOutputs()` 宣告資料相容性
- `supportsStreaming()` 宣告是否會輸出局部結果
- `process(...)` 執行實際工作
- `abort()` 中止進行中的工作
- `hasConfiguration()` / `showConfiguration()` 提供模組設定對話框

## 設定模式

目前應用程式中有兩種設定模式：

### 1. Host 管理的模組設定
Host 可以根據 `globalParameterDefinitions()` 建立並保存模組設定。

儲存位置：
- `Modules/<moduleId>/Global`

特性：
- host 自動建立設定 UI
- host 會把這些值載入到 `globalParams`
- 模組可以在 `process(..., globalParams, ...)` 中讀取

### 2. 模組自主管理的設定
模組可以透過 `hasConfiguration()` 與 `showConfiguration()` 提供自己的設定對話框。

特性：
- 模組自己掌握完整的設定 UI 與保存格式
- host 只負責開啟對話框
- host 不會解讀儲存欄位
- 設定可能儲存在 `Modules/<moduleId>/Global` 之外

Gemmi 與 OpenAI 目前使用的是這種模式。

## GemmiAssistant

### 責任
- 負責 Gemini 專用的設定與請求格式化
- 將帳號儲存在與讀取自 `Gemmi/Accounts`
- 使用 Gemini 原生的請求 / 回應結構
- 使用 `system_instruction` 與 `contents` payload
- 將圖片資料以 Gemini 的 `inline_data` 方式送出
- 解析 `candidates[0].content.parts[].text`

### 設定
- 使用模組自主管理的設定
- 帳號資料儲存在 `Gemmi/Accounts`
- 目前沒有暴露 host 管理的 `globalParameterDefinitions()`
- `process(...)` 直接從 `QSettings` 讀帳號設定，而不是從 `globalParams`

### 運作模式
- `supportsStreaming()` 為 `false`
- `process(...)` 會等完整回應後再輸出最終文字
- 不會進行 `onTextData(..., false)` 的逐步串流輸出
- `abort()` 只會取消目前的 network reply

### 預設值
- 預設 model：`gemini-2.0-flash`
- 預設 URL：`https://generativelanguage.googleapis.com/v1beta`
- 預設 suffix：`/models/{model}:generateContent`
- 預設 auth mode：`api-key`
- 預設 system prompt：`You are a helpful assistant.`

### 行為備註
- 介面層級支援 text、image、file 輸入，但目前實作只把 text 與 image 內容轉成 request body
- 當 `authMode == api-key` 時使用 `x-goog-api-key`，否則使用 `Authorization: Bearer`
- 設定 UI 包含 Gemini 專用的連線測試流程

## OpenAIAssistant

### 責任
- 負責 OpenAI 風格的帳號管理與請求格式化
- 將帳號儲存在與讀取自 `OpenAI/Accounts`
- 同時支援 chat-completions 與 legacy completions 兩種呼叫方式
- 處理可選的 Azure 專用 endpoint 組裝
- 透過 server-sent events 輸出串流結果
- 解析 `choices[0].delta.content` 或 `choices[0].text`

### 設定
- 使用模組自主管理的設定
- 帳號資料儲存在 `OpenAI/Accounts`
- 目前沒有暴露 host 管理的 `globalParameterDefinitions()`
- `process(...)` 直接從 `QSettings` 讀帳號設定，而不是從 `globalParams`

### 運作模式
- `supportsStreaming()` 為 `true`
- `process(...)` 會在 SSE chunk 到來時輸出局部文字
- 最終完成時仍會再輸出一次完整結果
- `abort()` 會取消目前的 network reply

### 預設值
- 預設帳號 prompt 仍是 `You are a helpful assistant.`
- API type 預設為 `Chat`
- auth mode 預設為 `Bearer`
- URL / suffix 由每個帳號自行設定

### 行為備註
- 介面層級支援 text、image、file 輸入
- Chat mode 會送出帶 system message 與 user message 的 `messages`
- Legacy / completions mode 會送出單一 `prompt`
- 可選欄位包含 `ResponseFormat`、`ReasoningEffort` 與 raw JSON 合併
- 會依 URL 偵測 Gemini 相容 endpoint，並套用稍微不同的參數處理

## Action set 的持久化與匯出限制

Action set 會儲存：
- action-set 中繼資料，例如名稱、快捷鍵、完成行為與各種旗標
- step 清單
- 每個 step 的 `moduleId`
- 每個 step 的 action 參數

Action set 目前不會儲存：
- 模組擁有的帳號清單
- API key 或其他模組密鑰
- 來自 `Gemmi/Accounts` 或 `OpenAI/Accounts` 的帳號定義
- 外部模組設定對話框的獨立副本

影響：
- 匯出 action set 時只會帶出 action 參數，不包含模組帳號設定
- 匯入到另一台機器或另一份 profile 時，Gemmi / OpenAI 的 step 可能找不到對應帳號設定
- 如果 action 參考的帳號名稱不存在，模組會改為要求選帳號或回傳錯誤
- 目前帳號選擇是依名稱比對，因此帳號改名可能會破壞既有 action-set 參照

## Gemmi 與 OpenAI 跟其他模組不一樣嗎？

是的。

和共用模組契約相比，這兩個模組都比一般外掛更專門，而且彼此之間也不同：

- **GemmiAssistant** 是單次回應、非串流的 Gemini 原生介面
- **OpenAIAssistant** 是支援串流的介面，並同時支援 chat-completions 與 legacy completions 兩種模式

它們在設定擁有權上也和 host-managed 模組不同：
- **GemmiAssistant** 與 **OpenAIAssistant** 都擁有自己的帳號儲存與設定對話框
- 使用 `globalParameterDefinitions()` 的模組則依賴 host 管理的 `Modules/<moduleId>/Global` 設定儲存

所以行為並不相同：
- Gemmi = 批次回應、Gemini 專屬 schema、自主管理設定
- OpenAI = 串流回應、OpenAI 相容 schema，含 legacy fallback、自主管理設定

## 總結

若要簡短分類：
- **Host 責任**：模組載入、選擇、UI 組裝、生命週期
- **模組責任**：供應商專屬 API 契約、設定持久化、請求執行、結果解析
- **Gemmi**：非串流的 Gemini 介面，且帳號設定由模組自主管理
- **OpenAI**：支援 chat/completions 分支的串流 OpenAI 介面，且帳號設定由模組自主管理
