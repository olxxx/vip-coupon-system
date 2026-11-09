#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <functional>
#include <string>
#include <vector>

namespace ui {

namespace theme {
// 核心语义调色板 (高对比度深色终端配色)
inline const ftxui::Color Accent      = ftxui::Color::RGB(56, 189, 248);  // #38BDF8 天蓝 (焦点/品牌/主强调)
inline const ftxui::Color Good        = ftxui::Color::RGB(52, 211, 153);  // #34D399 翠绿 (余额/立减/可用/成功)
inline const ftxui::Color Warn        = ftxui::Color::RGB(251, 191, 36);  // #FBBF24 琥珀 (会员等级/实付总额/周末)
inline const ftxui::Color Bad         = ftxui::Color::RGB(248, 113, 113); // #F87171 珊瑚红 (售罄/不可用/错误/拦截)
inline const ftxui::Color Muted       = ftxui::Color::RGB(113, 113, 122); // #71717A 锌灰 (失焦边框/表头/次要说明)
inline const ftxui::Color RowSelectBg = ftxui::Color::RGB(12, 74, 110);   // #0C4A6E 聚焦选中行深蓝底色
inline const ftxui::Color RowHoverBg  = ftxui::Color::RGB(30, 41, 59);    // #1E293B 失焦选中行暗灰蓝底色
inline const ftxui::Color BadgeBg     = ftxui::Color::RGB(24, 24, 27);    // #18181B 按钮常规底色
inline const ftxui::Color CursorBg    = ftxui::Color::RGB(51, 65, 85);    // #334155 光标/鼠标悬停行与控件底色
inline const ftxui::Color SelectCursorBg = ftxui::Color::RGB(14, 105, 160); // #0E69A0 选中行同时被光标/悬停覆盖时的底色
} // namespace theme

// --- 视觉排版原语 ---

// 焦点感知圆角面板：focused 为 true 时边框与标题呈天蓝色高亮，否则为暗灰细框
ftxui::Element panel(const std::string& title, ftxui::Element body, bool focused = false);

// 固定显示列宽单元（基于 FTXUI 终端字符宽度布局，天然支持 UTF-8 中文双宽对齐）
ftxui::Element col(ftxui::Element el, int width, bool align_right = false);
ftxui::Element col(const std::string& text_str, int width, bool align_right = false);

// 左右对齐键值行（用于详情面板）
ftxui::Element kv(const std::string& key, ftxui::Element value);
ftxui::Element kv(const std::string& key, const std::string& value);

// --- 交互组件封装 ---

// 透传 Tab / Shift+Tab 给父容器，防止 Menu / Toggle 吞噬焦点切换键
ftxui::Component PassTab(ftxui::Component child);

// 紧凑型单选切换栏 (如: (•)全部  ( )数码3C ...)
// 光标/选中两步模型：←/→ 与鼠标悬停移动光标（仅高亮），Enter/空格/单击才写入 *selected。
ftxui::Component InlineToggle(const std::vector<std::string>* labels,
                              int* selected,
                              std::function<void()> on_change = {});

// 按钮语义类型
enum class ButtonKind {
    Primary,    // 主操作：强调色文字
    Secondary,  // 次要操作：亮灰文字 + 括号胶囊标识
    Danger,     // 危险/删除操作：红色文字 + 括号胶囊标识
    Link,       // 页面跳转类：强调色文字，焦点反白
    Tab,        // 页签（未激活）
    TabActive,  // 页签（当前页）
    Disabled    // 不可用外观（仍会触发 on_click，由页面给出提示）
};

// 色块按钮。三态样式：
//   常态：操作按钮带 [文案] 胶囊底色与语义着色；鼠标悬停：淡底色 + 下划线（不改变焦点）；
//   键盘焦点：高亮底色 + 加粗 + 下划线。
// 鼠标仅在左键按下时获取焦点，移动不会抢走其他控件（如输入框）的焦点。
ftxui::Component StyledButton(std::function<std::string()> label,
                              std::function<void()> on_click,
                              std::function<ButtonKind()> kind);

// 静态文案按钮（is_primary 为 true 使用 Primary，否则 Secondary）
ftxui::Component ActionButton(const std::string& label,
                              std::function<void()> on_click,
                              bool is_primary = false);

// 危险/删除类静态文案按钮（使用 ButtonKind::Danger 珊瑚红高亮）
ftxui::Component DangerButton(const std::string& label,
                              std::function<void()> on_click);

// 单行输入框（常态暗底、悬停淡底、聚焦深蓝底）。
// 聚焦时按 Esc 先退出输入，焦点移到页面内第一个非输入控件（Esc 被消费，不会返回上一页）。
ftxui::Component TextInput(std::string* value,
                           const std::string& placeholder = "",
                           bool is_password = false,
                           std::function<void()> on_enter = {},
                           std::function<void()> on_change = {});

// 自适应多列对齐列表组件（光标/选中两步模型）：
// - 光标：方向键/滚轮/鼠标悬停移动，仅高亮，不改变 *selected；
// - 选中：Enter 或鼠标左键单击，写入 *selected 并触发 on_select。
// 每帧自动根据 count_fn() 同步行数并钳制 *selected。row_fn 的 active 参数表示该行是否为选中行。
ftxui::Component DataList(int* selected,
                          std::function<int()> count_fn,
                          std::function<ftxui::Element(int index, bool active, bool focused)> row_fn,
                          std::function<void()> on_select = {},
                          const std::string& empty_hint = "暂无数据");

} // namespace ui
