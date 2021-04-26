#pragma once

#include "icy_gui_attr.hpp"
#include <icy_engine/core/icy_color.hpp>

using namespace icy;


gui_widget_attr gui_str_to_attr(const string_view str) noexcept
{
	switch (hash(str))
	{
	case "class"_hash:
	case "Class"_hash:
		return gui_widget_attr::class_;

	case "width"_hash:
	case "Width"_hash:
		return gui_widget_attr::width;

	case "width_min"_hash:
	case "widthMin"_hash:
	case "WidthMin"_hash:
	case "min_width"_hash:
	case "minWidth"_hash:
	case "MinWidth"_hash:
		return gui_widget_attr::width_min;


	case "width_max"_hash:
	case "widthMax"_hash:
	case "WidthMax"_hash:
	case "max_width"_hash:
	case "maxWidth"_hash:
	case "MaxWidth"_hash:
		return gui_widget_attr::width_max;

	case "height"_hash:
	case "Height"_hash:
		return gui_widget_attr::height;

	case "height_min"_hash:
	case "heightMin"_hash:
	case "HeightMin"_hash:
	case "min_height"_hash:
	case "minHeight"_hash:
	case "MinHeight"_hash:
		return gui_widget_attr::height_min;

	case "heigth_max"_hash:
	case "heigthMax"_hash:
	case "HeigthMax"_hash:
	case "max_height"_hash:
	case "maxHeight"_hash:
	case "MaxHeight"_hash:
		return gui_widget_attr::height_max;

	case "color"_hash:
	case "colour"_hash:
	case "Color"_hash:
	case "Colour"_hash:
		return gui_widget_attr::color;


	case "bkcolor"_hash:
	case "bkcolour"_hash:
	case "bkColor"_hash:
	case "bkColour"_hash:
	case "BkColor"_hash:
	case "BkColour"_hash:
	case "backColor"_hash:
	case "backColour"_hash:
	case "BackColor"_hash:
	case "BackColour"_hash:
		return gui_widget_attr::bkcolor;

	case "weight"_hash:
	case "Weight"_hash:
		return gui_widget_attr::weight;

	case "weight_x"_hash:
	case "weightX"_hash:
	case "WeightX"_hash:
		return gui_widget_attr::weight_x;

	case "weight_y"_hash:
	case "weightY"_hash:
	case "WeightY"_hash:
		return gui_widget_attr::weight_y;

	case "flex_scroll"_hash:
	case "flexScroll"_hash:
	case "FlexScroll"_hash:
		return gui_widget_attr::flex_scroll;

	case "splitter_size"_hash:
	case "splitterSize"_hash:
	case "SplitterSize"_hash:
	case "size_splitter"_hash:
	case "sizeSplitter"_hash:
	case "SizeSplitter"_hash:
		return gui_widget_attr::splitter_size;

	//case "items"_hash:
	//case "Items"_hash:
	//	return gui_widget_attr::max_items;
	}

	return gui_widget_attr::none;
}
gui_widget_type gui_str_to_type(const string_view str) noexcept
{
	switch (hash(str))
	{
	case "window_main"_hash:
	case "windowMain"_hash:
	case "WindowMain"_hash:
	case "main_window"_hash:
	case "mainWindow"_hash:
	case "MainWindow"_hash:
		return gui_widget_type::window_main;

	case "window_popup"_hash:
	case "windowPopup"_hash:
	case "WindowPopup"_hash:
	case "popup_window"_hash:
	case "popupWindow"_hash:
	case "PopupWindow"_hash:
		return gui_widget_type::window_popup;

	case "menu_main"_hash:
	case "menuMain"_hash:
	case "MenuMain"_hash:
	case "main_menu"_hash:
	case "mainMenu"_hash:
	case "MainMenu"_hash:
		return gui_widget_type::menu_main;

	case "menu_popup"_hash:
	case "menuPopup"_hash:
	case "MenuPopup"_hash:
	case "popup_menu"_hash:
	case "popupMenu"_hash:
	case "PopupMenu"_hash:
		return gui_widget_type::menu_popup;

	case "view_combo"_hash:
	case "viewCombo"_hash:
	case "ViewCombo"_hash:
	case "combo_view"_hash:
	case "comboView"_hash:
	case "ComboView"_hash:
		return gui_widget_type::view_combo;

	case "view_list"_hash:
	case "viewList"_hash:
	case "ViewList"_hash:
	case "list_view"_hash:
	case "listView"_hash:
	case "ListView"_hash:
		return gui_widget_type::view_list;

	case "view_tree"_hash:
	case "viewTree"_hash:
	case "ViewTree"_hash:
	case "tree_view"_hash:
	case "treeView"_hash:
	case "TreeView"_hash:
		return gui_widget_type::view_tree;

	case "view_table"_hash:
	case "viewTable"_hash:
	case "ViewTable"_hash:
	case "table_view"_hash:
	case "tableView"_hash:
	case "TableView"_hash:
		return gui_widget_type::view_table;

	case "edit_line"_hash:
	case "editLine"_hash:
	case "EditLine"_hash:
	case "line_edit"_hash:
	case "lineEdit"_hash:
	case "LineEdit"_hash:
		return gui_widget_type::edit_line;

	case "edit_text"_hash:
	case "editText"_hash:
	case "EditText"_hash:
	case "text_edit"_hash:
	case "textEdit"_hash:
	case "TextEdit"_hash:
		return gui_widget_type::edit_text;

	case "splitter"_hash:
	case "Splitter"_hash:
		return gui_widget_type::splitter;
	}

	return gui_widget_type::none;
}
gui_widget_layout gui_str_to_layout(const string_view str) noexcept
{
	switch (hash(str))
	{
	case "vbox"_hash:
	case "vBox"_hash:
	case "VBox"_hash:
		return gui_widget_layout::vbox;

	case "hbox"_hash:
	case "hBox"_hash:
	case "HBox"_hash:
		return gui_widget_layout::hbox;

	case "grid"_hash:
	case "Grid"_hash:
		return gui_widget_layout::grid;

	case "flex"_hash:
	case "Flex"_hash:
		return gui_widget_layout::flex;
	}

	return gui_widget_layout::none;
}
gui_variant gui_attr_default(const gui_widget_attr attr) noexcept
{
	switch (attr)
	{
	case gui_widget_attr::color:
		return color(colors::white);
	case gui_widget_attr::bkcolor:
		return color(colors::black);
	case gui_widget_attr::flex_scroll:
		return false;
	case gui_widget_attr::splitter_size:
		return 5.0f;
	default:
		return gui_variant();
	}
}