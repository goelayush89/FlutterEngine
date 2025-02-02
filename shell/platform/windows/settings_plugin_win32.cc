// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/windows/settings_plugin_win32.h"

#include "flutter/shell/platform/windows/system_utils.h"

namespace flutter {

namespace {
constexpr wchar_t kGetPreferredBrightnessRegKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
constexpr wchar_t kGetPreferredBrightnessRegValue[] = L"AppsUseLightTheme";
}  // namespace

// static
std::unique_ptr<SettingsPlugin> SettingsPlugin::Create(
    BinaryMessenger* messenger,
    TaskRunner* task_runner) {
  return std::make_unique<SettingsPluginWin32>(messenger, task_runner);
}

SettingsPluginWin32::SettingsPluginWin32(BinaryMessenger* messenger,
                                         TaskRunner* task_runner)
    : SettingsPlugin(messenger, task_runner) {
  RegOpenKeyEx(HKEY_CURRENT_USER, kGetPreferredBrightnessRegKey,
               RRF_RT_REG_DWORD, KEY_NOTIFY, &preferred_brightness_reg_hkey_);
}

SettingsPluginWin32::~SettingsPluginWin32() {
  StopWatching();
  RegCloseKey(preferred_brightness_reg_hkey_);
}

void SettingsPluginWin32::StartWatching() {
  if (preferred_brightness_reg_hkey_ != NULL) {
    WatchPreferredBrightnessChanged();
  }
}

void SettingsPluginWin32::StopWatching() {
  preferred_brightness_changed_watcher_ = nullptr;
}

bool SettingsPluginWin32::GetAlwaysUse24HourFormat() {
  return Prefer24HourTime(GetUserTimeFormat());
}

float SettingsPluginWin32::GetTextScaleFactor() {
  return 1.0;
}

SettingsPlugin::PlatformBrightness
SettingsPluginWin32::GetPreferredBrightness() {
  DWORD use_light_theme;
  DWORD use_light_theme_size = sizeof(use_light_theme);
  LONG result = RegGetValue(HKEY_CURRENT_USER, kGetPreferredBrightnessRegKey,
                            kGetPreferredBrightnessRegValue, RRF_RT_REG_DWORD,
                            nullptr, &use_light_theme, &use_light_theme_size);

  if (result == 0) {
    return use_light_theme ? SettingsPlugin::PlatformBrightness::kLight
                           : SettingsPlugin::PlatformBrightness::kDark;
  } else {
    // The current OS does not support dark mode. (Older Windows 10 or before
    // Windows 10)
    return SettingsPlugin::PlatformBrightness::kLight;
  }
}

void SettingsPluginWin32::WatchPreferredBrightnessChanged() {
  preferred_brightness_changed_watcher_ =
      std::make_unique<EventWatcherWin32>([this]() {
        task_runner_->PostTask([this]() {
          SendSettings();
          WatchPreferredBrightnessChanged();
        });
      });

  RegNotifyChangeKeyValue(
      preferred_brightness_reg_hkey_, FALSE, REG_NOTIFY_CHANGE_LAST_SET,
      preferred_brightness_changed_watcher_->GetHandle(), TRUE);
}

}  // namespace flutter
