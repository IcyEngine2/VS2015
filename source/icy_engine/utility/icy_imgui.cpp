#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS 1

#include <icy_engine/utility/icy_imgui.hpp>
#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_render.hpp>
#include "../../libs/imgui/imgui.h"

#if _DEBUG
#pragma comment(lib, "imguid")
#pragma comment(lib, "icy_engine_graphicsd")
#else
#pragma comment(lib, "imgui")
#pragma comment(lib, "icy_engine_graphics")
#endif

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
static std::atomic<uint32_t> imgui_query = 0;
class imgui_system_data;
enum class internal_message_type : uint32_t
{
    none,
    display_create,
    display_destroy,
    display_repaint,
    widget_create,
    widget_delete,
    widget_label,
    widget_state,
    widget_value,
    widget_udata,
    clear,
};
struct internal_message
{
    internal_message_type type = internal_message_type::none;
    uint32_t display = 0;
    uint32_t query = 0;
    shared_ptr<window> handle;
    uint32_t parent = 0;
    uint32_t widget = 0;
    string label;
    imgui_widget_state state;
    variant value;
};
struct imgui_widget
{
    uint32_t parent = 0;
    imgui_widget_state state;
    string label;
    set<uint32_t> widgets;
    variant value;
    variant udata;
    array<char> buffer;
};
class imgui_display_data_usr : public imgui_display
{
public:
    imgui_display_data_usr(shared_ptr<imgui_system_data> system, const uint32_t index, const weak_ptr<icy::window> window) noexcept :
        m_system(system), m_index(index), m_window(window)
    {

    }
    ~imgui_display_data_usr() noexcept;
    uint32_t index() const noexcept override
    {
        return m_index;
    }
    shared_ptr<imgui_system> system() const noexcept override
    {
        if (auto system = shared_ptr<imgui_system_data>(m_system))
            return system;
        return nullptr;
    }
    shared_ptr<icy::window> handle() const noexcept override
    {
        if (auto window = shared_ptr<icy::window>(m_window))
            return window;
        return nullptr;
    }
    error_type repaint(uint32_t& query) const noexcept override;
    error_type widget_create(const uint32_t parent, const imgui_widget_type type, uint32_t& widget) const noexcept override;
    error_type widget_delete(const uint32_t widget) const noexcept override;
    error_type widget_label(const uint32_t widget, const string_view text) const noexcept override;
    error_type widget_state(const uint32_t widget, const imgui_widget_state state) const noexcept override;
    error_type widget_value(const uint32_t widget, const variant& value) const noexcept override;
    error_type widget_udata(const uint32_t widget, const variant& value) const noexcept override;
    error_type clear() const noexcept override;
private:
    weak_ptr<imgui_system_data> m_system;
    const uint32_t m_index = 0;
    mutable std::atomic<uint32_t> m_widget = 0;
    weak_ptr<icy::window> m_window;
};
class imgui_display_data_sys
{
public:
    imgui_display_data_sys() noexcept = default;
    imgui_display_data_sys(imgui_system_data& system, const uint32_t index) noexcept : m_system(&system), m_index(index)
    {

    }
    imgui_display_data_sys(imgui_display_data_sys&& rhs) noexcept : m_system(rhs.m_system), m_index(rhs.m_index),
        m_context(rhs.m_context), m_handle(std::move(rhs.m_handle)), m_wid(rhs.m_wid), m_now(rhs.m_now),
        m_cursor_type(rhs.m_cursor_type), m_texture(std::move(rhs.m_texture)), m_root(std::move(rhs.m_root)), m_demo(rhs.m_demo)
    {
        rhs.m_context = nullptr;
    }
    ~imgui_display_data_sys() noexcept
    {
        if (m_context)
            ImGui::DestroyContext(m_context);
    }
    ICY_DEFAULT_MOVE_ASSIGN(imgui_display_data_sys);
    error_type initialize(shared_ptr<window> window) noexcept;
    error_type input(const input_message& msg) noexcept;
    error_type repaint(render_gui_frame& output) noexcept;
    error_type repaint(const uint32_t index, imgui_widget& widget) noexcept;
    error_type resize(window_size size) noexcept;
    uint32_t wid() noexcept
    {
        return m_wid;
    }
    error_type widget_create(const uint32_t parent, const uint32_t widget, const imgui_widget_type type) noexcept;
    error_type widget_delete(const uint32_t widget) noexcept;
    error_type widget_label(const uint32_t widget, const string_view text) noexcept;
    error_type widget_state(const uint32_t widget, const imgui_widget_state state) noexcept;
    error_type widget_value(const uint32_t widget, variant&& value) noexcept;
    error_type widget_udata(const uint32_t widget, variant&& value) noexcept;
    void clear() noexcept;
private:
    imgui_system_data* m_system = nullptr;
    uint32_t m_index = 0;
    ImGuiContext* m_context = nullptr;
    weak_ptr<window> m_handle;
    uint32_t m_wid = 0;
    clock_type::time_point m_now = clock_type::now();
    ImGuiMouseCursor m_cursor_type = ImGuiMouseCursor_None;
    map<void*, guid> m_texture;
    map<uint32_t, imgui_widget> m_widgets;
    imgui_widget m_root;
    bool m_demo = false;
};
class imgui_system_data : public imgui_system
{
public:
    error_type initialize() noexcept;
    ~imgui_system_data() noexcept
    {
        if (m_thread)
            m_thread->wait();
        filter(0);
    }
private:
    error_type exec() noexcept override;
    error_type signal(const icy::event_data* event) noexcept override
    {
        return m_sync.wake();
    }
    const icy::thread& thread() const noexcept override
    {
        return *m_thread;
    }
    error_type create_display(shared_ptr<imgui_display>& display, shared_ptr<icy::window> handle) noexcept override;
private:
    sync_handle m_sync;
    shared_ptr<event_thread> m_thread;
    std::atomic<uint32_t> m_index = 0;
    map<uint32_t, imgui_display_data_sys> m_displays;
};
ICY_STATIC_NAMESPACE_END

imgui_display_data_usr::~imgui_display_data_usr() noexcept
{
    if (auto system = shared_ptr<imgui_system_data>(m_system))
    {
        internal_message msg;
        msg.type = internal_message_type::display_destroy;
        msg.display = m_index;
        system->post(nullptr, event_type::system_internal, std::move(msg));
    }
}
error_type imgui_display_data_usr::repaint(uint32_t& query) const noexcept
{
    auto system = shared_ptr<imgui_system_data>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_message msg;
    msg.type = internal_message_type::display_repaint;
    msg.display = m_index;
    msg.query = query = imgui_query.fetch_add(1, std::memory_order_acq_rel) + 1;
    return system->post(nullptr, event_type::system_internal, std::move(msg));
}
error_type imgui_display_data_usr::widget_create(const uint32_t parent, const imgui_widget_type type, uint32_t& widget) const noexcept
{
    auto system = shared_ptr<imgui_system_data>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_message msg;
    msg.type = internal_message_type::widget_create;
    msg.display = m_index;
    msg.parent = parent;
    msg.widget = widget = m_widget.fetch_add(1, std::memory_order_acq_rel) + 1;
    msg.state.widget_type = type;
    return system->post(nullptr, event_type::system_internal, std::move(msg));
}
error_type imgui_display_data_usr::widget_delete(const uint32_t widget) const noexcept
{
    auto system = shared_ptr<imgui_system_data>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);
    if (!widget)
        return error_type();
    internal_message msg;
    msg.type = internal_message_type::widget_delete;
    msg.display = m_index;
    msg.widget = widget;
    return system->post(nullptr, event_type::system_internal, std::move(msg));
}
error_type imgui_display_data_usr::widget_label(const uint32_t widget, const string_view text) const noexcept
{
    auto system = shared_ptr<imgui_system_data>(m_system);
    if (!system || !widget)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_message msg;
    msg.type = internal_message_type::widget_label;
    msg.display = m_index;
    msg.widget = widget;
    ICY_ERROR(copy(text, msg.label));
    return system->post(nullptr, event_type::system_internal, std::move(msg));
}
error_type imgui_display_data_usr::widget_state(const uint32_t widget, const imgui_widget_state state) const noexcept
{
    auto system = shared_ptr<imgui_system_data>(m_system);
    if (!system || !widget)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_message msg;
    msg.type = internal_message_type::widget_state;
    msg.display = m_index;
    msg.widget = widget;
    msg.state = state;
    return system->post(nullptr, event_type::system_internal, std::move(msg));
}
error_type imgui_display_data_usr::widget_value(const uint32_t widget, const variant& value) const noexcept
{
    auto system = shared_ptr<imgui_system_data>(m_system);
    if (!system || !widget)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_message msg;
    msg.type = internal_message_type::widget_value;
    msg.display = m_index;
    msg.widget = widget;
    msg.value = value;
    return system->post(nullptr, event_type::system_internal, std::move(msg));
}
error_type imgui_display_data_usr::widget_udata(const uint32_t widget, const variant& value) const noexcept
{
    auto system = shared_ptr<imgui_system_data>(m_system);
    if (!system || !widget)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_message msg;
    msg.type = internal_message_type::widget_udata;
    msg.display = m_index;
    msg.widget = widget;
    msg.value = value;
    return system->post(nullptr, event_type::system_internal, std::move(msg));
}
error_type imgui_display_data_usr::clear() const noexcept
{
    auto system = shared_ptr<imgui_system_data>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_message msg;
    msg.type = internal_message_type::clear;
    msg.display = m_index;
    return system->post(nullptr, event_type::system_internal, std::move(msg));
}

error_type imgui_display_data_sys::initialize(shared_ptr<window> handle) noexcept
{
    m_context = ImGui::CreateContext();
    if (!m_context)
        return make_stdlib_error(std::errc::not_enough_memory);
        
    ImGui::SetCurrentContext(m_context);
    ImGui::StyleColorsDark();

    auto& io = ImGui::GetIO();

    const auto size = handle->size();
    // Build atlas
    auto tex_p = (unsigned char*)(nullptr);
    auto tex_w = 0;
    auto tex_h = 0;
    io.Fonts->GetTexDataAsRGBA32(&tex_p, &tex_w, &tex_h);
    
    matrix<color> colors(tex_h, tex_w);
    if (colors.empty())
        return make_stdlib_error(std::errc::not_enough_memory);

    for (auto row = 0; row < tex_h; ++row)
    {
        for (auto col = 0; col < tex_w; ++col)
        {
            const auto offset = (row * tex_w + col) * 4;
            colors.at(row, col) = color::from_rgba(tex_p[offset + 0], tex_p[offset + 1], tex_p[offset + 2], tex_p[offset + 3]);
        }
    }

    auto rsystem = resource_system::global();
    if (!rsystem)
        return make_unexpected_error();

    const auto new_guid = guid::create();
    ICY_ERROR(rsystem->store(new_guid, colors));
    ICY_ERROR(m_texture.insert(nullptr, new_guid));

    io.DisplaySize = { float(size.x), float(size.y) };
    io.KeyMap[ImGuiKey_Tab] = uint32_t(key::tab);
    io.KeyMap[ImGuiKey_LeftArrow] = uint32_t(key::left);
    io.KeyMap[ImGuiKey_RightArrow] = uint32_t(key::right);
    io.KeyMap[ImGuiKey_UpArrow] = uint32_t(key::up);
    io.KeyMap[ImGuiKey_DownArrow] = uint32_t(key::down);
    io.KeyMap[ImGuiKey_PageUp] = uint32_t(key::page_up);
    io.KeyMap[ImGuiKey_PageDown] = uint32_t(key::page_down);
    io.KeyMap[ImGuiKey_Home] = uint32_t(key::home);
    io.KeyMap[ImGuiKey_End] = uint32_t(key::end);
    io.KeyMap[ImGuiKey_Insert] = uint32_t(key::insert);
    io.KeyMap[ImGuiKey_Delete] = uint32_t(key::del);
    io.KeyMap[ImGuiKey_Backspace] = uint32_t(key::back);
    io.KeyMap[ImGuiKey_Space] = uint32_t(key::space);
    io.KeyMap[ImGuiKey_Enter] = uint32_t(key::enter);
    io.KeyMap[ImGuiKey_Escape] = uint32_t(key::esc);
    io.KeyMap[ImGuiKey_KeyPadEnter] = uint32_t(key::num_enter);
    io.KeyMap[ImGuiKey_A] = 'A';
    io.KeyMap[ImGuiKey_C] = 'C';
    io.KeyMap[ImGuiKey_V] = 'V';
    io.KeyMap[ImGuiKey_X] = 'X';
    io.KeyMap[ImGuiKey_Y] = 'Y';
    io.KeyMap[ImGuiKey_Z] = 'Z';
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_IsSRGB;

    m_handle = handle;
    m_wid = handle->index();

    return error_type();
}
error_type imgui_display_data_sys::input(const input_message& msg) noexcept
{
    ImGui::SetCurrentContext(m_context);
    auto& io = ImGui::GetIO();
    
    if (msg.type == input_type::key_press)
    {
        switch (msg.key)
        {
        case key::left_ctrl:
        case key::right_ctrl:
            io.KeyCtrl = true;
            break;
        case key::left_alt:
        case key::right_alt:
            io.KeyAlt = true;
            break;
        case key::left_shift:
        case key::right_shift:
            io.KeyShift = true;
            break;
        case key::left_win:
        case key::right_win:
            io.KeySuper = true;
            break;
        }
        io.KeysDown[uint32_t(msg.key)] = true;
    }
    else if (msg.type == input_type::key_release)
    {
        switch (msg.key)
        {
        case key::left_ctrl:
        case key::right_ctrl:
            io.KeyCtrl = false;
            break;
        case key::left_alt:
        case key::right_alt:
            io.KeyAlt = false;
            break;
        case key::left_shift:
        case key::right_shift:
            io.KeyShift = false;
            break;
        case key::left_win:
        case key::right_win:
            io.KeySuper = false;
            break;
        }
        io.KeysDown[uint32_t(msg.key)] = false;
    }
    else if (msg.type == input_type::text)
    {
        char text[5] = {};
        memcpy(text, msg.text, sizeof(msg.text));
        io.AddInputCharactersUTF8(text);
    }
    else if (msg.type == input_type::mouse_press || msg.type == input_type::mouse_double)
    {
        switch (msg.key)
        {
        case key::mouse_left:   io.MouseDown[0] = true; break;
        case key::mouse_right:  io.MouseDown[1] = true; break;
        case key::mouse_mid:    io.MouseDown[2] = true; break;
        case key::mouse_x1:     io.MouseDown[3] = true; break;
        case key::mouse_x2:     io.MouseDown[4] = true; break;
        }
    }
    else if (msg.type == input_type::mouse_release)
    {
        switch (msg.key)
        {
        case key::mouse_left:   io.MouseDown[0] = false; break;
        case key::mouse_right:  io.MouseDown[1] = false; break;
        case key::mouse_mid:    io.MouseDown[2] = false; break;
        case key::mouse_x1:     io.MouseDown[3] = false; break;
        case key::mouse_x2:     io.MouseDown[4] = false; break;
        }
    }
    else if (msg.type == input_type::mouse_wheel)
    {
        io.MouseWheel += msg.wheel;
    }

    switch (msg.type)
    {
    case input_type::mouse_move:
    case input_type::mouse_press:
    case input_type::mouse_release:
    case input_type::mouse_double:
    case input_type::mouse_wheel:
        io.MousePos = { float(msg.point_x), float(msg.point_y) };
        break;
    }

    // Update OS mouse cursor with the cursor requested by imgui
    auto mouse_cursor = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
    if (m_cursor_type != mouse_cursor)
    {
        m_cursor_type = mouse_cursor;
        if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) == 0)
        {
            if (auto window = shared_ptr<icy::window>(m_handle))
            {
                if (mouse_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
                {
                    // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
                    ICY_ERROR(window->cursor(1, nullptr));
                }
                else
                {
                    // Show OS mouse cursor
                    auto type = window_cursor::type::arrow;
                    switch (mouse_cursor)
                    {
                    case ImGuiMouseCursor_Arrow:        type = window_cursor::type::arrow; break;
                    case ImGuiMouseCursor_TextInput:    type = window_cursor::type::ibeam; break;
                    case ImGuiMouseCursor_ResizeAll:    type = window_cursor::type::size; break;
                    case ImGuiMouseCursor_ResizeEW:     type = window_cursor::type::size_x; break;
                    case ImGuiMouseCursor_ResizeNS:     type = window_cursor::type::size_y; break;
                    case ImGuiMouseCursor_ResizeNESW:   type = window_cursor::type::size_diag1; break;
                    case ImGuiMouseCursor_ResizeNWSE:   type = window_cursor::type::size_diag0; break;
                    case ImGuiMouseCursor_Hand:         type = window_cursor::type::hand; break;
                    case ImGuiMouseCursor_NotAllowed:   type = window_cursor::type::no; break;
                    }
                    shared_ptr<window_cursor> cursor;
                    ICY_ERROR(create_window_cursor(cursor, type));
                    ICY_ERROR(window->cursor(1, cursor));
                }
            }
        }
    }
    return error_type();
}
error_type imgui_display_data_sys::repaint(const uint32_t index, imgui_widget& widget) noexcept
{
    auto visible = !(widget.state.widget_flags & imgui_widget_flag::is_hidden);
    if (!visible)
        return error_type();

    if (widget.state.widget_flags & imgui_widget_flag::is_same_line)
        ImGui::SameLine();

    if (widget.state.widget_flags & imgui_widget_flag::is_repeat)
        ImGui::PushButtonRepeat(true);
    
    if (widget.state.widget_flags & imgui_widget_flag::is_bullet)
        ImGui::Bullet();

    const auto on_exit = [&]
    {
        switch (widget.state.widget_type)
        {
        case imgui_widget_type::window: if (widget.parent) ImGui::EndChild(); else ImGui::End(); break;
        case imgui_widget_type::popup: if (visible) ImGui::EndPopup(); break;
        case imgui_widget_type::modal: if (visible) ImGui::EndPopup(); break;
        case imgui_widget_type::group: ImGui::EndGroup(); break;
        case imgui_widget_type::tab_bar: if (visible) ImGui::EndTabBar(); break;
        case imgui_widget_type::tree_view: if (visible) ImGui::TreePop(); break;
        case imgui_widget_type::list_view: if (visible) ImGui::EndListBox(); break;
        case imgui_widget_type::combo_view: if (visible) ImGui::EndCombo(); break;
        case imgui_widget_type::menu: if (visible) ImGui::EndMenu(); break;
        case imgui_widget_type::menu_context: if (visible) ImGui::EndPopup(); break;
        case imgui_widget_type::menu_bar: if (visible) ImGui::EndMenuBar(); break;
        case imgui_widget_type::main_menu_bar: if (visible) ImGui::EndMainMenuBar(); break;
        case imgui_widget_type::tab_item: if (visible) ImGui::EndTabItem(); break;
        }
        if (widget.state.widget_flags & imgui_widget_flag::is_repeat)
            ImGui::PopButtonRepeat();
    };

    const auto state = widget.state;
    const auto label = widget.label.bytes().data();

    imgui_event new_event;

    switch (const auto type = widget.state.widget_type)
    {
    case imgui_widget_type::window:
        if (widget.parent)
        {
            visible = ImGui::BeginChild(label, ImVec2(0, 0), false, widget.state.custom_flags);
        }
        else
        {
            auto show = true;
            visible = ImGui::Begin(label, &show, widget.state.custom_flags);
            if (!show)
            {
                widget.state.widget_flags |= imgui_widget_flag::is_hidden;
                new_event.type = imgui_event_type::window_close;
            }
        }
        break;

    case imgui_widget_type::popup:
        ImGui::OpenPopup(label, widget.state.custom_flags);
        visible = ImGui::BeginPopup(label);
        break;

    case imgui_widget_type::modal:
    {
        auto show = true;
        ImGui::OpenPopup(label, widget.state.custom_flags);
        visible = ImGui::BeginPopupModal(label, &show, widget.state.custom_flags);
        if (!show)
        {
            widget.state.widget_flags |= imgui_widget_flag::is_hidden;
            new_event.type = imgui_event_type::window_close;
        }
        break;
    }

    case imgui_widget_type::group:
        ImGui::BeginGroup();
        break;
    
    case imgui_widget_type::text:
    {
        string str;
        ICY_ERROR(to_string(widget.value, str));
        ImGui::TextUnformatted(str.bytes().data());
        break;
    }

    case imgui_widget_type::tab_bar:
        visible = ImGui::BeginTabBar(label, widget.state.custom_flags);
        break;

    case imgui_widget_type::tree_view:
        visible = ImGui::TreeNode(label);
        break;

    case imgui_widget_type::list_view:
        visible = ImGui::BeginListBox(label);
        break;

    case imgui_widget_type::combo_view:
    {
        string str;
        ICY_ERROR(to_string(widget.value, str));
        visible = ImGui::BeginCombo(label, str.bytes().data(), widget.state.custom_flags);
        break;
    }

    case imgui_widget_type::button:
        if (ImGui::Button(label))
            new_event.type = imgui_event_type::widget_click;     
        break;

    case imgui_widget_type::button_check:
    {
        auto checked = false;
        to_value(widget.value, checked);
        if (ImGui::Checkbox(label, &checked))
        {
            new_event.value = widget.value = checked;
            new_event.type = imgui_event_type::widget_edit;
        }
        visible = checked;
        break;
    }

    case imgui_widget_type::button_radio:
    {
        auto active = false;
        to_value(widget.value, active);
        if (ImGui::RadioButton(label, active))
        {
            new_event.value = widget.value = true;
            new_event.type = imgui_event_type::widget_edit;
            auto it = m_widgets.find(widget.parent);
            if (it != m_widgets.end())
            {
                for (auto&& child : it->value.widgets)
                {
                    if (child == index)
                        continue;

                    auto jt = m_widgets.find(child);
                    if (jt == m_widgets.end())
                        continue;

                    if (jt->value.state.widget_type != imgui_widget_type::button_radio)
                        continue;

                    jt->value.value = false;
                }
            }
        }
        visible = active;
        break;
    }
    case imgui_widget_type::button_arrow_min_x:
    case imgui_widget_type::button_arrow_max_x:
    case imgui_widget_type::button_arrow_min_y:
    case imgui_widget_type::button_arrow_max_y:
    {
        auto dir = ImGuiDir_None;
        switch (widget.state.widget_type)
        {
        case imgui_widget_type::button_arrow_min_x: dir = ImGuiDir_Left; break;
        case imgui_widget_type::button_arrow_max_x: dir = ImGuiDir_Right; break;
        case imgui_widget_type::button_arrow_min_y: dir = ImGuiDir_Up; break;
        case imgui_widget_type::button_arrow_max_y: dir = ImGuiDir_Down; break;
        }
        if (visible = ImGui::ArrowButton(label, dir))
            new_event.type = imgui_event_type::widget_click;
        break;
    }

    case imgui_widget_type::menu:
        visible = ImGui::BeginMenu(label);
        break;

    case imgui_widget_type::menu_bar:
        visible = ImGui::BeginMenuBar();
        break;

    case imgui_widget_type::main_menu_bar:
        visible = ImGui::BeginMainMenuBar();
        break;

    case imgui_widget_type::drag_value:
    case imgui_widget_type::slider_value:
        break;

    case imgui_widget_type::line_input:
    case imgui_widget_type::text_input:
    {
        if (widget.state.widget_type == imgui_widget_type::line_input)
        {
            if (ImGui::InputText(label, widget.buffer.data(), widget.buffer.size(), widget.state.custom_flags))
            {
                string_view str;
                to_string(widget.buffer, str);
                new_event.value = str;
                new_event.type = imgui_event_type::widget_edit;
            }
        }
        else
        {
            if (ImGui::InputTextMultiline(label, widget.buffer.data(), widget.buffer.size(), ImVec2(), widget.state.custom_flags))
            {
                string_view str;
                to_string(widget.buffer, str);
                new_event.value = str;
                new_event.type = imgui_event_type::widget_edit;
            }
        }
        break;
    }
    case imgui_widget_type::color_edit:
    {
        color color;
        to_value(widget.value, color);
        float rgba[4];
        color.to_rgbaf(rgba);
        if (ImGui::ColorEdit4(label, rgba, widget.state.custom_flags))
        {
            new_event.value = widget.value = color::from_rgbaf(rgba[0], rgba[1], rgba[2], rgba[3]);
            new_event.type = imgui_event_type::widget_edit;
        }
        break;
    }
    case imgui_widget_type::color_select:
    {
        color color;
        to_value(widget.value, color);
        float rgba[4];
        color.to_rgbaf(rgba);
        if (ImGui::ColorPicker4(label, rgba, widget.state.custom_flags))
        {
            new_event.value = widget.value = color::from_rgbaf(rgba[0], rgba[1], rgba[2], rgba[3]);
            new_event.type = imgui_event_type::widget_edit;
        }
        break;
    }    
    case imgui_widget_type::menu_item:
    {
        auto selected = false;
        widget.value.get(selected);
        const auto enabled = !(widget.state.widget_flags & imgui_widget_flag::is_disabled);
        if (visible = ImGui::MenuItem(label, nullptr, selected, enabled))
            new_event.type = imgui_event_type::widget_click;
        break;
    }
    case imgui_widget_type::menu_context:
        visible = ImGui::BeginPopupContextItem();
        break;

    case imgui_widget_type::tab_item:
    {
        auto open = true;
        visible = ImGui::BeginTabItem(label, &open, widget.state.custom_flags);
        if (!open)
        {
            widget.state.widget_flags |= imgui_widget_flag::is_hidden;
            new_event.type = imgui_event_type::widget_close;
        }
        break;
    }
    case imgui_widget_type::view_item:
    {
        auto selected = false;
        to_value(widget.value, selected);
        if (ImGui::Selectable(label, selected, widget.state.custom_flags))
        {
            new_event.value = widget.value = !selected;
            new_event.type = imgui_event_type::widget_edit;
        }
        break;
    }
    case imgui_widget_type::separator:
        ImGui::Separator();
        break;


    }

    if (widget.state.widget_flags & (imgui_widget_flag::is_drag_source | imgui_widget_flag::is_drop_target))
        ImGui::PushID(index);
    
    ICY_SCOPE_EXIT
    {
        if (widget.state.widget_flags & (imgui_widget_flag::is_drag_source | imgui_widget_flag::is_drop_target))
            ImGui::PopID();
    };

    if (widget.state.widget_flags & imgui_widget_flag::is_drag_source)
    {
        if (ImGui::BeginDragDropSource(widget.state.custom_flags))
        {
            ImGui::SetDragDropPayload("IcyImGui", &index, sizeof(index));
            ImGui::EndDragDropSource();
        }
    }
    if (widget.state.widget_flags & imgui_widget_flag::is_drop_target)
    {
        if (ImGui::BeginDragDropTarget())
        {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("IcyImGui");
            if (payload && payload->DataSize == sizeof(uint32_t))
            {
                const auto drag_index = *static_cast<uint32_t*>(payload->Data);
                const auto it = m_widgets.find(drag_index);
                if (it != m_widgets.end())
                {
                    new_event.type = imgui_event_type::drag_drop;
                    new_event.value = drag_index;
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    ICY_SCOPE_EXIT{ on_exit(); };
    if (visible)
    {
        for (auto&& index : widget.widgets)
        {
            auto it = m_widgets.find(index);
            if (it != m_widgets.end())
                ICY_ERROR(repaint(it->key, it->value));
        }
    }
    if (new_event.type != imgui_event_type::none)
    {
        new_event.display = m_index;
        new_event.widget = index;
        new_event.udata = widget.udata;
        ICY_ERROR(event::post(m_system, event_type::gui_update, std::move(new_event)));
    }
    return error_type();
}
error_type imgui_display_data_sys::repaint(render_gui_frame& output) noexcept
{
    ImGui::SetCurrentContext(m_context);
    auto& io = ImGui::GetIO();
    
    const auto now = clock_type::now();
    io.DeltaTime = 1e-9f * std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_now).count();
    m_now = now;

    {
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
        if (m_demo)
            ImGui::ShowDemoWindow(&m_demo);
        ICY_SCOPE_EXIT{ ImGui::Render(); };
        for (auto&& index : m_root.widgets)
        {
            auto it = m_widgets.find(index);
            if (it != m_widgets.end())
                ICY_ERROR(repaint(it->key, it->value));
        }
    }

    const auto data = ImGui::GetDrawData();
    output.viewport.vec[0] = data->DisplayPos.x;
    output.viewport.vec[1] = data->DisplayPos.y;
    output.viewport.vec[2] = data->DisplaySize.x;
    output.viewport.vec[3] = data->DisplaySize.y;
    output.vbuffer = data->TotalVtxCount;
    output.ibuffer = data->TotalIdxCount;

    ICY_ERROR(output.data.reserve(data->CmdListsCount));
    for (auto k = 0; k < data->CmdListsCount; ++k)
    {
        const auto list = data->CmdLists[k];
        render_gui_list new_list;
        ICY_ERROR(new_list.idx.assign(list->IdxBuffer.Data, list->IdxBuffer.Data + list->IdxBuffer.Size));
        ICY_ERROR(new_list.vtx.resize(list->VtxBuffer.Size));
        ICY_ERROR(new_list.cmd.reserve(list->CmdBuffer.Size));
        for (auto n = 0; n < list->VtxBuffer.Size; ++n)
        {
            const auto kx = 2.0f / (data->DisplaySize.x);
            const auto ky = 2.0f / (data->DisplaySize.y);

            new_list.vtx[n].col = color::from_rgba(list->VtxBuffer[n].col);
            new_list.vtx[n].tex.x = list->VtxBuffer[n].uv.x;
            new_list.vtx[n].tex.y = list->VtxBuffer[n].uv.y;

            new_list.vtx[n].pos.x = list->VtxBuffer[n].pos.x * kx - 1;
            new_list.vtx[n].pos.y = 1 - list->VtxBuffer[n].pos.y * ky;
        }
        
        for (auto n = 0; n < list->CmdBuffer.Size; ++n)
        {
            const auto& cmd = list->CmdBuffer[n];
            const auto it = m_texture.find(cmd.GetTexID());
            if (it == m_texture.end())
                return make_unexpected_error();

            render_gui_cmd new_cmd;
            memcpy(&new_cmd.clip, &cmd.ClipRect, sizeof(cmd.ClipRect));
            new_cmd.tex = it->value;
            new_cmd.vtx = cmd.VtxOffset;
            new_cmd.idx = cmd.IdxOffset;
            new_cmd.size = cmd.ElemCount;
            ICY_ERROR(new_list.cmd.push_back(std::move(new_cmd)));
        }
        ICY_ERROR(output.data.push_back(std::move(new_list)));
    }
    return error_type();
}
error_type imgui_display_data_sys::resize(const window_size size) noexcept
{
    ImGui::SetCurrentContext(m_context);
    auto& io = ImGui::GetIO();
    io.DisplaySize = { float(size.x), float(size.y) };
    return error_type();
}
error_type imgui_display_data_sys::widget_create(const uint32_t parent, const uint32_t widget, const imgui_widget_type type) noexcept
{
    if (parent)
    {
        if (!m_widgets.try_find(parent))
            return error_type();
    }

    auto jt = m_widgets.find(widget);
    if (jt != m_widgets.end())
        return error_type();

    ICY_ERROR(m_widgets.insert(widget, imgui_widget(), &jt));
    jt->value.state.widget_type = type;
    jt->value.state.widget_flags = 0;
    jt->value.state.custom_flags = 0;
    jt->value.parent = parent;

    if (type == imgui_widget_type::popup ||
        type == imgui_widget_type::modal)
    {
        jt->value.state.widget_flags = 0u;
    }
    if (type == imgui_widget_type::line_input ||
        type == imgui_widget_type::text_input)
    {
        ICY_ERROR(jt->value.buffer.resize(4096));
    }

    const auto it = m_widgets.find(parent);
    if (it != m_widgets.end())
    {
        ICY_ERROR(it->value.widgets.try_insert(widget));
    }
    else
    {
        ICY_ERROR(m_root.widgets.try_insert(widget));
    }

    ICY_ERROR(jt->value.label.appendf("##%1"_s, widget));

    return error_type();
}
error_type imgui_display_data_sys::widget_delete(const uint32_t widget) noexcept
{
    if (widget == 0)
        return error_type();
    
    auto it = m_widgets.find(widget);
    if (it == m_widgets.end())
        return error_type();

    if (it->value.parent)
    {
        const auto pt = m_widgets.find(it->value.parent);
        if (pt != m_widgets.end())
        {
            auto jt = pt->value.widgets.find(widget);
            if (jt != pt->value.widgets.end())
                pt->value.widgets.erase(jt);
        }
    }
    else
    {
        auto jt = m_root.widgets.find(widget);
        if (jt != m_root.widgets.end())
            m_root.widgets.erase(jt);
    }

    set<uint32_t> indices;
    ICY_ERROR(indices.insert(widget));
    if (!it->value.widgets.empty())
    {
        ++it;
        for (; it != m_widgets.end(); ++it)
        {
            if (indices.try_find(it->value.parent))
            {
                ICY_ERROR(indices.insert(it->key));
            }
        }
    }
    for (auto k = indices.size(); k; --k)
    {
        const uint32_t index = indices.data()[k - 1];
        const auto jt = m_widgets.find(index);
        if (jt != m_widgets.end())
            m_widgets.erase(jt);
    }
    return error_type();
}
error_type imgui_display_data_sys::widget_label(const uint32_t widget, const string_view text) noexcept
{
    auto it = m_widgets.find(widget);
    if (it == m_widgets.end())
        return error_type();

    it->value.label.clear();
    ICY_ERROR(it->value.label.appendf("%1##%2"_s, text, it->key));
    return error_type();
}
error_type imgui_display_data_sys::widget_state(const uint32_t widget, const imgui_widget_state state) noexcept
{
    auto it = m_widgets.find(widget);
    if (it == m_widgets.end())
        return error_type();

    if (state.custom_flags != 0xFFFFFFFF)
    {
        it->value.state.custom_flags = state.custom_flags;
    }
    if (state.widget_flags != imgui_widget_flag::any)
    {
        it->value.state.widget_flags = state.widget_flags;
    }
    return error_type();
}
error_type imgui_display_data_sys::widget_value(const uint32_t widget, variant&& value) noexcept
{
    auto it = m_widgets.find(widget);
    if (it == m_widgets.end())
        return error_type();

    if (it->value.state.widget_type == imgui_widget_type::line_input ||
        it->value.state.widget_type == imgui_widget_type::text_input)
    {
        string str;
        ICY_ERROR(to_string(value, str));

        auto& buffer = it->value.buffer;
        const auto capacity = buffer.capacity();
        buffer.clear();
        ICY_ERROR(buffer.assign(str.bytes()));
        ICY_ERROR(buffer.push_back(0));
        ICY_ERROR(buffer.resize(std::max(buffer.capacity(), capacity)));
    }
    else
    {
        it->value.value = std::move(value);
    }
    return error_type();
}
error_type imgui_display_data_sys::widget_udata(const uint32_t widget, variant&& value) noexcept
{
    auto it = m_widgets.find(widget);
    if (it == m_widgets.end())
        return error_type();
    
    it->value.udata = std::move(value);
    return error_type();
}
void imgui_display_data_sys::clear() noexcept
{
    m_widgets.clear();
    m_root = imgui_widget();
}

error_type imgui_system_data::initialize() noexcept
{
    ICY_ERROR(m_sync.initialize());
    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;

    if (!IMGUI_CHECKVERSION())
        return make_unexpected_error();

    const auto alloc = [](size_t size, void*) { return icy::realloc(nullptr, size); };
    const auto free = [](void* ptr, void*) { icy::realloc(ptr, 0); };
    ImGui::SetAllocatorFunctions(alloc, free, nullptr);

    const uint64_t event_types = 0
        | event_type::system_internal
        | event_type::window_input
        | event_type::window_action
        | event_type::window_resize;

    filter(event_types);
    return error_type();
}
error_type imgui_system_data::exec() noexcept
{
    while (*this)
    {    
        map<uint32_t, array<imgui_event>> repaint;
        while (auto event = pop())
        {
            if (event->type == event_type::system_internal)
            {
                auto& event_data = event->data<internal_message>();
                auto it = m_displays.find(event_data.display);
                if (event_data.type == internal_message_type::display_create)
                {
                    if (it == m_displays.end())
                    {
                        ICY_ERROR(m_displays.insert(event_data.display, imgui_display_data_sys(*this, event_data.display), &it));
                        ICY_ERROR(it->value.initialize(event_data.handle));
                    }
                }
                else if (event_data.type == internal_message_type::display_destroy)
                {
                    if (it != m_displays.end())
                        m_displays.erase(it);
                }
                else if (event_data.type == internal_message_type::display_repaint)
                {
                    if (it != m_displays.end())
                    {
                        imgui_event new_event;
                        new_event.type = imgui_event_type::display_render;
                        new_event.query = event_data.query;
                        new_event.display = it->key;
                        auto jt = repaint.find(it->key);
                        if (jt == repaint.end())
                        {
                            ICY_ERROR(repaint.insert(it->key, array<imgui_event>(), &jt));
                        }
                        ICY_ERROR(jt->value.push_back(std::move(new_event)));
                    }
                }
                else if (it != m_displays.end())
                {
                    switch (event_data.type)
                    {
                    case internal_message_type::widget_create:
                    {
                        ICY_ERROR(it->value.widget_create(event_data.parent, event_data.widget, event_data.state.widget_type));
                        break;
                    }
                    case internal_message_type::widget_delete:
                    {
                        ICY_ERROR(it->value.widget_delete(event_data.widget));
                        break;
                    }
                    case internal_message_type::widget_label:
                    {
                        ICY_ERROR(it->value.widget_label(event_data.widget, std::move(event_data.label)));
                        break;
                    }
                    case internal_message_type::widget_state:
                    {
                        ICY_ERROR(it->value.widget_state(event_data.widget, event_data.state));
                        break;
                    }
                    case internal_message_type::widget_value:
                    {
                        ICY_ERROR(it->value.widget_value(event_data.widget, std::move(event_data.value)));
                        break;
                    }
                    case internal_message_type::widget_udata:
                    {
                        ICY_ERROR(it->value.widget_udata(event_data.widget, std::move(event_data.value)));
                        break;
                    }
                    case internal_message_type::clear:
                    {
                        it->value.clear();
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
            else if (event->type == event_type::window_input)
            {
                const auto& event_data = event->data<window_message>();
                for (auto&& pair : m_displays)
                {
                    if (pair.value.wid() == event_data.window)
                    {
                        ICY_ERROR(pair.value.input(event_data.input));
                        break;
                    }
                }
            }
            else if (event->type == event_type::window_resize)
            {
                const auto& event_data = event->data<window_message>();
                for (auto&& pair : m_displays)
                {
                    if (pair.value.wid() == event_data.window)
                    {
                        ICY_ERROR(pair.value.resize(event_data.size));
                        break;
                    }
                }
            }            
        }

        for (auto&& pair : repaint)
        {
            auto it = m_displays.find(pair.key);
            if (it == m_displays.end())
                continue;

            ICY_ERROR(it->value.repaint(pair.value.back().render));
            for (auto&& event : pair.value)
                ICY_ERROR(event::post(this, event_type::gui_render, std::move(event)));
        }
        ICY_ERROR(m_sync.wait());
    }
    return error_type();
}
error_type imgui_system_data::create_display(shared_ptr<imgui_display>& display, shared_ptr<icy::window> handle) noexcept
{
    if (!handle)
        return make_stdlib_error(std::errc::invalid_argument);

    shared_ptr<imgui_display_data_usr> ptr;
    const auto index = m_index.fetch_add(1, std::memory_order_acq_rel) + 1;
    ICY_ERROR(make_shared(ptr, make_shared_from_this(this), index, handle));
    internal_message msg;
    msg.type = internal_message_type::display_create;
    msg.display = index;
    msg.handle = handle;
    ICY_ERROR(event_system::post(nullptr, event_type::system_internal, std::move(msg)));
    display = std::move(ptr);
    return error_type();
}

error_type icy::create_imgui_system(shared_ptr<imgui_system>& system) noexcept
{
    shared_ptr<imgui_system_data> new_system;
    ICY_ERROR(make_shared(new_system));
    ICY_ERROR(new_system->initialize());
    system = std::move(new_system);
    return error_type();
}