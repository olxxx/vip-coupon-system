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
ftxui::Component InlineToggle(const std::vector<std::string>* labels,
                              int* selected,
                              std::function<void()> on_change = {});

// 终端风格按钮: [ 文本 ]，聚焦或悬停时高亮反色
ftxui::Component ActionButton(const std::string& label,
                              std::function<void()> on_click,
                              bool is_primary = false);

// 单行输入框（自动带底框与聚焦变色）
ftxui::Component TextInput(std::string* value,
                           const std::string& placeholder = "",
                           bool is_password = false,
                           std::function<void()> on_enter = {},
                           std::function<void()> on_change = {});

// 自适应多列对齐列表组件：
// 每帧自动根据 count_fn() 同步行数并钳制 *selected，支持方向键、鼠标滚轮点击及 Enter 触发
ftxui::Component DataList(int* selected,
                          std::function<int()> count_fn,
                          std::function<ftxui::Element(int index, bool active, bool focused)> row_fn,
                          std::function<void()> on_enter = {},
                          const std::string& empty_hint = "暂无数据");

} // namespace ui
