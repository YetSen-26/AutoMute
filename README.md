# AutoMute（后台自动静音非焦点进程）

这是一个 Windows 小工具：启动后常驻托盘，自动将**非焦点（后台）进程**的音频会话静音；你可以在 GUI 中配置白名单，让某些进程即使在后台也保持有声。

适用于边打游戏边刷视频边做工作又想要音频不会互相打扰的人群）））

## 功能
- 自动静音后台进程（按“音频会话/进程”维度，不是系统全局静音）
- 前台应用始终保持不被静音（切换窗口会及时更新）
- 白名单：白名单内进程在后台也不静音
- 托盘运行：关闭窗口会隐藏到托盘；托盘菜单可退出
- 可选开机自启（调用 `add_to_startup.bat`）

## 实现方式（简述）
- 使用 Windows Core Audio：枚举所有输出设备的 `IAudioSessionManager2` → `IAudioSessionEnumerator` → `IAudioSessionControl2` 获取会话 `ProcessId`，再通过 `ISimpleAudioVolume::SetMute(TRUE/FALSE)` 对每个会话静音/解除静音。
- 通过 `GetForegroundWindow()` 获取当前前台窗口，再解析其进程 exe 名（例如 `chrome.exe`）。**同名 exe 的会话视为同一“前台应用”**，统一解除静音（用于解决浏览器多进程导致音频 PID != 前台窗口 PID 的问题）。
- 白名单保存在 `whitelist.txt`（每行一个 exe 名，统一小写，例如 `chrome.exe`）。

## 使用指南
1. 运行 `AutoMute.exe`。
2. 在下拉框中选择一个可见窗口（格式：`exe - title`），点击 **Add to whitelist** 将该窗口所属进程加入白名单。
3. 白名单列表显示当前已加入的 exe；选中后点击 **Remove from whitelist** 可移除。
4. 关闭窗口会隐藏到托盘；在托盘右键菜单 **Exit** 退出程序（程序会尝试恢复它静音过的会话）。

## 注意事项
- `whitelist.txt` 会生成在程序当前工作目录（与 exe 所在目录通常一致）。该文件已在 `.gitignore` 中忽略。
- 若进程被“强制结束/崩溃”，可能来不及执行“解除静音”的清理逻辑，导致部分会话保持静音；若要做到异常退出也能自动恢复，需要额外 watchdog/守护进程（可后续加入）。

## 构建（CMake + wxWidgets）
- 依赖：wxWidgets 3.3.x（MSVC x64），Windows + Visual Studio
- 示例：
  ```bat
  rmdir /s /q build
  cmake -B build -S . -A x64 -T v143
  cmake --build build --config Release
  ```



*别忘记自己在打游戏*
