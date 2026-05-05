# Clipboard Assistant

[English](README.md) | [繁體中文](README_TW.md)

Clipboard Assistant 是一個以 Qt 6 製作的 Windows 桌面工具，用來協助管理剪貼簿內容，並透過 plugin 機制擴充自訂功能。

## 警告

- 本程式基本上是透過 Gemini 與 GPT 協助進行開發。
- 目前 OpenAI / Gemini 的 API Key 會以明文方式，透過 `QSettings` 儲存於系統內；在 Windows 上通常會儲存在 Registry 中。

## 專案目標

- 常駐於系統匣，方便快速叫出主畫面與設定畫面
- 透過快速鍵執行一連串自訂的動作，例如：
  - 自動複製選取的內容、使用設定好的提示詞讓 AI 進行處理（例如翻譯）
  - 擷取螢幕上的內容、使用設定好的提示詞透過 AI 進行處理（例如翻譯）
  - 處理剪貼簿內的內容，完成後可直接貼上，或僅更新剪貼簿內容供後續使用
- 透過 Qt plugin 機制載入額外功能模組

## 使用範例

### 設定 plugin

如果要使用 OpenAI 功能，需要先建立對應的帳戶：

1. 先點選主視窗左下角的「設定」開啟設定畫面
2. 於下方的「模組設定」，在左邊選取「OpenAI 助手」、然後點選右方的「進階設定」
3. 在開啟的「管理 OpenAI 帳戶」中，點選左下方的「+」新增一組帳戶，輸入名稱與 API Key、模型、基礎 URL 後按下「確定」
   - 建議可以先點選「測試連線」確認參數正確

### 建立動作集

1. 點選左下角的「+」以建立新的動作集
2. 輸入動作集名稱、並視需要設定快速鍵
   - 可勾選「全域」即可在任何場合使用
3. 根據需要調整各項啟動、以及完成後的動作
4. 於左下角的「流程功能」加入需要的功能
   - 可加入多個功能進行串接
   - 部分功能在「步驟配置」會有對應的設定可以使用

### 設定範例

![動作集編輯器](doc/actionset-tw.png)

這樣的設定，之後只要按下 Ctrl + Shift + S，就會讓使用者進行螢幕的擷取，然後將擷取的畫面透過設定的 OpenAI 伺服器進行推論、並顯示結果。

其結果如下：

![主畫面](doc/main-window-tw.png)


有必要亦可在動作集內串接多個功能，進行較為複雜的處理：

![動作集編輯器 - 進階畫面](doc/actionset2-tw.png)

## 目前包含的功能

### 內建助手模組

主程式內建數個可直接加入動作集的助手模組，例如：

- `RegEx Assistant`：使用正規表示式比對、擷取或取代文字內容
- `External App Assistant`：呼叫外部程式
  - 可將剪貼簿文字帶入命令列參數
  - 或擷取程式輸出作為結果
  - 可串接既有的 CLI 工具，或使用指定參數啟動應用程式
- `Text Input Assistant`：插入固定文字，或在執行流程時手動輸入內容

這些內建助手可與 plugin 混合串接，形成多步驟的動作集流程。

### plugin 型助手模組

部分較為複雜的功能目前以 plugin 的形式來實作：

- `OpenAIAssistant`：與 OpenAI API 整合的 plugin
  - 支援文字、圖片與串流回覆
  - 理論上支援所有 OpenAI API 相容之服務
- `GemmiAssistant`：支援 Google AI 的服務
  - 未經嚴謹測試
- `ScriptAssistant`：以腳本方式擴充的 plugin，使用 `QJSEngine` 執行 JavaScript
  - 每個腳本都必須定義 `process(text)` 函式
  - 可使用一般 JavaScript API，例如 `text.toUpperCase()`、`JSON.parse()`

## 主程式功能

- 主介面與設定介面分離
- 設定介面可調整：
  - 叫出主介面的全域快速鍵
  - 開機自動執行
  - 已載入的 plugin 清單
- 主介面可：
  - 顯示目前剪貼簿內容
  - 顯示使用者自訂動作集清單
  - 新增、編輯、刪除、排序動作集
  - 匯入／匯出單一或多個動作集 JSON
  - 為各項功能設定快速鍵
- 每個動作集可串接多個步驟，形成處理流程
- 動作集可設定進階選項，例如自動複製、擷取畫面、完成後動作、自動關閉與隱藏啟動
- plugin 執行時會取得目前剪貼簿內容或擷取內容並回傳處理結果，由主程式決定後續輸出方式，例如更新剪貼簿、顯示輸出內容或直接貼上

## 建置環境

- Windows 11
- Visual Studio 2026
- Qt 6.11（QtCore、QtGui、QtWidgets、QtNetwork，部分模組另含 QML）

開啟 `ClipboardAssistant.slnx` 即可建置

## 專案結構

- `ClipboardAssistant/`：主應用程式
- `OpenAIAssistant/`：OpenAI Plugin
- `GemmiAssistant/`：Gemini Plugin
- `ScriptAssistant/`：JavaScript Plugin
- `Common/`：共用標頭
- `doc/`：README 使用的畫面截圖與文件資源
- `licenses/`：第三方授權文件
- `ClipboardAssistant.slnx`：Visual Studio 方案檔
- `THIRD_PARTY_NOTICES.md`：第三方元件聲明

## 授權與第三方元件

- 本專案程式碼授權請參考根目錄 `LICENSE.txt`。
- 發行二進位版本時，請一併附上：
  - `THIRD_PARTY_NOTICES.md`
  - `licenses/` 目錄中的授權文件
- Qt 授權與原始碼資訊：
  - https://www.qt.io/licensing/
  - https://code.qt.io/
- 建議以動態連結方式部署 Qt，避免限制使用者替換 LGPL 元件的權利。
