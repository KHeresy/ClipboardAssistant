# Clipboard Assistant

[English](README.md) | [繁體中文](README_TW.md)

Clipboard Assistant is a Windows desktop tool built with Qt 6 for managing clipboard content and extending custom functionality through a plugin mechanism.

## Project Goals

- Stay in the system tray for quick access to the main window and settings window
- Let users view the current clipboard content
- Run custom actions through hotkeys
- Support outputting results by pasting directly, updating the clipboard, or showing them in a window
- Load additional feature modules through the Qt plugin mechanism

## Included Modules

- `OpenAIAssistant`: a plugin module integrated with the OpenAI API, supporting text, images, and streaming responses
- `GemmiAssistant`: an additional plugin module
- `ScriptAssistant`: a script-based extension module that uses JavaScript via `QJSEngine`

## Main Application Features

- Separate main window and settings window
- Settings window options include:
  - Global hotkey for opening the main window
  - Launch at startup
  - List of loaded plugins
- Main window can:
  - Display the current clipboard content
  - Show the user-defined action list
  - Assign hotkeys to individual actions
- When a plugin runs, it receives the current clipboard content and returns the processed result; the main application decides how to output it

## Plugin Design

Each plugin can provide one or more actions and define:

- Action name
- Action description
- Hotkey
- Whether it supports text, images, and RTF
- Whether it provides a settings UI and required parameters

## ScriptAssistant Script Format

- Uses JavaScript syntax
- Scripts are executed by `QJSEngine`
- Each script must define a `process(text)` function
- Example functions can use standard JavaScript APIs such as `text.toUpperCase()` and `JSON.parse()`

## Build Environment

- Windows
- Visual Studio
- Qt 6 (QtCore, QtGui, QtWidgets, QtNetwork, with some modules also using QML)

## Repository Structure

- `ClipboardAssistant/`: main application
- `OpenAIAssistant/`: OpenAI plugin
- `GemmiAssistant/`: plugin module
- `ScriptAssistant/`: plugin module
- `Common/`: shared headers
- `licenses/`: third-party license files

## Licensing and Third-Party Components

- Refer to the root `LICENSE.txt` for the project code license.
- When distributing binaries, include:
  - `THIRD_PARTY_NOTICES.md`
  - license files under the `licenses/` directory
- Qt licensing and source information:
  - https://www.qt.io/licensing/
  - https://code.qt.io/
- It is recommended to deploy Qt via dynamic linking to avoid restricting users' rights to replace LGPL components.
