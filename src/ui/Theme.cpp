#include "ui/Theme.hpp"

#include <algorithm>
#include <memory>
#include <utility>

namespace ui {

using namespace ftxui;

Element panel(const std::string& title, Element body, bool focused, PanelStyle style) {
    Color focus_color = theme::Accent;
    std::string icon = " ◆ ";

    switch (style) {
        case PanelStyle::Primary:
            focus_color = theme::Accent;
            icon = " ◆ ";
            break;
        case PanelStyle::Header:
            focus_color = theme::Accent;
            icon = " ❖ ";
            break;
        case PanelStyle::SubNav:
            focus_color = theme::Accent;
            icon = " ◈ ";
            break;
        case PanelStyle::Action:
            focus_color = theme::Good;
            icon = " ✦ ";
            break;
        case PanelStyle::Highlight:
            focus_color = theme::Warn;
            icon = " ★ ";
            break;
    }

    Element title_el = focused
        ? (text(icon + title + " ") | bold | color(focus_color))
        : (text(" " + title + " ") | color(Color::GrayLight));
    Element content = std::move(body) | color(Color::White);
    return window(std::move(title_el), std::move(content), ROUNDED)
           | color(focused ? focus_color : theme::Muted);
}

Element sectionHeader(const std::string& title, Color bar_color) {
    return hbox({
        text(" ▎ ") | bold | color(bar_color),
        text(title) | bold | color(Color::White),
        filler()
    });
}

Element col(Element el, int width, bool align_right) {
    if (align_right) {
        return hbox({filler(), std::move(el)}) | size(WIDTH, EQUAL, width);
    }
    return std::move(el) | size(WIDTH, EQUAL, width);
}

Element col(const std::string& text_str, int width, bool align_right) {
    return col(text(text_str), width, align_right);
}

Element kv(const std::string& key, Element value) {
    return hbox({
        text(key) | color(theme::Muted),
        filler(),
        std::move(value)
    });
}

Element kv(const std::string& key, const std::string& value) {
    return kv(key, text(value) | color(Color::White));
}

namespace {

bool isLeftPress(Event event) {
    return event.is_mouse() && event.mouse().button == Mouse::Left &&
           event.mouse().motion == Mouse::Pressed;
}

class PassTabComponent : public ComponentBase {
public:
    explicit PassTabComponent(Component child) {
        Add(std::move(child));
    }

    bool OnEvent(Event event) override {
        if (event == Event::Tab || event == Event::TabReverse) {
            return false;
        }
        return ComponentBase::OnEvent(event);
    }
};

// ---------------------------------------------------------------------------
//  胶囊色块按钮：操作类按钮自带 [文案] 边框标识与底色，彻底区分于普通静态文本
// ---------------------------------------------------------------------------
struct ButtonPalette {
    Color fg;
    Color focus_bg;
    bool bold_fg;
};

ButtonPalette paletteOf(ButtonKind kind) {
    switch (kind) {
        case ButtonKind::Primary:   return {theme::Accent, theme::Accent, true};
        case ButtonKind::Danger:    return {theme::Bad, theme::Bad, true};
        case ButtonKind::Link:      return {theme::Accent, Color::White, true};
        case ButtonKind::Tab:       return {Color::GrayLight, Color::White, false};
        case ButtonKind::TabActive: return {Color::White, Color::White, true};
        case ButtonKind::Disabled:  return {theme::Muted, theme::Muted, false};
        case ButtonKind::Secondary: break;
    }
    return {Color::White, Color::White, false};
}

class StyledButtonComponent : public ComponentBase {
public:
    StyledButtonComponent(std::function<std::string()> label,
                          std::function<void()> on_click,
                          std::function<ButtonKind()> kind)
        : label_(std::move(label)), on_click_(std::move(on_click)), kind_(std::move(kind)) {}

    Element OnRender() override {
        const ButtonKind kind = kind_ ? kind_() : ButtonKind::Secondary;
        const ButtonPalette pal = paletteOf(kind);
        const std::string raw = label_ ? label_() : std::string();

        const bool is_nav_tab = (kind == ButtonKind::Tab ||
                                 kind == ButtonKind::TabActive ||
                                 kind == ButtonKind::Link);
        const bool already_bracketed = !raw.empty() && (raw.front() == '[' || raw.back() == ']');

        std::string display_str;
        if (is_nav_tab || already_bracketed) {
            display_str = " " + raw + " ";
        } else {
            display_str = "[" + raw + "]";
        }

        Element el = text(display_str);

        if (Focused()) {
            el = std::move(el) | bold | color(pal.fg) | bgcolor(theme::CursorBg) | underlined;
        } else {
            el = std::move(el) | color(pal.fg);
            if (pal.bold_fg) el = std::move(el) | bold;
            if (kind == ButtonKind::TabActive) {
                el = std::move(el) | bgcolor(theme::RowSelectBg);
            } else if (hovered_ && kind != ButtonKind::Disabled) {
                el = std::move(el) | bgcolor(theme::CursorBg) | underlined;
            } else if (!is_nav_tab) {
                el = std::move(el) | bgcolor(theme::BadgeBg);
            }
        }
        return std::move(el) | reflect(box_);
    }

    bool OnEvent(Event event) override {
        if (event.is_mouse()) {
            hovered_ = box_.Contain(event.mouse().x, event.mouse().y);
            if (hovered_ && isLeftPress(event)) {
                TakeFocus();
                if (on_click_) on_click_();
                return true;
            }
            return false; // 悬停只更新外观，不抢焦点、不吞事件
        }
        if (event == Event::Return || event == Event::Character(' ')) {
            if (on_click_) on_click_();
            return true;
        }
        return false;
    }

    bool Focusable() const override { return true; }

private:
    std::function<std::string()> label_;
    std::function<void()> on_click_;
    std::function<ButtonKind()> kind_;
    Box box_;
    bool hovered_ = false;
};

// ---------------------------------------------------------------------------
//  大号醒目主操作按钮（复刻原版登录按钮：外层圆角框 + 居中胶囊悬停/选中高亮效果）
// ---------------------------------------------------------------------------
class StyledBigButtonComponent : public ComponentBase {
public:
    StyledBigButtonComponent(std::function<std::string()> label,
                             std::function<void()> on_click,
                             std::function<ButtonKind()> kind)
        : label_(std::move(label)), on_click_(std::move(on_click)), kind_(std::move(kind)) {}

    Element OnRender() override {
        const ButtonKind kind = kind_ ? kind_() : ButtonKind::Primary;
        const ButtonPalette pal = paletteOf(kind);
        const std::string raw = label_ ? label_() : std::string();

        Element inner = text("[  " + raw + "  ]");
        if (Focused()) {
            inner = std::move(inner) | bold | color(pal.fg) | bgcolor(theme::CursorBg) | underlined;
        } else {
            inner = std::move(inner) | color(pal.fg);
            if (pal.bold_fg) inner = std::move(inner) | bold;
            if (hovered_ && kind != ButtonKind::Disabled) {
                inner = std::move(inner) | bgcolor(theme::CursorBg) | underlined;
            } else {
                inner = std::move(inner) | bgcolor(theme::BadgeBg);
            }
        }

        const Color border_c = (kind == ButtonKind::Disabled) ? theme::Muted : pal.fg;
        return (std::move(inner) | center)
               | borderStyled(ROUNDED, border_c)
               | reflect(box_);
    }

    bool OnEvent(Event event) override {
        if (event.is_mouse()) {
            hovered_ = box_.Contain(event.mouse().x, event.mouse().y);
            if (hovered_ && isLeftPress(event)) {
                TakeFocus();
                if (on_click_) on_click_();
                return true;
            }
            return false;
        }
        if (event == Event::Return || event == Event::Character(' ')) {
            if (on_click_) on_click_();
            return true;
        }
        return false;
    }

    bool Focusable() const override { return true; }

private:
    std::function<std::string()> label_;
    std::function<void()> on_click_;
    std::function<ButtonKind()> kind_;
    Box box_;
    bool hovered_ = false;
};

// ---------------------------------------------------------------------------
//  输入框包装：Esc 先退出输入
// ---------------------------------------------------------------------------
class InputComponent : public ComponentBase {
public:
    explicit InputComponent(Component input) {
        Add(std::move(input));
    }

    bool OnEvent(Event event) override {
        if (event == Event::Escape && Focused()) {
            ComponentBase* root = this;
            while (root->Parent()) root = root->Parent();
            focusFirstNonInput(root);
            return true;
        }
        return ComponentBase::OnEvent(event);
    }

    bool Focusable() const override { return true; }

private:
    static bool focusFirstNonInput(ComponentBase* node) {
        if (dynamic_cast<InputComponent*>(node) != nullptr) return false;
        if (!node->Focusable()) return false;
        if (node->ChildCount() == 0) {
            node->TakeFocus();
            return true;
        }
        for (std::size_t i = 0; i < node->ChildCount(); ++i) {
            if (focusFirstNonInput(node->ChildAt(i).get())) return true;
        }
        return false;
    }
};

// ---------------------------------------------------------------------------
//  光标 / 选中 两步模型列表
// ---------------------------------------------------------------------------
class DataListComponent : public ComponentBase {
public:
    DataListComponent(int* selected,
                      std::function<int()> count_fn,
                      std::function<Element(int, bool, bool)> row_fn,
                      std::function<void()> on_select,
                      std::string empty_hint)
        : selected_(selected),
          count_fn_(std::move(count_fn)),
          row_fn_(std::move(row_fn)),
          on_select_(std::move(on_select)),
          empty_hint_(std::move(empty_hint)) {}

    Element OnRender() override {
        const int n = sync();
        if (n == 0) {
            return vbox({
                text("  " + empty_hint_) | color(theme::Muted),
                filler()
            }) | flex;
        }
        const bool focused = Focused();
        if (!focused) cursor_ = *selected_;
        boxes_.resize(static_cast<std::size_t>(n));

        Elements rows;
        rows.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            const bool sel = (i == *selected_);
            const bool cur = (focused && i == cursor_) || i == hover_;
            Element prefix = text(sel ? "▌" : " ") | color(sel ? theme::Accent : theme::Muted);
            Element row = hbox({std::move(prefix), row_fn_(i, sel, focused) | xflex});
            if (sel && cur) {
                row = std::move(row) | bgcolor(theme::SelectCursorBg) | bold;
            } else if (sel) {
                row = std::move(row) | bgcolor(focused ? theme::RowSelectBg : theme::RowHoverBg)
                      | (focused ? bold : nothing);
            } else if (cur) {
                row = std::move(row) | bgcolor(theme::CursorBg);
            }
            row = std::move(row) | reflect(boxes_[static_cast<std::size_t>(i)]);
            if ((focused && i == cursor_) || (!focused && sel)) {
                row = std::move(row) | focus;
            }
            rows.push_back(std::move(row));
        }
        return vbox(std::move(rows)) | vscroll_indicator | yframe | reflect(box_) | flex;
    }

    bool OnEvent(Event event) override {
        const int n = sync();
        if (event.is_mouse()) return OnMouse(event, n);
        if (event == Event::Tab || event == Event::TabReverse) return false;
        if (n == 0) return false;

        if (event == Event::ArrowUp) {
            if (cursor_ <= 0) return false; // 到顶后交给父容器移动焦点
            --cursor_;
            hover_ = -1;
            return true;
        }
        if (event == Event::ArrowDown) {
            if (cursor_ >= n - 1) return false;
            ++cursor_;
            hover_ = -1;
            return true;
        }
        if (event == Event::Home) { cursor_ = 0; hover_ = -1; return true; }
        if (event == Event::End) { cursor_ = n - 1; hover_ = -1; return true; }
        if (event == Event::PageUp) { cursor_ = std::max(0, cursor_ - 10); hover_ = -1; return true; }
        if (event == Event::PageDown) { cursor_ = std::min(n - 1, cursor_ + 10); hover_ = -1; return true; }
        if (event == Event::Return) {
            select(cursor_);
            return true;
        }
        return false;
    }

    bool Focusable() const override { return true; }

private:
    int sync() {
        const int n = std::max(0, count_fn_ ? count_fn_() : 0);
        if (selected_) {
            *selected_ = (n > 0) ? std::clamp(*selected_, 0, n - 1) : 0;
        }
        cursor_ = (n > 0) ? std::clamp(cursor_, 0, n - 1) : 0;
        if (hover_ >= n) hover_ = -1;
        return n;
    }

    void select(int i) {
        if (selected_) *selected_ = i;
        if (on_select_) on_select_();
    }

    bool OnMouse(Event event, int n) {
        const Mouse& m = event.mouse();
        const bool inside = box_.Contain(m.x, m.y);

        if (m.button == Mouse::WheelUp || m.button == Mouse::WheelDown) {
            if (!inside || n == 0) return false;
            const int next = std::clamp(cursor_ + (m.button == Mouse::WheelUp ? -1 : 1), 0, n - 1);
            cursor_ = next;
            hover_ = -1;
            TakeFocus();
            return true;
        }

        // 悬停仅更新 hover_，不 TakeFocus、不改变选中，输入框焦点不会被抢走
        hover_ = -1;
        if (inside) {
            for (int i = 0; i < n && i < static_cast<int>(boxes_.size()); ++i) {
                if (boxes_[static_cast<std::size_t>(i)].Contain(m.x, m.y)) {
                    hover_ = i;
                    break;
                }
            }
        }
        if (isLeftPress(event) && hover_ >= 0) {
            TakeFocus();
            cursor_ = hover_;
            select(hover_);
            return true;
        }
        return false;
    }

    int* selected_ = nullptr;
    std::function<int()> count_fn_;
    std::function<Element(int, bool, bool)> row_fn_;
    std::function<void()> on_select_;
    std::string empty_hint_;
    int cursor_ = 0;
    int hover_ = -1;
    Box box_;
    std::vector<Box> boxes_;
};

// ---------------------------------------------------------------------------
//  横向单选切换栏：同样采用光标/选中两步模型
// ---------------------------------------------------------------------------
class InlineToggleComponent : public ComponentBase {
public:
    InlineToggleComponent(const std::vector<std::string>* labels,
                          int* selected,
                          std::function<void()> on_change)
        : labels_(labels), selected_(selected), on_change_(std::move(on_change)) {}

    Element OnRender() override {
        const int n = sync();
        const bool focused = Focused();
        if (!focused) cursor_ = *selected_;
        boxes_.resize(static_cast<std::size_t>(n));

        Elements items;
        for (int i = 0; i < n; ++i) {
            const bool sel = (i == *selected_);
            const bool cur = (focused && i == cursor_) || i == hover_;
            Element el = text(" " + std::string(sel ? "(•)" : "( )") + (*labels_)[static_cast<std::size_t>(i)] + " ");
            if (sel) {
                el = std::move(el) | bold | color(theme::Accent);
            } else {
                el = std::move(el) | color(cur ? Color::White : theme::Muted);
            }
            if (cur) el = std::move(el) | bgcolor(theme::CursorBg);
            items.push_back(std::move(el) | reflect(boxes_[static_cast<std::size_t>(i)]));
        }
        return hbox(std::move(items));
    }

    bool OnEvent(Event event) override {
        const int n = sync();
        if (event.is_mouse()) {
            const Mouse& m = event.mouse();
            hover_ = -1;
            for (int i = 0; i < n; ++i) {
                if (boxes_[static_cast<std::size_t>(i)].Contain(m.x, m.y)) {
                    hover_ = i;
                    break;
                }
            }
            if (isLeftPress(event) && hover_ >= 0) {
                TakeFocus();
                cursor_ = hover_;
                select(hover_);
                return true;
            }
            return false;
        }
        if (n == 0) return false;
        if (event == Event::ArrowLeft) {
            if (cursor_ <= 0) return false;
            --cursor_; hover_ = -1;
            return true;
        }
        if (event == Event::ArrowRight) {
            if (cursor_ >= n - 1) return false;
            ++cursor_; hover_ = -1;
            return true;
        }
        if (event == Event::Return || event == Event::Character(' ')) {
            select(cursor_);
            return true;
        }
        return false;
    }

    bool Focusable() const override { return true; }

private:
    int sync() {
        const int n = labels_ ? static_cast<int>(labels_->size()) : 0;
        if (selected_) *selected_ = (n > 0) ? std::clamp(*selected_, 0, n - 1) : 0;
        cursor_ = (n > 0) ? std::clamp(cursor_, 0, n - 1) : 0;
        if (hover_ >= n) hover_ = -1;
        return n;
    }

    void select(int i) {
        if (*selected_ == i) return;
        *selected_ = i;
        if (on_change_) on_change_();
    }

    const std::vector<std::string>* labels_;
    int* selected_;
    std::function<void()> on_change_;
    int cursor_ = 0;
    int hover_ = -1;
    std::vector<Box> boxes_;
};

} // namespace

Component PassTab(Component child) {
    return Make<PassTabComponent>(std::move(child));
}

Component InlineToggle(const std::vector<std::string>* labels,
                       int* selected,
                       std::function<void()> on_change) {
    return Make<InlineToggleComponent>(labels, selected, std::move(on_change));
}

Component StyledButton(std::function<std::string()> label,
                       std::function<void()> on_click,
                       std::function<ButtonKind()> kind) {
    return Make<StyledButtonComponent>(std::move(label), std::move(on_click), std::move(kind));
}

Component ActionButton(const std::string& label,
                       std::function<void()> on_click,
                       bool is_primary) {
    return StyledButton(
        [label] { return label; },
        std::move(on_click),
        [is_primary] { return is_primary ? ButtonKind::Primary : ButtonKind::Secondary; });
}

Component DangerButton(const std::string& label,
                       std::function<void()> on_click) {
    return StyledButton(
        [label] { return label; },
        std::move(on_click),
        [] { return ButtonKind::Danger; });
}

Component StyledBigButton(std::function<std::string()> label,
                          std::function<void()> on_click,
                          std::function<ButtonKind()> kind) {
    return Make<StyledBigButtonComponent>(std::move(label), std::move(on_click), std::move(kind));
}

Component BigActionButton(const std::string& label,
                          std::function<void()> on_click,
                          ButtonKind kind) {
    return StyledBigButton(
        [label] { return label; },
        std::move(on_click),
        [kind] { return kind; });
}

Component TextInput(std::string* value,
                    const std::string& placeholder,
                    bool is_password,
                    std::function<void()> on_enter,
                    std::function<void()> on_change) {
    InputOption opt = InputOption::Default();
    opt.multiline = false;
    opt.password = is_password;
    opt.on_enter = std::move(on_enter);
    opt.on_change = std::move(on_change);
    opt.transform = [](InputState st) -> Element {
        Element el = std::move(st.element) | color(st.is_placeholder ? theme::Muted : Color::White);
        const Color bg = st.focused ? theme::RowSelectBg : (st.hovered ? theme::CursorBg : theme::BadgeBg);
        Element box = hbox({text(" "), std::move(el) | xflex, text(" ")}) | bgcolor(bg);
        return st.focused ? (std::move(box) | bold) : std::move(box);
    };
    return Make<InputComponent>(Input(value, placeholder, opt));
}

Component DataList(int* selected,
                   std::function<int()> count_fn,
                   std::function<Element(int index, bool active, bool focused)> row_fn,
                   std::function<void()> on_select,
                   const std::string& empty_hint) {
    return Make<DataListComponent>(
        selected, std::move(count_fn), std::move(row_fn), std::move(on_select), empty_hint);
}

} // namespace ui
