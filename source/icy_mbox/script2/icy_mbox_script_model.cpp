#include "icy_mbox_script_main.hpp"
using namespace icy;

ICY_STATIC_NAMESPACE_BEG
class mbox_model_explorer : public mbox_model
{
    struct node_type
    {
        error_type initialize(const mbox::base& base) noexcept
        {
            index = base.index;
            parent = base.parent;
            type = base.type;
            ICY_ERROR(to_string(base.name, name));
            return {};
        }
        icy::guid index;
        icy::guid parent;
        icy::string name;
        mbox::type type = mbox::type::directory;
        icy::dictionary<icy::guid> nodes;
    };
public:
    using mbox_model::mbox_model;
    error_type reset(const mbox::library& library, const guid& root, const mbox::type filter) noexcept override;
    error_type exec(const event event) noexcept override;
    error_type context(const gui_variant& var) noexcept override;
    xgui_node find(const gui_variant& var) noexcept override
    {
        if (const auto ptr = m_data.try_find(var.as_guid()))
            return node(*ptr);
        return xgui_node();
    }
private:
    xgui_node node(const node_type& mbox_node) noexcept;
    error_type initialize(const node_type& mbox_node, xgui_node gui_node) noexcept;
private:
    mbox::type m_filter = mbox::type::_total;
    icy::map<icy::guid, node_type> m_data;
};
class mbox_model_directory : public mbox_model
{
    struct node_type
    {
        error_type initialize(const mbox::base& base, const mbox::library* const library) noexcept
        {
            index = base.index;
            type = base.type;
            if (base.type == mbox::type::command ||
                base.type == mbox::type::character)
            {
                size = base.actions.size();
            }
            else if (base.type == mbox::type::directory && library)
            {
                array<guid> vec2;
                ICY_ERROR(library->children(base.index, false, vec2));
                size = vec2.size();
            }
            return {};
        }
        guid index;
        mbox::type type = mbox::type::directory;
        size_t size = 0;
    };
    enum
    {
        column_name,
        column_type,
        column_size,
        _column_total,
    };
public:
    using mbox_model::mbox_model;
    error_type reset(const mbox::library& library, const guid& root, const mbox::type filter) noexcept override;
    error_type exec(const event event) noexcept override;
    error_type context(const gui_variant& var) noexcept override;
    xgui_node find(const gui_variant& var) noexcept override
    {
        return {};
    }
private:
    icy::error_type initialize(const size_t row, const string_view name, const node_type& node) noexcept;
private:
    const mbox::library* m_library = nullptr;
    icy::dictionary<node_type> m_data;
};
class mbox_model_command : public mbox_model
{
    enum
    {
        column_type,
        column_value,
        column_args,
        _column_total,
    };
public:
    using mbox_model::mbox_model;
    error_type reset(const mbox::library& library, const guid& root, const mbox::type filter) noexcept override;
    error_type exec(const event event) noexcept override;
    error_type context(const gui_variant& var) noexcept override;
    xgui_node find(const gui_variant& var) noexcept override
    {
        return {};
    }
    void lock() noexcept override
    {
        m_type = mbox_model_type::view_command;
    }
    void unlock() noexcept override
    {
        m_type = mbox_model_type::edit_command;
    }
private:
    error_type initialize(const size_t row) noexcept;
private:
    const mbox::library* m_library = nullptr;
    icy::array<mbox::action> m_data;
};
ICY_STATIC_NAMESPACE_END

error_type mbox_model::create(const mbox_model_type type, unique_ptr<mbox_model>& model) noexcept
{
    unique_ptr<mbox_model> ptr;
    switch (type)
    {
    case mbox_model_type::explorer:
        ptr = make_unique<mbox_model_explorer>(type);
        break;

    case mbox_model_type::directory:
        ptr = make_unique<mbox_model_directory>(type);
        break;

    case mbox_model_type::view_command:
    case mbox_model_type::edit_command:
        ptr = make_unique<mbox_model_command>(type);
        break;

    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }
    if (!ptr)
        return make_stdlib_error(std::errc::not_enough_memory);

    ICY_ERROR(ptr->m_model.initialize());
    model = std::move(ptr);
    return {};
}

static error_type mbox_context(const guid& index, const mbox::type type) noexcept
{
    xgui_widget menu;
    xgui_widget menu_create;
    xgui_action action_view;
    xgui_action action_edit;
    xgui_action action_delete;
    xgui_action action_rename;
    xgui_action action_create;
    map<mbox::type, xgui_action> actions_create;

    ICY_ERROR(menu.initialize(gui_widget_type::menu, {}));
    ICY_ERROR(menu_create.initialize(gui_widget_type::menu, {}));
    if (type == mbox::type::command || 
        type == mbox::type::character ||
        type == mbox::type::timer)
    {
        ICY_ERROR(action_view.initialize("View"_s));
        ICY_ERROR(action_edit.initialize("Edit"_s));
    }
    else if (type == mbox::type::directory)
    {
        ICY_ERROR(action_edit.initialize("Open"_s));
    }
    
    ICY_ERROR(action_delete.initialize("Delete"_s));
    ICY_ERROR(action_rename.initialize("Rename"_s));
    ICY_ERROR(action_create.initialize("Create"_s));

    if (type == mbox::type::directory)
    {
        ICY_ERROR(actions_create.insert(mbox::type::directory, xgui_action()));
        ICY_ERROR(actions_create.insert(mbox::type::command, xgui_action()));
        ICY_ERROR(actions_create.insert(mbox::type::group, xgui_action()));
        ICY_ERROR(actions_create.insert(mbox::type::character, xgui_action()));
        for (auto&& pair : actions_create)
        {
            ICY_ERROR(pair.value.initialize(to_string(pair.key)));
            ICY_ERROR(menu_create.insert(pair.value));
        }
    }

    if (action_view.index)
        ICY_ERROR(menu.insert(action_view));

    if (action_edit.index)
        ICY_ERROR(menu.insert(action_edit));

    ICY_ERROR(menu.insert(action_delete));
    ICY_ERROR(menu.insert(action_rename));
    if (!actions_create.empty())
    {
        ICY_ERROR(menu.insert(action_create));
        ICY_ERROR(action_create.bind(menu_create));        
    }
    if (index == mbox::root)
        ICY_ERROR(action_delete.enable(false));
    
    gui_action action;
    ICY_ERROR(menu.exec(action));

    auto event_type = event_type::none;
    if (action == action_view && action_view.index)
        event_type = mbox_event_type_view;
    else if (action == action_edit && action_edit.index)
        event_type = mbox_event_type_edit;
    else if (action == action_rename)
        event_type = mbox_event_type_rename;
    else if (action == action_delete)
        event_type = mbox_event_type_delete;
    else if (action.index)
    {
        for (auto&& pair : actions_create)
        {
            if (pair.value != action)
                continue;

            mbox_event_data_create event_data;
            event_data.parent = index;
            event_data.type = pair.key;
            ICY_ERROR(event::post(nullptr, mbox_event_type_create, event_data));
            break;
        }
    }
    if (event_type != event_type::none)
    {
        ICY_ERROR(event::post(nullptr, event_type, mbox_event_data_base(index)));
    }
    return {};    
}

error_type mbox_model_explorer::reset(const mbox::library& library, const guid& root, const mbox::type filter) noexcept
{
    ICY_ERROR(m_model.clear());
    m_root = root;
    m_filter = filter;
    m_data.clear();

    array<guid> vec;
    ICY_ERROR(library.children(root, true, vec));

    const auto root_ptr = library.find(root);
    node_type root_node;
    ICY_ERROR(root_node.initialize(*root_ptr));
    ICY_ERROR(m_data.insert(root, std::move(root_node)));
    for (auto&& index : vec)
    {
        const auto ptr = library.find(index);
        ICY_ASSERT(ptr, "INVALID PTR");

        if (m_filter == mbox::type::_total)
            ;
        else if (ptr->type == mbox::type::directory || ptr->type == m_filter)
            ;
        else
            continue;
        
        const auto it = m_data.find(ptr->parent);
        ICY_ASSERT(it != m_data.end(), "CORRUPTED NODE");

        node_type new_node;
        ICY_ERROR(new_node.initialize(*ptr));  
        ICY_ERROR(it->value.nodes.insert(ptr->name, index));
        ICY_ERROR(m_data.insert(index, std::move(new_node)));
    }
        
    ICY_ERROR(m_model.insert_cols(0, 1));
    ICY_ERROR(m_model.insert_rows(0, 1));
    ICY_ERROR(initialize(m_data.find(root)->value, m_model.node(0, 0)));
    return {};
}
error_type mbox_model_explorer::exec(const event event) noexcept 
{
    if (event->type == mbox_event_type_transaction)
    {
        for (auto&& oper : event->data<mbox_event_data_transaction>().oper())
        {
            if (oper.type == mbox::transaction::operation_type::insert)
            {
                if (m_filter == mbox::type::_total)
                    ;
                else if (oper.value.type == mbox::type::directory || oper.value.type == m_filter)
                    ;
                else
                    continue;

                const auto parent = m_data.find(oper.value.parent);
                const auto it = parent->value.nodes.lower_bound(oper.value.name);
                const auto row = std::distance(parent->value.nodes.begin(), it);
                auto parent_node = node(parent->value);

                node_type mbox_node;
                ICY_ERROR(mbox_node.initialize(oper.value));
                ICY_ERROR(parent_node.insert_rows(row, 1));
                auto xgui_node = parent_node.node(row, 0);
                ICY_ERROR(initialize(mbox_node, xgui_node));

                ICY_ERROR(parent->value.nodes.insert(oper.value.name, oper.value.index));
                ICY_ERROR(m_data.insert(oper.value.index, std::move(mbox_node)));
            }
        }
    }
    return {};
}
error_type mbox_model_explorer::context(const gui_variant& var) noexcept
{
    const auto index = var.as_guid();
    const auto it = m_data.find(index);
    if (it == m_data.end())
        return {};

    ICY_ERROR(mbox_context(index, it->value.type));
    return {};
}
xgui_node mbox_model_explorer::node(const node_type& mbox_node) noexcept
{
    if (mbox_node.index == m_root)
        return m_model.node(0, 0);

    const auto it = m_data.find(mbox_node.parent);
    const auto parent_node = node(it->value);
    const auto row = std::distance(it->value.nodes.begin(), it->value.nodes.find(mbox_node.name));
    return parent_node.node(row, 0);
}
error_type mbox_model_explorer::initialize(const node_type& mbox_node, xgui_node gui_node) noexcept
{    
    switch (mbox_node.type)
    {
    case mbox::type::character:
        ICY_ERROR(gui_node.icon(find_image(mbox_image::type_character)));
        break;
    case mbox::type::command:
        ICY_ERROR(gui_node.icon(find_image(mbox_image::type_command)));
        break;
    case mbox::type::group:
        ICY_ERROR(gui_node.icon(find_image(mbox_image::type_group)));
        break;
    case mbox::type::directory:
        ICY_ERROR(gui_node.icon(find_image(mbox_image::type_directory)));
        break;
    }
    ICY_ERROR(gui_node.text(mbox_node.name));
    ICY_ERROR(gui_node.udata(mbox_node.index));
    ICY_ERROR(gui_node.insert_cols(0, 1));
    if (!mbox_node.nodes.empty())
    {
        ICY_ERROR(gui_node.insert_rows(0, mbox_node.nodes.size()));
        auto row = 0_z;
        for (auto&& pair : mbox_node.nodes)
        {
            auto new_node = gui_node.node(row, 0);
            ICY_ERROR(initialize(m_data.find(pair.value)->value, new_node));
            ++row;
        }
    }
    return {};
}

error_type mbox_model_directory::reset(const mbox::library& library, const guid& root, const mbox::type filter) noexcept
{
    ICY_ERROR(m_model.clear());
    m_root = root;
    m_library = &library;
    m_data.clear();
    ICY_ERROR(m_model.insert_cols(0, _column_total));
    ICY_ERROR(m_model.hheader(column_name, "Name"_s));
    ICY_ERROR(m_model.hheader(column_type, "Type"_s));
    ICY_ERROR(m_model.hheader(column_size, "Size"_s));

    array<guid> vec;
    ICY_ERROR(library.children(root, false, vec));
    for (auto&& index : vec)
    {
        const auto& base = *library.find(index);
        node_type new_node;
        ICY_ERROR(new_node.initialize(base, &library));
        ICY_ERROR(m_data.insert(base.name, std::move(new_node)));
    }
    ICY_ERROR(m_model.insert_rows(0, vec.size()));
    auto row = 0_z;
    for (auto&& pair : m_data)
        ICY_ERROR(initialize(row++, pair.key, pair.value));
    
    return {};
}
error_type mbox_model_directory::exec(const event event) noexcept
{
    if (event->type == mbox_event_type_transaction)
    {
        for (auto&& oper : event->data<mbox_event_data_transaction>().oper())
        {
            if (oper.type == mbox::transaction::operation_type::insert)
            {
                if (oper.value.parent == m_root)
                {
                    const auto it = std::lower_bound(m_data.keys().begin(), m_data.keys().end(), oper.value.name);
                    const auto row = std::distance(m_data.keys().begin(), it);
                    ICY_ERROR(m_model.insert_rows(row, 1));
                    node_type new_node;
                    ICY_ERROR(new_node.initialize(oper.value, nullptr));
                    ICY_ERROR(initialize(row, oper.value.name, new_node));
                    ICY_ERROR(m_data.insert(oper.value.name, std::move(new_node)));
                }
                else
                {
                    for (auto&& pair : m_data)
                    {
                        if (oper.value.parent == pair.value.index)
                        {
                            const auto row = std::distance(m_data.keys().data(), &pair.key);
                            pair.value.size += 1;
                            string str_size;
                            ICY_ERROR(to_string(pair.value.size, str_size));
                            ICY_ERROR(m_model.node(row, column_size).text(str_size));
                        }
                    }
                }
            }
            else if (oper.type == mbox::transaction::operation_type::modify)
            {
                auto it = m_data.begin();
                for (; it != m_data.end(); ++it)
                {
                    if (it->value.index == oper.value.index)
                        break;
                }
                if (it != m_data.end())
                {

                }
            }
        }
    }
    return {};
}
error_type mbox_model_directory::context(const gui_variant& var) noexcept
{
    return {};
    //make_stdlib_error(std::errc::function_not_supported);
}
error_type mbox_model_directory::initialize(const size_t row, const string_view name, const node_type& node) noexcept
{
    auto image_type = mbox_image::none;
    switch (node.type)
    {
    case mbox::type::directory:
        image_type = mbox_image::type_directory;
        break;
    case mbox::type::character:
        image_type = mbox_image::type_character;
        break;
    case mbox::type::command:
        image_type = mbox_image::type_command;
        break;
    case mbox::type::group:
        image_type = mbox_image::type_group;
        break;
    }

    string str_size;
    ICY_ERROR(to_string(node.size, str_size));
    ICY_ERROR(m_model.node(row, 0).udata(node.index));
    ICY_ERROR(m_model.node(row, column_name).text(name));
    ICY_ERROR(m_model.node(row, column_type).text(to_string(node.type)));
    ICY_ERROR(m_model.node(row, column_size).text(str_size));    
    ICY_ERROR(m_model.node(row, 0).icon(find_image(image_type)));
    return {};
}

error_type mbox_model_command::reset(const mbox::library& library, const guid& root, const mbox::type filter) noexcept
{
    ICY_ERROR(m_model.clear());
    m_root = root;
    m_library = &library;
    m_data.clear();
    ICY_ERROR(m_model.insert_cols(0, _column_total));
    ICY_ERROR(m_model.hheader(column_type, "Type"_s));
    ICY_ERROR(m_model.hheader(column_value, "Value"_s));
    ICY_ERROR(m_model.hheader(column_args, "Args"_s));

    ICY_ERROR(m_data.assign(library.find(root)->actions));
    ICY_ERROR(m_model.insert_rows(0, m_data.size()));
    auto row = 0_z;
    for (auto&& action : m_data)
        ICY_ERROR(initialize(row++));
    return {};
}
error_type mbox_model_command::exec(const event event) noexcept
{
    return {}; // return make_stdlib_error(std::errc::function_not_supported);
}
error_type mbox_model_command::context(const gui_variant& var) noexcept
{
    auto readonly = false;
    auto row = m_data.size();
    mbox::action action;
    if (var.type() == gui_variant_type::none)
    {
        xgui_widget menu;
        xgui_submenu menu_create;
        xgui_submenu menu_create_group;
        xgui_submenu menu_create_input;
        xgui_submenu menu_create_timer;
        xgui_submenu menu_create_event;
        xgui_submenu menu_create_button;
        xgui_action action_lock;
        xgui_action action_save;

        map<mbox::action_type, xgui_action> actions_create;
        ICY_ERROR(menu.initialize(gui_widget_type::menu, {}));
        ICY_ERROR(menu_create.initialize("New Action"_s));
        ICY_ERROR(menu_create_group.initialize("Group"_s));
        ICY_ERROR(menu_create_input.initialize("Input"_s));
        ICY_ERROR(menu_create_timer.initialize("Timer"_s));
        ICY_ERROR(menu_create_event.initialize("Event"_s));
        ICY_ERROR(action_save.initialize("Save"_s));
        ICY_ERROR(action_lock.initialize(m_type == mbox_model_type::view_command ? "Unlock"_s : "Lock"_s));
        
        ICY_ERROR(menu_create.widget.insert(menu_create_group.action));
        ICY_ERROR(menu_create.widget.insert(menu_create_input.action));
        ICY_ERROR(menu_create.widget.insert(menu_create_timer.action));
        ICY_ERROR(menu_create.widget.insert(menu_create_event.action));

        for (auto k = 1u; k < uint32_t(mbox::action_type::_total); ++k)
        {
            const auto type = mbox::action_type(k);
            xgui_action action;
            ICY_ERROR(action.initialize(mbox::to_string(type)));

            switch (type)
            {
            case mbox::action_type::group_join:             //  group
            case mbox::action_type::group_leave:            //  group
                ICY_ERROR(menu_create_group.widget.insert(action));
                break;

            case mbox::action_type::button_press:
            case mbox::action_type::button_release:
            case mbox::action_type::button_click:
                ICY_ERROR(menu_create_input.widget.insert(action));
                break;

            case mbox::action_type::timer_start:
            case mbox::action_type::timer_stop:
            case mbox::action_type::timer_pause:
            case mbox::action_type::timer_resume:
                ICY_ERROR(menu_create_timer.widget.insert(action));
                break;


            case mbox::action_type::on_button_press:
            case mbox::action_type::on_button_release:
            case mbox::action_type::on_timer:
                ICY_ERROR(menu_create_event.widget.insert(action));
                break;
                
            case mbox::action_type::command_execute:
            case mbox::action_type::command_replace:
                ICY_ERROR(menu_create.widget.insert(action));
                break;
            }

            ICY_ERROR(actions_create.insert(type, std::move(action)));
        }
        ICY_ERROR(menu.insert(action_save));
        ICY_ERROR(menu.insert(action_lock));
        ICY_ERROR(menu.insert(menu_create.action));

        if (m_type == mbox_model_type::view_command)
        {
            ICY_ERROR(menu_create.action.enable(false));
            ICY_ERROR(action_save.enable(false));
        }

        gui_action gui_action;
        ICY_ERROR(menu.exec(gui_action));

        if (gui_action == action_save)
        {
            ICY_ERROR(event::post(nullptr, mbox_event_type_save, mbox_event_data_base(m_root)));
        }
        else if (gui_action == action_lock)
        {
            ICY_ERROR(event::post(nullptr, m_type == mbox_model_type::view_command ?
                mbox_event_type_unlock : mbox_event_type_lock, mbox_event_data_base(m_root)));
        }

        auto type = mbox::action_type::none;
        for (auto&& pair : actions_create)
        {
            if (pair.value == gui_action)
            {
                type = pair.key;
                break;
            }
        }
        switch (type)
        {
        case mbox::action_type::group_join:
            action = mbox::action::create_group_join({});
            break;
        case mbox::action_type::group_leave:
            action = mbox::action::create_group_leave({});
            break;
        case mbox::action_type::button_press:
            action = mbox::action::create_button_press({});
            break;
        case mbox::action_type::button_release:
            action = mbox::action::create_button_release({});
            break;
        case mbox::action_type::button_click:
            action = mbox::action::create_button_click({});
            break;
        case mbox::action_type::timer_start:
            action = mbox::action::create_timer_start({}, 1);
            break;
        case mbox::action_type::timer_stop:
            action = mbox::action::create_timer_stop({});
            break;
        case mbox::action_type::timer_pause:
            action = mbox::action::create_timer_pause({});
            break;
        case mbox::action_type::timer_resume:
            action = mbox::action::create_timer_resume({});
            break;
        case mbox::action_type::command_execute:
            action = mbox::action::create_command_execute({});
            break;
        case mbox::action_type::command_replace:
            action = mbox::action::create_command_replace({});
            break;
        case mbox::action_type::on_button_press:
            action = mbox::action::create_on_button_press({}, {});
            break;
        case mbox::action_type::on_button_release:
            action = mbox::action::create_on_button_release({}, {});
            break;
        }
    }
    else if (var.type() == gui_variant_type::uinteger)
    {
        row = var.as_uinteger();
        if (row >= m_data.size())
            return {};
            
        xgui_widget menu;
        xgui_action view;
        xgui_action edit;
        xgui_action remove;
        xgui_action move_up;
        xgui_action move_down;

        ICY_ERROR(menu.initialize(gui_widget_type::menu, {}));
        ICY_ERROR(view.initialize("View"_s));
        ICY_ERROR(edit.initialize("Edit"_s));
        ICY_ERROR(remove.initialize("Delete"_s));
        ICY_ERROR(move_up.initialize("Move up"_s));
        ICY_ERROR(move_down.initialize("Move down"_s));
        
        ICY_ERROR(menu.insert(view));
        ICY_ERROR(menu.insert(edit));
        ICY_ERROR(menu.insert(remove));
        ICY_ERROR(menu.insert(move_up));
        ICY_ERROR(menu.insert(move_down));

        if (m_type == mbox_model_type::view_command)
        {
            ICY_ERROR(edit.enable(false));
            ICY_ERROR(remove.enable(false));
            ICY_ERROR(move_up.enable(false));
            ICY_ERROR(move_down.enable(false));
        }

        if (row == 0)
            ICY_ERROR(move_up.enable(false));
        if (row + 1 == m_data.size())
            ICY_ERROR(move_down.enable(false));

        gui_action gui_action;
        ICY_ERROR(menu.exec(gui_action));

        if (gui_action == view)
        {
            readonly = true;
            action = m_data[row];
        }
        else if (gui_action == edit)
        {
            action = m_data[row];
        }
        else if (gui_action == remove)
        {
            ICY_ERROR(m_model.remove_rows(row, 1));
            for (auto k = row + 1; k < m_data.size(); ++k)
                m_data[k] = std::move(m_data[k]);
            m_data.pop_back();
        }
        else if (gui_action == move_up)
        {
            std::swap(m_data[row - 1], m_data[row]);
            ICY_ERROR(initialize(row - 1));
            ICY_ERROR(initialize(row - 0));
        }
        else if (gui_action == move_down)
        {
            std::swap(m_data[row + 1], m_data[row]);
            ICY_ERROR(initialize(row + 1));
            ICY_ERROR(initialize(row + 0));
        }
    }

    if (action.type() == mbox::action_type::none)
        return {};

    auto saved = !readonly;
    ICY_ERROR(mbox_form_action::exec(*m_library, m_root, action, saved));
    if (saved && !readonly)
    {
        if (var.type() == gui_variant_type::none)
        {
            ICY_ERROR(m_model.insert_rows(m_data.size(), 1));
            ICY_ERROR(m_data.push_back(action));
        }
        else
        {
            m_data[row] = action;
        }
        ICY_ERROR(initialize(row));
    }
    return {};
}
error_type mbox_model_command::initialize(const size_t row) noexcept
{
    const auto& action = m_data[row];
    auto image_type = mbox_image::none;
    string value;
    string args;

    switch (action.type())
    {
    case mbox::action_type::group_join:
    case mbox::action_type::group_leave:
        image_type = mbox_image::type_group;
        ICY_ERROR(m_library->path(action.group(), value));
        break;

    case mbox::action_type::on_button_press:
    case mbox::action_type::on_button_release:
        image_type = mbox_image::type_event;
        ICY_ERROR(mbox::to_string(action.event_button(), value));
        ICY_ERROR(m_library->path(action.event_command(), args));
        break;

    case mbox::action_type::on_timer:
        image_type = mbox_image::type_event;
        ICY_ERROR(m_library->path(action.event_timer(), value));
        break;

    case mbox::action_type::button_press:
    case mbox::action_type::button_release:
    case mbox::action_type::button_click:
        image_type = mbox_image::type_input;
        ICY_ERROR(mbox::to_string(action.button(), value));
        break;

    case mbox::action_type::timer_start:
        ICY_ERROR(to_string(action.timer().count, args));
        //  fall through
    case mbox::action_type::timer_stop:
    case mbox::action_type::timer_pause:
    case mbox::action_type::timer_resume:
        image_type = mbox_image::type_timer;
        ICY_ERROR(m_library->path(action.timer().timer, value));
        break;

    case mbox::action_type::command_execute:
    {
        image_type = mbox_image::type_command;
        ICY_ERROR(m_library->path(action.execute().command, value));
        const auto execute_type = action.execute().etype;
        ICY_ERROR(to_string(mbox::to_string(execute_type), args));
        if (execute_type != mbox::execute_type::self)
        {
            string str_group;
            ICY_ERROR(m_library->path(action.execute().group, str_group));
            args.appendf(" [%1]"_s, string_view(str_group));
        }
        break;
    }

    case mbox::action_type::command_replace:
        ICY_ERROR(m_library->path(action.replace().source, value));
        ICY_ERROR(m_library->path(action.replace().target, args));
        break;
    }

    ICY_ERROR(m_model.node(row, column_type).icon(find_image(image_type)));
    ICY_ERROR(m_model.node(row, column_type).text(mbox::to_string(action.type())));
    ICY_ERROR(m_model.node(row, column_type).udata(row));
    ICY_ERROR(m_model.node(row, column_value).text(value));
    ICY_ERROR(m_model.node(row, column_args).text(args));
    return {};
}