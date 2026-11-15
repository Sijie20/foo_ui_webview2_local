# foo_ui_webview2 的工作原理

> **维护纪律（法律件）**：
> 1. 本件为组件架构法律件；事实变更须直接替换旧表述并重排章节序号，不得追加平行段落。
> 2. 累计改动超过全件 50% 时须整件重写；不得保留已被推翻的旧说法。
> 3. **组件基线**：**2.2.2**（2026-11-16，已核对 `VERSION`、`src/version.h` 与测试台 DLL；源码目录历史名称 `local-2.2.1` 不是版本声明；生产部署版本沿用运维确认，本次文档更新不重装组件）· **foobar2000 最低版本**：**2.0+**。

foo_ui_webview2 是一个 foobar2000 UI 组件：它通过 Microsoft WebView2 承载前端页面，并使网页能够控制播放器。主题本质是标准 Web 工程，支持 HTML、CSS、JavaScript 及各类现代前端框架；组件负责将其渲染为主窗口或嵌入其他界面的面板。

<a id="主要特性"></a>
<a id="key-features"></a>

## 1. 页面与宿主

组件向所有加载的页面注入全局对象 `window.fb2k`。调用 `fb2k.invoke('playback.play')` 时，消息经由 WebView2 发送到 C++ 原生后端，由宿主执行对应操作并返回结构化结果信封。反方向上，曲目切换等播放器事件以具名通知的形式到达前端，通过 `fb2k.on('playback:trackChanged', handler)` 接收。方法调用采用点号分隔（`playback.play`），事件通知采用冒号分隔（`playback:trackChanged`）。协议细节见 [Bridge 协议](/zh/reference/bridge)。

页面与播放器的所有交互均经过此 Bridge 通道。在 foobar2000 之外（例如普通浏览器标签页），页面不存在 `window.fb2k`，SDK 调用将以 `NOT_SUPPORTED` 错误信封解决（resolve）。目标页面选择与目录解析优先级规则见 §2.2。

## 2. 原生 UI 选择、主题与持久化配置

### 2.1 首次启动与原生切换主题菜单

全新 profile 首次启动时，foobar2000 可能先显示 **UI 模块选择对话框**；选择 **WebView2 UI** 才将本组件作为主界面。这是宿主选择，独立于 HTML 主题选择（§2.2）与默认主题 onboarding（§2.3）。组件通过 `user_interface_factory` 注册 `WebViewUI`；尚未完成这一步时没有网页，不能据此认定 Bridge 故障。

原生菜单 **View → WebView2 UI → 切换主题**（英文 **Switch Theme**）动态枚举 `profile/webview-ui/` 中已安装模板目录，跳过点开头目录；当前主题显示单选标记，没有模板时显示禁用占位项。点击子项后重新检查目录、持久化主题名，并重载主窗口/后台窗口与跟随全局模板的面板；自带模板或 URL override 的面板不随之改变。

动态子项的执行地址是 **父命令 GUID + 子项 `subGuid`**，不是翻译后的标题，也不是父 GUID 单独一个值。切换主题父命令为 `{C8D9E0F3-2A3B-4C5D-6E7F-8091A2B3C4D7}`；子项 GUID 由模板名经 SDK MD5 hasher 生成。应从菜单发现接口取得当前地址，向 `discovery.executeMainMenuCommand`（或对应主菜单 API）同时传两字段，容器本身不是可执行叶子。源码锚点：`src/ui/WebViewMainMenu.cpp`、`src/api/DiscoveryApi.cpp`。

### 2.2 主题选择真源

运行时唯一真源是 **`profile/config.sqlite` 的 `configStrings` 中 `cfg_var.B7E8F3A2-4C5D-2E9F-8A1B-6C7D8E9F0A2C`**。`GetActiveTemplateName()` 读取 `cfg_active_template`，其默认值为 `default`，空值也回退 `default`。**`webview-ui/active-template.txt` 只是安装/打包遗留标记，不参与当前运行时回退**（基线见文首）。

2026-11-16 运维实测：保持 config 为 `foobox-style`，把标记改为 `default` 后重启，页面仍是 foobox-style；在隔离测试台删除 `profile/config.sqlite` 后，选择回退到 **default**，不是标记中原有主题。不要为清主题历史而删整个数据库，它还承载其他宿主/组件配置；运行时切主题应走原生菜单（§2.1）或 Preferences，而不是直接修改 SQLite。

模板名选择与页面目录兜底是两件事。本地页面由 `FrontendDirectoryResolver/Policy` 依次尝试面板指定模板（若有）、全局活动模板、组件 bundled resources、`profile/webview-ui/default`；候选必须有 `index.html`。开发 URL 覆盖另见[页面从哪里加载](./page-loading.md)。源码锚点：`src/prefs/PreferencesPage.cpp`、`src/core/FrontendDirectoryResolver.cpp`。

### 2.3 portable_config 与默认主题 onboarding

`config.*` 将整个 JSON 对象存于 configStore 字符串键 **`foo_ui_webview2.portable_config`**，不是写入一个同名文件，且与原生活动主题变量相互独立（见 §2.2）。例如 **`defaultTheme.onboarding.record`** 是该对象中的字面点号 JSON 键，不是单独 SQLite 列，也不是嵌套目录。`ConfigApi.cpp` 优先读 SQLite-backed store，新值缺失时才迁移旧 cfg_string；不可读或非对象的数据保留原样，不用空对象覆盖。

安装的默认前端在连接/启动检查后（晚于 §2.1 的原生 UI 模块选择）读取 onboarding 记录与当前窗口身份：成功读取但记录不存在、窗口为 **main** 且没有阻塞启动流程时才显示引导。步骤为 **媒体库 → 外观 → 托盘 → 更新与在线内容**，支持上一步/下一步、跳过和完成；writer 在上述键保存 `{version: 1, outcome: ...}`。这是**默认主题策略**，不是全组件通用原生向导；切到 foobox-style 不要求执行它。本行为已查安装态默认前端 0.2.0 的 onboarding 模块，不由组件版本号推断。

### 2.4 CDP 与继承的浏览器参数

组件 Developer 设置启用远程调试后，在创建 WebView2 environment 时加入 `--remote-debugging-port=<port>`；端口范围 **1024–65535**，默认 **9222**，不是始终开放的固定服务。修改开关/端口需重启进程。源码锚点：`src/webview/WebViewEnvironment.cpp`、`src/prefs/PreferencesCdpPort.h`。

启动器也能在启动 foobar2000 前设置 **`WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS`**；测试台启动器明确传入 `--remote-debugging-port=9222` 及防遮挡/后台降级参数。这是 WebView2 继承的启动参数，不是 Bridge 配置键；浏览器进程已创建后，再改启动 shell 的环境变量不会重配该进程。应核对实际 `http://127.0.0.1:9222/json/version` 和 target/profile 身份，而不是仅凭 Developer 勾选状态判断端口：环境覆盖或另一运行实例也可能影响观察。CDP 只用于本机隔离测试台。

## 3. 证据与审查材料纪律

源码库内禁止保存 **`_evidence-*` 目录、审查报告和截图证据**，也不能变相放入库内 `.dev/evidence` 或 `docs/review/`。本主题/组件协作的上述材料统一去源码树外同级 **`fang-foobox-theme.dev`**（迁移清单与 `FBX_DEV_ROOT` 约束见 `fang-foobox-theme.dev/architecture.md` §15.2），记录构建/HEAD、时间与目标 profile 身份。发布契约和本架构页可留在文档目录，但不能把原始审查/证据附件混放其中；主题侧迁移及引用更新对应 `9db73f3c` / `3435b8e4`。纯文档更新不代表已重跑运行时测试。

<a id="bridge-与-sdk"></a>

## 4. Bridge 基础与 SDK 分层

开发者可直接基于底层 Bridge 进行全功能主题开发；但原生 `fb2k.invoke` 与事件监听依赖字符串标识符，缺乏静态类型约束。官方 npm 包 `foo-webview-sdk` 在 Bridge 基础协议之上构建了三层递进的抽象能力：

- **类型化客户端 API (`fb.*`)**：如 `fb.player.play()`、`fb.playlist.getAll()`，提供 TypeScript 类型定义与自动补全，底层调用相同的宿主方法。
- **功能性 Web Components (`fb-*`)**：如播放控制按钮、播放列表视图等开箱即用的自定义元素，内置与宿主的实时状态同步与交互逻辑，遵循无视觉偏见的零样式设计（仅提供 Part 与 Slot 供主题样式化）。
- **SMP 兼容层**：面向 Spider Monkey Panel (SMP) 现有脚本的移植适配层，模拟经典对象的属性与事件体系。

组件提供 400 多个方法和 100 多个事件，按命名空间分组，例如 `playback`、`playlist`、`library`、`window`。[API 概述](/zh/api/overview)列出每个命名空间的方法数和所在页面，[SDK 命名空间](/zh/sdk/namespaces)列出对应的 `fb.*` 封装。

::: tip 从 Spider Monkey Panel (SMP) 迁移
SDK 自带 SMP 兼容层，包含：
- **35 个 SMP 事件映射**：`on_playback_new_track` 等核心事件原生对齐
- **8 个包装器类**：`FbMetadbHandle`、`FbTitleFormat`、`ContextMenuManager` 等
- **完整的 `fb` / `plman` 对象**：支持同步属性访问、播放控制与播放列表管理

兼容层采用「本地缓存 + 事件驱动」机制将异步宿主调用适配为 SMP 同步属性模型。该缓存具备最终一致性，适合 UI 渲染；若需进行精确的事务性控制，可直接调用原生 `fb2k.invoke()`。注意：原 SMP 中基于 Windows GDI/GDI+ 的即时绘制逻辑（如 `on_paint(gr)` 回调及绘图上下文 API）无法直接映射，此类视觉渲染应采用现代化标准 Web 技术栈（HTML、CSS、SVG 或 Canvas 2D/WebGL）进行重构。详见 [SMP 兼容层](/zh/reference/smp-compat)。
:::

## 5. 设计原则

> foo_ui_webview2 遵循「组件做能力，应用做策略」的设计原则。

### 5.1 组件提供什么

| 类别 | 描述 | 示例 |
| --- | --- | --- |
| foobar2000 核心能力 | 播放控制、播放列表、媒体库、封面 | `playback.*`、`playlist.*` |
| 窗口管理 | DWM 效果、标题栏、缩放 | `window.*` |
| 系统集成 | 文件操作、对话框、Shell | `file.*`、`dialog.*` |
| 音频分析 | 频谱、波形 | `audio.*` |
| 配置存储 | 持久化 key-value 存储 | `config.*` |
| 流媒体能力 | 通用 URL 播放、即时队列 | `jitQueue.*` |

### 5.2 主题实现什么

| 类别 | 描述 | 为什么不在组件里 |
| --- | --- | --- |
| 在线服务集成 | Spotify、网易云、QQ 音乐等 | 业务逻辑、授权流程各异 |
| 歌词多源聚合 | 在线搜索、翻译合并 | 业务逻辑 |
| 封面缓存策略 | 预加载、LRU 缓存 | 策略选择 |
| UI 主题与布局 | 颜色、字体、动画 | 应用特性 |

### 5.3 示例：能力与策略

```javascript
// 组件提供能力：一个可直接用作图片地址的 URL
const current = await fb2k.invoke('playback.getCurrentTrack');
if (current.success && current.track) {
    const res = await fb2k.invoke('artwork.getFb2kUrlByPath', { path: current.track.path, maxSize: 200 });
    const img = document.querySelector('img.cover');
    if (res.success && img instanceof HTMLImageElement) img.src = res.dataUrl;
}

// 主题决定策略：提前取哪些封面、保留多久
function prefetchCovers(urls) {
    urls.forEach((url) => {
        new Image().src = url;
    });
}
```
