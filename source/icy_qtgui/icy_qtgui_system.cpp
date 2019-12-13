#include <icy_qtgui/icy_qtgui.hpp>
#include <icy_engine/core/icy_new.hpp>
#include <QtCore/qtimer.h>
#include <QtCore/qmutex.h>
#include <QtCore/qthread.h>
#include <QtCore/qplugin.h>
#include <QtGui/qevent.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qaction.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qmainwindow.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qgridlayout.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qsplitter.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qabstractitemview.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qtreeview.h>
#include <QtWidgets/qtableview.h>
#if _DEBUG
#pragma comment(lib, "Qt5/Qt5Cored")
#pragma comment(lib, "Qt5/Qt5Guid")
#pragma comment(lib, "Qt5/Qt5Widgetsd")
#pragma comment(lib, "Qt5/Qt5EventDispatcherSupportd")
#pragma comment(lib, "Qt5/Qt5FontDatabaseSupportd")
#pragma comment(lib, "Qt5/Qt5WindowsUIAutomationSupportd")
#pragma comment(lib, "Qt5/Qt5ThemeSupportd")
#pragma comment(lib, "Qt5/qwindowsd")
#pragma comment(lib, "icy_engine_cored")
#else
#pragma comment(lib, "Qt5/Qt5Core")
#pragma comment(lib, "Qt5/Qt5Gui")
#pragma comment(lib, "Qt5/Qt5Widgets")
#pragma comment(lib, "Qt5/Qt5EventDispatcherSupport")
#pragma comment(lib, "Qt5/Qt5FontDatabaseSupport")
#pragma comment(lib, "Qt5/Qt5WindowsUIAutomationSupport")
#pragma comment(lib, "Qt5/Qt5ThemeSupport")
#pragma comment(lib, "Qt5/qwindows")
#pragma comment(lib, "icy_engine_core")
#endif
#if QT_STATIC
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#endif
#pragma comment(lib, "user32")
#pragma comment(lib, "Advapi32")
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "Shell32")
#pragma comment(lib, "Ole32")
#pragma comment(lib, "Version")
#pragma comment(lib, "Netapi32")
#pragma comment(lib, "Userenv")
#pragma comment(lib, "Winmm")
#pragma comment(lib, "Imm32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "Wtsapi32")
#pragma comment(lib, "Dwmapi")


using namespace icy;

extern QAbstractItemModel* qtgui_model_create(QObject& root, gui_model_base& base);
extern uint32_t qtgui_model_bind(const QModelIndex& index, const gui_widget& widget);
extern QVariant qtgui_make_variant(const gui_variant& var);
extern std::errc qtgui_make_variant(const QVariant& qvar, gui_variant& var);

ICY_STATIC_NAMESPACE_BEG
static const auto qtgui_property_name = "user";
struct qtgui_event_node
{
    void* unused = nullptr;
    event_type type = event_type::none; 
    gui_event args;
};
QLayout* make_layout(const gui_layout layout, QWidget& parent)
{
    switch (layout)
    {
    case gui_layout::grid:
        return new QGridLayout(&parent);
    case gui_layout::hbox:
        return new QHBoxLayout(&parent);
    case gui_layout::vbox:
        return new QVBoxLayout(&parent);
    default:
        return nullptr;
    }
}
QString make_string(const string_view str)
{
    return QString::fromUtf8(str.bytes().data(), static_cast<int>(str.bytes().size()));
}
enum class qtgui_event_type : uint32_t
{
    none,
    create_widget,
    create_action,
    insert_widget,
    insert_action,
    show,
    text,
    bind_action,
    bind_widget,
};
class qtgui_event_create_widget : public QEvent
{
public:
    qtgui_event_create_widget(const gui_widget widget, const gui_widget_type wtype, const gui_layout layout, const gui_widget parent) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + static_cast<uint32_t>(qtgui_event_type::create_widget))),
        wtype(wtype), layout(layout), parent(parent), widget(widget)
    {

    }
public:
    const gui_widget_type wtype;
    const gui_layout layout;
    const gui_widget parent;
    const gui_widget widget;
};
class qtgui_event_create_action : public QEvent
{
public:
    qtgui_event_create_action(const gui_action action, const QString text) : 
        QEvent(static_cast<QEvent::Type>(QEvent::User + static_cast<uint32_t>(qtgui_event_type::create_action))), 
        action(action), text(text)
    {

    }
public:
    const gui_action action;
    const QString text;
};
class qtgui_event_insert_widget : public QEvent
{
public:
    qtgui_event_insert_widget(const gui_widget widget, const uint32_t x, const uint32_t y, const uint32_t dx, const uint32_t dy) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + static_cast<uint32_t>(qtgui_event_type::insert_widget))),
        widget(widget), x(x), y(y), dx(dx), dy(dy)
    {

    }
public:
    const gui_widget widget;
    const uint32_t x;
    const uint32_t y;
    const uint32_t dx;
    const uint32_t dy;
};
class qtgui_event_insert_action : public QEvent
{
public:
    qtgui_event_insert_action(const gui_widget widget, const gui_action action) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + static_cast<uint32_t>(qtgui_event_type::insert_action))), 
        widget(widget), action(action)
    {

    }
public:
    const gui_widget widget;
    const gui_action action;
};
class qtgui_event_show : public QEvent
{
public:
    qtgui_event_show(const gui_widget widget, const bool value) : 
        QEvent(static_cast<QEvent::Type>(QEvent::User + static_cast<uint32_t>(qtgui_event_type::show))), 
        widget(widget), value(value)
    {

    }
public:
    const gui_widget widget;
    const bool value;
};
class qtgui_event_text : public QEvent
{
public:
    qtgui_event_text(const gui_widget widget, const QString text) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + static_cast<uint32_t>(qtgui_event_type::text))),
        widget(widget), text(text)
    {

    }
public:
    const gui_widget widget;
    const QString text;
};
class qtgui_event_bind_action : public QEvent
{
public:
    qtgui_event_bind_action(const gui_action action, const gui_widget widget) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + static_cast<uint32_t>(qtgui_event_type::bind_action))),
        action(action), widget(widget)
    {

    }
public:
    const gui_action action;
    const gui_widget widget;
};
class qtgui_event_bind_widget : public QEvent
{
public:
    qtgui_event_bind_widget(const gui_widget widget, const QModelIndex& index) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + static_cast<uint32_t>(qtgui_event_type::bind_widget))),
        widget(widget), index(index)
    {

    }
public:
    const gui_widget widget;
    const QModelIndex index;
};
class qtgui_splitter : public QSplitter
{
    using QSplitter::QSplitter;
};
class qtgui_system : public gui_system, public QApplication
{
public:
    qtgui_system() : QApplication(__argc, __argv)
    {
        m_widgets.push_back(nullptr);
        m_actions.push_back(nullptr);
    }
private:
    void release() noexcept override
    {
        if (const auto next = static_cast<qtgui_event_node*>(m_queue.pop()))
            delete next;
        delete this;
        detail::global_heap = {};
    }
    bool notify(QObject* object, QEvent* event) noexcept override
    {
        if (event->type() == QEvent::Type::Close && qobject_cast<QMainWindow*>(object))
        {
            event->ignore();
            auto node = new qtgui_event_node;
            node->type = event_type::window_close;
            node->args.widget.index = object->property(qtgui_property_name).toULongLong();
            m_queue.push(node);
            qApp->exit();
            return false;
        }
        else if (event->type() == QEvent::Type::Quit)
        {
            qApp->exit();
            return false;
        }
        else if (object == this && event->type() >= QEvent::Type::User && event->type() < QEvent::Type::MaxUser)
        {
            auto error = static_cast<std::errc>(0);
            const auto type = static_cast<qtgui_event_type>(event->type() - QEvent::Type::User);
            QMutexLocker lk(&m_lock);
            if (type == qtgui_event_type::create_widget)
            {
                error = process(*static_cast<qtgui_event_create_widget*>(event));
            }
            else if (type == qtgui_event_type::create_action)
            {
                error = process(*static_cast<qtgui_event_create_action*>(event));
            }
            else if (type == qtgui_event_type::insert_widget)
            {
                error = process(*static_cast<qtgui_event_insert_widget*>(event));
            }
            else if (type == qtgui_event_type::insert_action)
            {
                error = process(*static_cast<qtgui_event_insert_action*>(event));
            }
            else if (type == qtgui_event_type::show)
            {
                error = process(*static_cast<qtgui_event_show*>(event));                
            }
            else if (type == qtgui_event_type::text)
            {
                error = process(*static_cast<qtgui_event_text*>(event));
            }
            else if (type == qtgui_event_type::bind_action)
            {
                error = process(*static_cast<qtgui_event_bind_action*>(event));
            }
            else if (type == qtgui_event_type::bind_widget)
            {
                error = process(*static_cast<qtgui_event_bind_widget*>(event));
            }
            else
            {
                error = std::errc::function_not_supported;
            }

            if (const auto cerr = static_cast<int>(error))
                qApp->exit(cerr);
        }
        return QApplication::notify(object, event);
    }
    uint32_t wake() noexcept override
    {
        try
        {
            auto event = new QEvent(QEvent::Type::Quit);
            qApp->postEvent(this, event);
        }
        ICY_CATCH;
        return {};
    }
    uint32_t loop(event_type& type, gui_event& args) noexcept override
	{
        type = event_type::none;
        try
        {
            auto next = static_cast<qtgui_event_node*>(m_queue.pop());
            if (!next)
            {
                if (const auto error = qApp->exec())
                    return error;
                next = static_cast<qtgui_event_node*>(m_queue.pop());
            }
            if (next)
            {
                type = next->type;
                args = next->args;
                delete next;
            }
        }
        ICY_CATCH;
		return {};
	}
    uint32_t create(gui_widget& widget, const gui_widget_type type, const gui_layout layout, const gui_widget parent) noexcept override
    {
        try
        {
            {
                QMutexLocker lk(&m_lock);
                m_widgets.push_back(nullptr);
                widget.index = static_cast<uint64_t>(m_widgets.size() - 1);
            }
            const auto event = new qtgui_event_create_widget(widget, type, layout, parent);
            qApp->postEvent(this, event);
        }
        ICY_CATCH;
        return {};
    }
    uint32_t create(gui_action& action, const string_view text) noexcept override
    {
        try
        {
            {
                QMutexLocker lk(&m_lock);
                m_actions.push_back(nullptr);
                action.index = static_cast<uint64_t>(m_actions.size() - 1);
            }
            const auto event = new qtgui_event_create_action(action, make_string(text));
            qApp->postEvent(this, event);
            return {};
        }
        ICY_CATCH;
    }
    uint32_t initialize(gui_model_view& view, gui_model_base& base) noexcept override
    {
        try
        {
            auto model = qtgui_model_create(m_root, base);
            model->moveToThread(qApp->thread());
            view.ptr = model;
        }
        ICY_CATCH;
        return {};
    }
    uint32_t insert(const gui_widget widget, const uint32_t x, const uint32_t y, const uint32_t dx, const uint32_t dy) noexcept override
    {
        try
        {
            const auto event = new qtgui_event_insert_widget(widget, x, y, dx, dy);
            qApp->postEvent(this, event);
        }
        ICY_CATCH;
        return {};
    }
    uint32_t insert(const gui_widget widget, const gui_action action) noexcept override
    {
        try
        {
            const auto event = new qtgui_event_insert_action(widget, action);
            qApp->postEvent(this, event);
        }
        ICY_CATCH;
        return {};
    }
    uint32_t show(const gui_widget widget, const bool value) noexcept override
    {
        try
        {
            const auto event = new qtgui_event_show(widget, value);
            qApp->postEvent(this, event);
        }
        ICY_CATCH;
        return {};
    }
    uint32_t text(const gui_widget widget, const string_view text) noexcept override
    {
        try
        {
            const auto event = new qtgui_event_text(widget, make_string(text));
            qApp->postEvent(this, event);
        }
        ICY_CATCH;
        return {};
    }
    uint32_t bind(const gui_action action, const gui_widget widget) noexcept override
    {
        try
        {
            const auto event = new qtgui_event_bind_action(action, widget);
            qApp->postEvent(this, event);
        }
        ICY_CATCH;
        return {};
    }
private:
    std::errc process(const qtgui_event_create_widget& event)
    {
        if (event.widget.index >= m_widgets.size())
            return std::errc::invalid_argument;
        
        if (event.parent.index >= m_widgets.size())
            return std::errc::invalid_argument;

        auto parent = event.parent.index ? m_widgets[event.parent.index] : &m_root;
        if (const auto parent_window = qobject_cast<QMainWindow*>(parent))
            parent = parent_window->centralWidget();
        
        QWidget* widget = nullptr;
        switch (event.wtype)
        {
        case gui_widget_type::window:
        {
            const auto new_window = new QMainWindow(parent);
            new_window->setCentralWidget(new QWidget(new_window));
            make_layout(event.layout, *new_window->centralWidget());
            widget = new_window;
            break;
        }

        case gui_widget_type::none:
            widget = new QWidget(parent);
            make_layout(event.layout, *widget);
            break;

        case gui_widget_type::message:
            widget = new QMessageBox(parent);
            break;

        case gui_widget_type::menu:
            widget = new QMenu(parent);
            break;

        case gui_widget_type::line_edit:
            widget = new QLineEdit(parent);
            break;

        case gui_widget_type::vsplitter:
            widget = new qtgui_splitter(Qt::Vertical, parent);
            break;

        case gui_widget_type::hsplitter:
            widget = new qtgui_splitter(Qt::Horizontal, parent);
            break;

        case gui_widget_type::combo_box:
            widget = new QComboBox(parent);
            break;

        case gui_widget_type::list_view:
            widget = new QListView(parent);
            break;

        case gui_widget_type::tree_view:
            widget = new QTreeView(parent);
            break;

        case gui_widget_type::grid_view:
            widget = new QTableView(parent);
            break;

        default:
            return std::errc::function_not_supported;
        }
        const auto index = event.widget.index;
        widget->setProperty(qtgui_property_name, index);


        const auto update_func = [this, index](const QVariant& var)
        {
            auto node = new qtgui_event_node;
            node->type = event_type::gui_update;
            node->args.widget.index = index;
            const auto cerr = qtgui_make_variant(var, node->args.data);
            m_queue.push(node);
            qApp->exit(static_cast<int>(cerr));
        };

        if (const auto line_edit = qobject_cast<QLineEdit*>(widget))
        {
            QObject::connect(line_edit, &QLineEdit::editingFinished, 
                [update_func, line_edit]() { update_func(line_edit->text()); });
        }
        else if (const auto combo_box = qobject_cast<QComboBox*>(widget))
        {
            QObject::connect(combo_box, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                [update_func](const int index) { update_func(index); });
        }

        m_widgets[event.widget.index] = widget;
        return {};
    }
    std::errc process(const qtgui_event_create_action& event)
    {
        if (event.action.index >= m_actions.size())
            return std::errc::invalid_argument;
        
        QAction* action = new QAction(&m_root);
        action->setProperty(qtgui_property_name, event.action.index);
        action->setText(event.text);
        m_actions[event.action.index] = action;
        const auto actionIndex = event.action.index;
        QObject::connect(action, &QAction::triggered, [this, actionIndex]
        {
            auto node = new qtgui_event_node;
            node->type = event_type::gui_action;
            node->args.data = actionIndex;
            m_queue.push(node);
            qApp->exit();
        });
        return {};
    }
    std::errc process(const qtgui_event_insert_widget& event)
    {
        if (event.widget.index >= m_widgets.size())
            return std::errc::invalid_argument;

        const auto widget = m_widgets[event.widget.index];
        if (!widget)
            return std::errc::invalid_argument;

        const auto parent = widget->parentWidget();
        if (!parent || !parent->layout())
            return std::errc::invalid_argument;
               
        if (const auto grid = qobject_cast<QGridLayout*>(parent->layout()))
        {
            grid->addWidget(widget, event.y, event.x, event.dy, event.dx);
        }
        else if (const auto box = qobject_cast<QBoxLayout*>(parent->layout()))
        {
            box->addWidget(widget);
        }
        else
        {
            return std::errc::invalid_argument;
        }
        return {};
    }
    std::errc process(const qtgui_event_insert_action& event)
    {
        if (event.widget.index >= m_widgets.size() ||
            event.action.index >= m_actions.size())
            return std::errc::invalid_argument;
        
        const auto widget = m_widgets[event.widget.index];
        const auto action = m_actions[event.action.index];

        if (!widget || !action)
            return std::errc::invalid_argument;

        if (const auto message = qobject_cast<QMessageBox*>(widget))
        {
            const auto count = message->buttons().size();
            auto role = static_cast<QMessageBox::ButtonRole>(message->buttons().size());

            const auto button = new QPushButton(message);
            button->setText(action->text());
            QObject::connect(button, &QPushButton::clicked, action, &QAction::trigger);
            message->addButton(button, role);
        }
        else if (const auto menu = qobject_cast<QMenu*>(widget))
        {
            menu->addAction(action);         
        }
        return {};
    }
    std::errc process(const qtgui_event_show& event)
    {
        if (event.widget.index >= m_widgets.size())
            return std::errc::invalid_argument;

        const auto widget = m_widgets[event.widget.index];
        if (!widget)
            return std::errc::invalid_argument;

        if (const auto menu = qobject_cast<QMenu*>(widget))
            menu->move(QCursor::pos());
        
        widget->setVisible(event.value);
        return {};
    }
    std::errc process(const qtgui_event_text& event)
    {
        if (event.widget.index >= m_widgets.size())
            return std::errc::invalid_argument;

        const auto widget = m_widgets[event.widget.index];
        if (!widget)
            return std::errc::invalid_argument;

        if (const auto message = qobject_cast<QMessageBox*>(widget))
        {
            message->setText(event.text);
        }
        else
        {
            widget->setWindowTitle(event.text);
        }
        return {};
    }
    std::errc process(const qtgui_event_bind_action& event)
    {
        if (event.widget.index >= m_widgets.size() ||
            event.action.index >= m_actions.size())
            return std::errc::invalid_argument;

        const auto widget = m_widgets[event.widget.index];
        const auto action = m_actions[event.action.index];

        if (!widget || !action)
            return std::errc::invalid_argument;

        if (const auto menu = qobject_cast<QMenu*>(widget))
            action->setMenu(menu);
        else
            return std::errc::invalid_argument;
        return {};
    }
    std::errc process(const qtgui_event_bind_widget& event)
    {
        if (event.widget.index >= m_widgets.size())
            return std::errc::invalid_argument;

        const auto widget = m_widgets[event.widget.index];
        if (!widget)
            return std::errc::invalid_argument;

        const auto model = const_cast<QAbstractItemModel*>(event.index.model());
        if (const auto view = qobject_cast<QAbstractItemView*>(widget))
        {
            view->setModel(model);
            view->setRootIndex(event.index.internalId() ? event.index : QModelIndex());
        }
        else if (const auto combo = qobject_cast<QComboBox*>(widget))
        {
            combo->setModel(model);
            combo->setRootModelIndex(event.index.internalId() ? event.index : QModelIndex());
        }
        else
            return std::errc::invalid_argument;
        return {};
    }
private:
    detail::intrusive_mpsc_queue m_queue;
    QWidget m_root;
    QMutex m_lock;
    QList<QWidget*> m_widgets;
    QList<QAction*> m_actions;
};
ICY_STATIC_NAMESPACE_END

uint32_t qtgui_model_bind(const QModelIndex& index, const gui_widget& widget)
{
    try
    {
        const auto event = new qtgui_event_bind_widget(widget, index);
        qApp->sendEvent(qApp, event);
        return {};
    }
    ICY_CATCH;
}
static uint32_t icy_gui_system_create_noexcept(gui_system** system) noexcept
{
   
}

#undef ICY_GUI_ERROR
#define ICY_GUI_ERROR(X) if (const auto error = (X)){ if (error.source == error_source_stdlib) return error.code; else return 0xFFFF'FFFFui32; }

uint32_t ICY_QTGUI_API icy_gui_system_create(const int version, const global_heap_type heap, gui_system** system) noexcept
{
    if (!system || *system || !heap.realloc || version != ICY_GUI_VERSION)
        return static_cast<uint32_t>(std::errc::invalid_argument);

    detail::global_heap = heap;
    try
    {
        *system = new qtgui_system;
        return 0;
    }
    ICY_CATCH;
}