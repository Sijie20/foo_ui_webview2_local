// ============================================
// WebViewMainMenu.cpp - View 菜单下的「WebView2 UI」分组与显示/隐藏窗口命令
// ============================================

#include "pch.h"
#include "ui/BackgroundService.h"
#include "ui/UserInterface.h"
#include "window/MainWindow.h"
#include "prefs/PreferencesPage.h"
#include "core/WebViewContext.h"
#include "core/WebViewPanel.h"
#include "utils/I18n.h"

// ============================================
// Main menu command to toggle background window
// ============================================
static const GUID guid_mainmenu_group_webview = 
    { 0xc8d9e0f1, 0x2a3b, 0x4c5d, { 0x6e, 0x7f, 0x80, 0x91, 0xa2, 0xb3, 0xc4, 0xd5 } };

static const GUID guid_cmd_toggle_window = 
    { 0xc8d9e0f2, 0x2a3b, 0x4c5d, { 0x6e, 0x7f, 0x80, 0x91, 0xa2, 0xb3, 0xc4, 0xd6 } };

static mainmenu_group_popup_factory g_mainmenu_group(
    guid_mainmenu_group_webview,
    mainmenu_groups::view,
    mainmenu_commands::sort_priority_dontcare,
    "WebView2 UI"
);

namespace {
// 2.2.1 has no public webview_prefs::ReloadFrontendsForTemplateChange(bool).
// Equivalent to forceLocal=true: bypass the dev-server suppression for an
// explicit menu choice, navigating main/background windows to the local theme.
void ReloadFrontendsForExplicitTemplateChange() {
    std::vector<MainWindow*> windows;
    if (auto* ui = WebViewUI::GetInstance()) {
        if (ui->GetMainWindow()) windows.push_back(ui->GetMainWindow());
    }
    if (MainWindow* background = background_service::GetBackgroundWindow()) {
        if (std::find(windows.begin(), windows.end(), background) == windows.end()) {
            windows.push_back(background);
        }
    }
    for (MainWindow* window : windows) window->ReloadFrontendForTemplateChange();

    // Only panels following the global template participate, as in Preferences.
    auto& context = WebViewContext::GetInstance();
    for (HWND hwnd : context.GetAllInstances()) {
        if (!IsWindow(hwnd)) continue;
        WebViewPanel* panel = context.GetPanelByHwnd(hwnd);
        if (!panel) continue;
        const auto& config = panel->GetPanelConfig();
        if (!config.templateName.empty() || !config.urlOverride.empty()) continue;
        panel->ReloadFrontend();
    }
}

class ThemeCommand : public mainmenu_node_command {
public:
    explicit ThemeCommand(std::string name) : name_(std::move(name)) {}
    void get_display(pfc::string_base& text, t_uint32& flags) override {
        text = name_.empty() ? TRU("No available themes", "无可用主题") : name_.c_str();
        flags = name_.empty() ? mainmenu_commands::flag_disabled : 0;
        if (!name_.empty() && webview_prefs::GetActiveTemplateName() ==
            pfc::stringcvt::string_wide_from_utf8(name_.c_str()).get_ptr()) {
            flags |= mainmenu_commands::flag_radiochecked;
        }
    }
    GUID get_guid() override {
        auto hasher = hasher_md5::get();
        hasher_md5_state state;
        hasher->initialize(state);
        hasher->process(state, name_.data(), name_.size());
        return hasher->get_result_guid(state);
    }
    void execute(service_ptr_t<service_base>) override {
        // Recheck the directory: a snapshot menu may outlive a removed theme.
        if (name_.empty() || !webview_prefs::TemplateExists(name_)) return;
        webview_prefs::SetActiveTemplateName(name_);
        if (webview_prefs::GetActiveTemplateName() ==
            pfc::stringcvt::string_wide_from_utf8(name_.c_str()).get_ptr()) {
            // An explicit menu selection must navigate even when a failed dev
            // server override left the currently visible page on a local theme.
            ReloadFrontendsForExplicitTemplateChange();
        }
    }
private:
    const std::string name_;
};

class ThemeGroup : public mainmenu_node_group {
public:
    ThemeGroup() : names_(webview_prefs::GetTemplateList()) {
        for (const auto& name : names_) children_.push_back(fb2k::service_new<ThemeCommand>(name));
        if (children_.empty()) children_.push_back(fb2k::service_new<ThemeCommand>(std::string()));
    }
    void get_display(pfc::string_base& text, t_uint32& flags) override {
        text = names_.empty()
            ? TRU("Switch Theme (No available themes)", "切换主题（无可用主题）")
            : TRU("Switch Theme", "切换主题");
        flags = names_.empty() ? mainmenu_commands::flag_disabled : 0;
    }
    t_size get_children_count() override { return children_.size(); }
    mainmenu_node::ptr get_child(t_size index) override {
        return children_.at(index);
    }
private:
    const std::vector<std::string> names_;
    std::vector<mainmenu_node::ptr> children_;
};
} // namespace

class MainMenuCommands : public mainmenu_commands_v2 {
public:
    t_uint32 get_command_count() override {
        return 2;
    }
    
    GUID get_command(t_uint32 index) override {
        switch (index) {
            case 0: return guid_cmd_toggle_window;
            case 1: return {0xc8d9e0f3, 0x2a3b, 0x4c5d, {0x6e,0x7f,0x80,0x91,0xa2,0xb3,0xc4,0xd7}};
            default: return pfc::guid_null;
        }
    }
    
    void get_name(t_uint32 index, pfc::string_base& out) override {
        switch (index) {
            case 0: out = TRU("Show/Hide Window", "显示/隐藏窗口"); break;
            case 1: out = TRU("Switch Theme", "切换主题"); break;
            default: break;
        }
    }
    
    bool get_description(t_uint32 index, pfc::string_base& out) override {
        switch (index) {
            case 0: 
                out = TRU("Toggle visibility of WebView2 UI window", "切换 WebView2 UI 窗口的可见性");
                return true;
            default:
                return false;
        }
    }
    
    GUID get_parent() override {
        return guid_mainmenu_group_webview;
    }
    
    bool is_command_dynamic(t_uint32 index) override { return index == 1; }
    mainmenu_node::ptr dynamic_instantiate(t_uint32 index) override {
        if (index == 1) return fb2k::service_new<ThemeGroup>();
        return {};
    }

    void execute(t_uint32 index, service_ptr_t<service_base> callback) override {
        switch (index) {
            case 0:
                // Check if WebView2 UI is active as main interface
                if (WebViewUI::GetInstance()) {
                    WebViewUI::GetInstance()->activate();
                } else if (background_service::HasBackgroundWindow()) {
                    background_service::ToggleWindow();
                } else if (WebViewContext::GetInstance().GetInstanceCount() > 0) {
                    // 面板模式 - 没有独立窗口可切换
                    if (background_service::IsBackgroundModeEnabled()) {
                        // 后台模式已启用但窗口尚未创建，重新初始化
                        background_service::Initialize();
                        if (background_service::HasBackgroundWindow()) {
                            background_service::ShowWindow();
                        } else {
                            // 初始化失败，打开偏好设置页
                            ui_control::get()->show_preferences(webview_prefs::GetPreferencesPageGuid());
                        }
                    } else {
                        // 后台模式未启用，直接打开偏好设置页
                        ui_control::get()->show_preferences(webview_prefs::GetPreferencesPageGuid());
                    }
                } else {
                    // 无窗口也无面板，打开偏好设置页
                    ui_control::get()->show_preferences(webview_prefs::GetPreferencesPageGuid());
                }
                break;
            default: break;
        }
    }
    
    bool get_display(t_uint32 index, pfc::string_base& text, t_uint32& flags) override {
        flags = 0;
        get_name(index, text);
        if (index == 1 && webview_prefs::GetTemplateList().empty()) {
            text = TRU("Switch Theme (No available themes)", "切换主题（无可用主题）");
            flags |= flag_disabled;
        }
        
        if (index == 0) {
            if (WebViewUI::GetInstance()) {
                // 独立 UI 模式
                text = TRU("Show/Hide Window", "显示/隐藏窗口");
                if (WebViewUI::GetInstance()->is_visible()) flags |= flag_checked;
            } else if (background_service::HasBackgroundWindow()) {
                // 后台窗口模式
                text = TRU("Show/Hide Window", "显示/隐藏窗口");
                HWND hwnd = background_service::GetBackgroundWindow()->GetHwnd();
                if (hwnd && IsWindowVisible(hwnd)) flags |= flag_checked;
            } else {
                // 面板模式或无实例 - 显示为偏好设置入口
                text = TRU("Preferences...", "偏好设置...");
            }
        }
        
        return true;
    }
};

static mainmenu_commands_factory_t<MainMenuCommands> g_mainmenu_commands_factory;
