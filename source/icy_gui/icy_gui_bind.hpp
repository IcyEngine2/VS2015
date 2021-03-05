#pragma once

#include <icy_gui/icy_gui.hpp>

struct gui_bind_data
{
    icy::unique_ptr<icy::gui_bind> func;
    icy::weak_ptr<icy::gui_model> model;
    icy::weak_ptr<icy::gui_window> window;
    icy::gui_node node;
    icy::gui_widget widget;
};