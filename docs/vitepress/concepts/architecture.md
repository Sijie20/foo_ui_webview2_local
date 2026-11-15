# How foo_ui_webview2 works

> **Component baseline**: 2.2.2 (2026-11-16) · **Minimum foobar2000 version**: 2.0+

foo_ui_webview2 is a foobar2000 component that hosts web pages via Microsoft WebView2 and exposes playback control and status interfaces. A theme is a standard web application built with HTML, CSS, JavaScript, and any modern web framework; the component renders it as the main foobar2000 window or as an embedded panel within another user interface.

<a id="key-features"></a>
<a id="主要特性"></a>

## A page and the host

The component injects a global `window.fb2k` object into every loaded page. Calling `fb2k.invoke('playback.play')` sends a message through WebView2 to the native C++ backend, which executes the action against foobar2000 and returns a structured result envelope. In reverse, player state events (such as track progression) reach the page as named notifications received via `fb2k.on('playback:trackChanged', handler)`. Endpoint invocations use dot notation (`playback.play`), while event notifications use colon notation (`playback:trackChanged`). The complete wire format is documented in [Bridge protocol](/reference/bridge).

All interactions between the web page and the player traverse this Bridge channel. Outside foobar2000 (such as in an ordinary browser tab), `window.fb2k` is absent, and SDK calls resolve with a `NOT_SUPPORTED` failure envelope. Target page selection and directory resolution follow the priority rules detailed in [How a page is found](./page-loading.md).

## Native UI selection, themes and persistent configuration

### First launch and the native theme menu

This architecture baseline is **component 2.2.2** (2026-11-16), verified against `VERSION` and `src/version.h`; the historical checkout directory name `local-2.2.1` is not a version declaration. The testbed DLL also reports 2.2.2; the production deployment version is reported by operations, not reinstalled by this documentation update.

On a fresh profile, foobar2000 may first show its **UI module selection dialog**. Select **WebView2 UI** to use the component as the main UI. This host-owned choice is separate from choosing an HTML theme and from the default theme's onboarding. The component registers `WebViewUI` with `user_interface_factory`; a missing web page before this choice is not proof of a failed Bridge.

The native **View → WebView2 UI → Switch Theme** menu (Chinese: **View → WebView2 UI → 切换主题**) dynamically enumerates installed template directories under `profile/webview-ui/`, skipping dot-prefixed directories. The current template is radio-checked; no templates produces a disabled placeholder. Selecting a child rechecks the directory, persists the template name and reloads the main/background windows and panels that follow the global template; panels with their own template or URL override are not changed.

A dynamic child is addressed by **owner command GUID + child `subGuid`**, not by its translated label or the parent GUID alone. The Switch Theme owner is `{C8D9E0F3-2A3B-4C5D-6E7F-8091A2B3C4D7}`; each child GUID is derived from the template name through the SDK MD5 hasher. Obtain current addresses from menu discovery and pass both fields to `discovery.executeMainMenuCommand` (or the corresponding main-menu API). The container itself is not an executable leaf. Source anchors: `src/ui/WebViewMainMenu.cpp` and `src/api/DiscoveryApi.cpp`.

### Theme selection authority (not marker-file precedence)

The runtime authority is **`cfg_var.B7E8F3A2-4C5D-2E9F-8A1B-6C7D8E9F0A2C`** in `configStrings` within `profile/config.sqlite`. `GetActiveTemplateName()` reads `cfg_active_template`, whose default is `default`; an empty value also resolves to `default`. **`webview-ui/active-template.txt` is an installer/packaging legacy marker, not a runtime fallback in this 2.2.2 implementation.** Thus the old shorthand “config > marker > default” must not be interpreted as an implemented three-stage selector.

The 2026-11-16 operational check kept config at `foobox-style`, changed the marker to `default` and restarted: the page remained foobox-style. Deleting `profile/config.sqlite` in the isolated testbed reset the selection to **default**, not to the marker's previous theme. Do not delete the database merely to clear theme history: it also holds other host/component settings. Runtime theme switching should use the native menu or Preferences, not direct SQLite edits.

Template-name selection is distinct from page-directory fallback. For local pages, `FrontendDirectoryResolver/Policy` tries a panel-specific template (when present), the configured active template, bundled resources, then `profile/webview-ui/default`; a candidate must have `index.html`. Development URL overrides are covered in [How a page is found](./page-loading.md). Source anchors: `src/prefs/PreferencesPage.cpp` and `src/core/FrontendDirectoryResolver.cpp`.

### Portable configuration and default-theme onboarding

`config.*` stores its JSON object under the configStore string key **`foo_ui_webview2.portable_config`**, rather than in a file of that name. It is separate from the native active-template variable. For example, **`defaultTheme.onboarding.record`** is a literal dotted JSON key inside this object, not a separate SQLite column or nested-directory path. `ConfigApi.cpp` reads the SQLite-backed store first and migrates the old cfg_string only when the new value is absent; unreadable/non-object data is left untouched rather than overwritten with an empty object.

The installed default frontend checks the onboarding record and the current window identity after connection/startup checks. A successful read with no record, a **main** window and no blocking startup flow permits onboarding. Its steps are **Library → Appearance → Tray → Updates and online content**, with back/next, skip and finish controls. The writer persists `{version: 1, outcome: ...}` at the above key. This is **default-theme policy**, not a native component-wide wizard; selecting foobox-style does not require running it. The first-launch native UI dialog precedes and is independent of this web flow. This behavior was checked in the installed default frontend 0.2.0 onboarding module; it is not inferred from the component's version number.

### CDP and inherited browser arguments

The component's Developer setting enables remote debugging and supplies `--remote-debugging-port=<port>` when the WebView2 environment is created; the configurable range is **1024–65535**, with **9222** as the default, not an always-open fixed service. Switch/port changes require a process restart. See `src/webview/WebViewEnvironment.cpp` and `src/prefs/PreferencesCdpPort.h`.

A launcher can also set **`WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS`** before starting foobar2000; the testbed launcher explicitly includes `--remote-debugging-port=9222` plus anti-occlusion/backgrounding options. These are inherited WebView2 startup arguments, not a Bridge configuration key. Changing the launching shell's variable after the browser process exists does not reconfigure that process. Check the actual `http://127.0.0.1:9222/json/version` and target/profile identity rather than assuming the Developer checkbox alone explains the endpoint; environment overrides or another running instance can affect the observation. Keep CDP local and scoped to the testbed.

## Evidence and review materials

Source repositories must not contain **`_evidence-*` directories, review reports or screenshot evidence**, including disguised locations such as an in-repository `.dev/evidence` or `docs/review/`. For this theme/component collaboration, store these artifacts outside the source tree in the sibling **`fang-foobox-theme.dev`**, with build/HEAD, time and target-profile identity. Published contracts and this architecture page may remain in documentation; raw review/evidence artifacts must not be added beside them. The theme-side migration and reference updates are recorded by `9db73f3c` / `3435b8e4`. Documentation-only updates do not imply that runtime tests have been rerun.

<a id="the-bridge-and-the-sdk"></a>

## Bridge foundation and SDK layering

Developers can build full-featured themes directly using the underlying Bridge; however, raw `fb2k.invoke` calls and event subscriptions rely on string identifiers without compile-time type guarantees. The official npm package `foo-webview-sdk` provides three tiered abstraction layers on top of the wire protocol:

- **Typed client APIs (`fb.*`)**: Methods such as `fb.player.play()` and `fb.playlist.getAll()` provide TypeScript definitions and IDE autocompletion, and call the same host methods.
- **Functional Web Components (`fb-*`)**: Out-of-the-box custom elements such as playback controls and playlist views, encapsulating host communication and state synchronization with zero built-in visual styling (exposing CSS parts and slots for custom design).
- **SMP compatibility layer**: A migration adapter for existing Spider Monkey Panel (SMP) scripts, emulating its objects, properties, and events.

The component exposes more than 400 methods and 100 events, grouped into namespaces such as `playback`, `playlist`, `library` and `window`. The [API overview](/api/overview) lists every namespace with its method count and the page that documents it; [SDK namespaces](/sdk/namespaces) lists the `fb.*` wrappers over them.

::: tip Migrating from Spider Monkey Panel (SMP)
The SDK includes an SMP compatibility layer providing:
- **35 SMP event mappings**: Direct alignment for core event hooks like `on_playback_new_track`
- **8 wrapper classes**: `FbMetadbHandle`, `FbTitleFormat`, `ContextMenuManager`, and related utilities
- **Full `fb` / `plman` objects**: Synchronous properties, playback controls, and playlist manipulation

The compatibility layer employs a "local cache + event stream" architecture to bridge asynchronous host endpoints into SMP's synchronous property model. While this eventually-consistent cache suits UI rendering, operations that need precise transactional control should invoke `fb2k.invoke()` directly. Note: Legacy GDI/GDI+ immediate-mode drawing logic (such as `on_paint(gr)` callbacks and graphics context calls) cannot be ported directly; visual rendering must be rebuilt using modern Web standards (HTML, CSS, SVG, or Canvas 2D/WebGL). See [SMP compatibility](/reference/smp-compat).
:::

## Design principles

> foo_ui_webview2 follows "the component provides capabilities; the app decides policy."

### What the component provides

| Category | Description | Examples |
| --- | --- | --- |
| foobar2000 core capabilities | Playback, playlists, library, artwork | `playback.*`, `playlist.*` |
| Window management | DWM effects, caption, scaling | `window.*` |
| System integration | Files, dialogs, shell | `file.*`, `dialog.*` |
| Audio analysis | Spectrum and waveform | `audio.*` |
| Config storage | Persistent key-value storage | `config.*` |
| Streaming helpers | Generic URL playback, JIT queue | `jitQueue.*` |

### What the theme implements

| Category | Description | Why not in the component |
| --- | --- | --- |
| Online service integration | Spotify, NetEase, QQ Music, etc. | Auth and business flows differ |
| Multi-source lyrics aggregation | Online search and translation merge | Business logic |
| Artwork cache policy | Prefetch and LRU | Policy choice |
| UI theme and layout | Colors, fonts, animation | App-specific |

### Example: capability vs policy

```javascript
// Component capability: a URL the page can use as an image source
const current = await fb2k.invoke('playback.getCurrentTrack');
if (current.success && current.track) {
    const res = await fb2k.invoke('artwork.getFb2kUrlByPath', { path: current.track.path, maxSize: 200 });
    const img = document.querySelector('img.cover');
    if (res.success && img instanceof HTMLImageElement) img.src = res.dataUrl;
}

// Theme policy: which covers to fetch ahead of time, and how long to keep them
function prefetchCovers(urls) {
    urls.forEach((url) => {
        new Image().src = url;
    });
}
```
