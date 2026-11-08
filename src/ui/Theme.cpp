#include "ui/Theme.hpp"

#include <algorithm>
#include <memory>
#include <utility>

namespace ui {

using namespace ftxui;

Element panel(const std::string& title, Element body, bool focused) {
    Element title_el = focused
        ? (text(" ◆ " + title + " ") | bold | color(theme::Accent))
        : (text(" " + title + " ") | color(Color::GrayLight));
    Element content = std::move(body) | color(Color::White);
    return window(std::move(title_el), std::move(content), ROUNDED)
           | color(focused ? theme::Accent : theme::Muted);
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

class DataListComponent : public ComponentBase {
public:
    DataListComponent(int* selected,
                      std::function<int()> count_fn,
                      std::function<Element(int, bool, bool)> row_fn,
                      std::function<void()> on_enter,
                      std::string empty_hint)
        : selected_(selected),
          count_fn_(std::move(count_fn)),
          row_fn_(std::move(row_fn)),
          empty_hint_(std::move(empty_hint)) {
        MenuOption opt = MenuOption::Vertical();
        opt.on_enter = std::move(on_enter);
        opt.entries_option.transform = [this](const EntryState& st) -> Element {
            const int total = count_fn_ ? count_fn_() : 0;
            if (st.index < 0 || st.index >= total) {
                return emptyElement();
            }
            const bool list_focused = Focused();
            Element prefix = text(st.active ? "▌" : " ")
                             | color(st.active ? theme::Accent : theme::Muted);
            Element row = hbox({
                std::move(prefix),
                row_fn_(st.index, st.active, list_focused) | xflex
            });
            if (st.active) {
                row = std::move(row)
                      | bgcolor(list_focused ? theme::RowSelectBg : theme::RowHoverBg)
                      | (list_focused ? bold : nothing)
                      | focus;
            }
            return row;
        };
        menu_ = Menu(&labels_, selected_, opt);
        Add(menu_);
    }

    void sync() {
        const int n = std::max(0, count_fn_ ? count_fn_() : 0);
        if (static_cast<int>(labels_.size()) != n) {
            labels_.assign(static_cast<std::size_t>(n), "");
        }
        if (selected_) {
            *selected_ = (n > 0) ? std::clamp(*selected_, 0, n - 1) : 0;
        }
    }

    Element OnRender() override {
        sync();
        if (labels_.empty()) {
            return vbox({
                text("  " + empty_hint_) | color(theme::Muted),
                filler()
            }) | flex;
        }
        return menu_->Render() | vscroll_indicator | yframe | flex;
    }

    bool OnEvent(Event event) override {
        if (event == Event::Tab || event == Event::TabReverse) {
            return false;
        }
        sync();
        if (labels_.empty()) {
            return false;
        }
        return menu_->OnEvent(event);
    }

    bool Focusable() const override {
        return true;
    }

private:
    int* selected_ = nullptr;
    std::function<int()> count_fn_;
    std::function<Element(int, bool, bool)> row_fn_;
    std::string empty_hint_;
    std::vector<std::string> labels_;
    Component menu_;
};

} // namespace

Component PassTab(Component child) {
    return Make<PassTabComponent>(std::move(child));
}

Component InlineToggle(const std::vector<std::string>* labels,
                       int* selected,
                       std::function<void()> on_change) {
    MenuOption opt = MenuOption::Horizontal();
    opt.on_change = std::move(on_change);
    opt.entries_option.transform = [](const EntryState& st) -> Element {
        const std::string dot = st.active ? "(•)" : "( )";
        Element el = text(" " + dot + st.label + " ");
        if (st.active) {
            el = std::move(el) | bold | color(theme::Accent);
        } else if (st.focused) {
            el = std::move(el) | color(Color::White) | underlined;
        } else {
            el = std::move(el) | color(theme::Muted);
        }
        return el;
    };
    return PassTab(Menu(labels, selected, opt));
}

Component ActionButton(const std::string& label,
                       std::function<void()> on_click,
                       bool is_primary) {
    ButtonOption opt;
    opt.label = label;
    opt.on_click = std::move(on_click);
    opt.transform = [is_primary](const EntryState& st) -> Element {
        Element el = text(" [ " + st.label + " ] ");
        if (st.focused) {
            return std::move(el) | bold | color(Color::Black)
                   | bgcolor(is_primary ? theme::Accent : Color::White);
        }
        if (is_primary) {
            return std::move(el) | bold | color(theme::Accent);
        }
        return std::move(el) | color(Color::GrayLight);
    };
    return Button(opt);
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
        st.element |= color(st.is_placeholder ? theme::Muted : Color::White);
        if (st.focused) {
            return hbox({text(" ") | bgcolor(theme::RowSelectBg),
                         std::move(st.element) | bgcolor(theme::RowSelectBg) | bold,
                         text(" ") | bgcolor(theme::RowSelectBg)});
        }
        return hbox({text("["), std::move(st.element), text("]")}) | color(theme::Muted);
    };
    return Input(value, placeholder, opt);
}

Component DataList(int* selected,
                   std::function<int()> count_fn,
                   std::function<Element(int index, bool active, bool focused)> row_fn,
                   std::function<void()> on_enter,
                   const std::string& empty_hint) {
    return Make<DataListComponent>(
        selected, std::move(count_fn), std::move(row_fn), std::move(on_enter), empty_hint);
}

} // namespace ui
