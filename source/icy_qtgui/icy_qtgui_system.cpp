#include <icy_qtgui/icy_qtgui.hpp>
#include <QtCore/qtimer.h>
#include <QtCore/qmutex.h>
#include <QtCore/qthread.h>
#include <QtCore/qplugin.h>
#include <QtCore/qthreadpool.h>
#include <QtGui/qevent.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qstylefactory.h>
#include <QtWidgets/qaction.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qmenubar.h>
#include <QtWidgets/qmainwindow.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qgridlayout.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qtextedit.h>
#include <QtWidgets/qsplitter.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qabstractitemview.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qtreeview.h>
#include <QtWidgets/qtableview.h>
#include <QtWidgets/qprogressbar.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qtoolbutton.h>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/qinputdialog.h>
#include <QtWidgets/qcolordialog.h>
#include <QtWidgets/qfontdialog.h>
#if _DEBUG
#pragma comment(lib, "Qt5/Qt5Cored")
#pragma comment(lib, "Qt5/Qt5Guid")
#pragma comment(lib, "Qt5/Qt5Widgetsd")
#pragma comment(lib, "Qt5/Qt5EventDispatcherSupportd")
#pragma comment(lib, "Qt5/Qt5FontDatabaseSupportd")
#pragma comment(lib, "Qt5/Qt5WindowsUIAutomationSupportd")
#pragma comment(lib, "Qt5/Qt5ThemeSupportd")
#pragma comment(lib, "Qt5/qwindowsd")
#pragma comment(lib, "Qt5/qtlibpngd")
#pragma comment(lib, "Qt5/qtfreetyped")
#else
#pragma comment(lib, "Qt5/Qt5Core")
#pragma comment(lib, "Qt5/Qt5Gui")
#pragma comment(lib, "Qt5/Qt5Widgets")
#pragma comment(lib, "Qt5/Qt5EventDispatcherSupport")
#pragma comment(lib, "Qt5/Qt5FontDatabaseSupport")
#pragma comment(lib, "Qt5/Qt5WindowsUIAutomationSupport")
#pragma comment(lib, "Qt5/Qt5ThemeSupport")
#pragma comment(lib, "Qt5/qwindows")
#pragma comment(lib, "Qt5/qtlibpng")
#pragma comment(lib, "Qt5/qtfreetype")
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
static auto qtgui_exit_code = std::errc(0);
extern void qtgui_exit(const std::errc code = std::errc(0))
{
    if (qtgui_exit_code == std::errc(0))
        qtgui_exit_code = code;
    qApp->exit(1);
}

ICY_STATIC_NAMESPACE_BEG
static auto qtapp_argc = 0;
static char** qtapp_argv = nullptr;
static const auto qtgui_property_name = "user";
struct qtgui_event_node
{
    void* unused = nullptr;
    event_type type = event_type::none; 
    gui_event args;
};
QLayout* make_layout(const gui_widget_flag layout, QWidget& parent)
{
    switch (layout)
    {
    case gui_widget_flag::layout_grid:
        return new QGridLayout(&parent);
    case gui_widget_flag::layout_hbox:
        return new QHBoxLayout(&parent);
    case gui_widget_flag::layout_vbox:
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
    enable_action,
};
class qtgui_event_create_widget : public QEvent
{
public:
    qtgui_event_create_widget(const gui_widget widget, const gui_widget_type wtype, const gui_widget parent, const gui_widget_flag flags) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + static_cast<uint32_t>(qtgui_event_type::create_widget))),
        wtype(wtype), flags(flags), parent(parent), widget(widget)
    {

    }
public:
    const gui_widget_type wtype;
    const gui_widget_flag flags;
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
class qtgui_event_enable_action : public QEvent
{
public:
    qtgui_event_enable_action(const gui_action action, const bool value) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + static_cast<uint32_t>(qtgui_event_type::enable_action))),
        action(action), value(value)
    {

    }
public:
    const gui_action action;
    const bool value;
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
    qtgui_system();   
private:
    void release() noexcept override;
    bool notify(QObject* object, QEvent* event) noexcept override;
    uint32_t wake() noexcept override;
    uint32_t loop(event_type& type, gui_event& args) noexcept override;
    uint32_t create(gui_widget& widget, const gui_widget_type type, const gui_widget parent, const gui_widget_flag flags) noexcept override;
    uint32_t create(gui_action& action, const string_view text) noexcept override;
    uint32_t initialize(gui_model_view& view, gui_model_base& base) noexcept override;
    uint32_t insert(const gui_widget widget, const uint32_t x, const uint32_t y, const uint32_t dx, const uint32_t dy) noexcept override;
    uint32_t insert(const gui_widget widget, const gui_action action) noexcept override;
    uint32_t show(const gui_widget widget, const bool value) noexcept override;
    uint32_t text(const gui_widget widget, const string_view text) noexcept override;
    uint32_t bind(const gui_action action, const gui_widget widget) noexcept override;
    uint32_t enable(const gui_action action, const bool value) noexcept override;
private:
    std::errc process(const qtgui_event_create_widget& event);
    std::errc process(const qtgui_event_create_action& event);
    std::errc process(const qtgui_event_insert_widget& event);
    std::errc process(const qtgui_event_insert_action& event);
    std::errc process(const qtgui_event_show& event);
    std::errc process(const qtgui_event_text& event);
    std::errc process(const qtgui_event_bind_action& event);
    std::errc process(const qtgui_event_bind_widget& event);
    std::errc process(const qtgui_event_enable_action& event);
private:
    detail::intrusive_mpsc_queue m_queue;
    QWidget m_root;
    QMutex m_lock;
    QList<QWidget*> m_widgets;
    QList<QAction*> m_actions;
};
ICY_STATIC_NAMESPACE_END

qtgui_system::qtgui_system() : QApplication(qtapp_argc, qtapp_argv)
{
    m_widgets.push_back(nullptr);
    m_actions.push_back(nullptr);
    const auto style = QStyleFactory::create("Fusion");
    QApplication::setStyle(style);
}
void qtgui_system::release() noexcept
{
    if (const auto next = static_cast<qtgui_event_node*>(m_queue.pop()))
        delete next;
    delete this;
}
bool qtgui_system::notify(QObject* object, QEvent* event) noexcept
{
    const auto func = [this, object, event](bool& output) noexcept -> uint32_t
    {
        try
        {
            if (event->type() == QEvent::Type::Close && qobject_cast<QMainWindow*>(object))
            {
                event->ignore();
                auto node = new qtgui_event_node;
                node->type = event_type::window_close;
                node->args.widget.index = object->property(qtgui_property_name).toULongLong();
                m_queue.push(node);
                qtgui_exit();
                return {};
            }
            else if (event->type() == QEvent::Type::Quit)
            {
                qtgui_exit();
                return {};
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
                else if (type == qtgui_event_type::enable_action)
                {
                    error = process(*static_cast<qtgui_event_enable_action*>(event));
                }
                else
                {
                    error = std::errc::function_not_supported;
                }
                output = true;
                return uint32_t(error);
            }
            else
            {
                output = QApplication::notify(object, event);
                return 0;
            }
        }
        ICY_CATCH
    };
    auto output = false;
    if (const auto error = func(output))
        qtgui_exit(std::errc(error));
    return output;
}
uint32_t qtgui_system::wake() noexcept
{
    try
    {
        auto event = new QEvent(QEvent::Type::Quit);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::loop(event_type& type, gui_event& args) noexcept
{
    type = event_type::none;
    try
    {
        auto next = static_cast<qtgui_event_node*>(m_queue.pop());
        if (!next)
        {
            if (const auto error = qApp->exec())
                return uint32_t(qtgui_exit_code);
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
uint32_t qtgui_system::create(gui_widget& widget, const gui_widget_type type, const gui_widget parent, const gui_widget_flag flags) noexcept
{
    try
    {
        {
            QMutexLocker lk(&m_lock);
            m_widgets.push_back(nullptr);
            widget.index = static_cast<uint64_t>(m_widgets.size() - 1);
        }
        const auto event = new qtgui_event_create_widget(widget, type, parent, flags);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::create(gui_action& action, const string_view text) noexcept
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
uint32_t qtgui_system::initialize(gui_model_view& view, gui_model_base& base) noexcept
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
uint32_t qtgui_system::insert(const gui_widget widget, const uint32_t x, const uint32_t y, const uint32_t dx, const uint32_t dy) noexcept
{
    try
    {
        const auto event = new qtgui_event_insert_widget(widget, x, y, dx, dy);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::insert(const gui_widget widget, const gui_action action) noexcept
{
    try
    {
        const auto event = new qtgui_event_insert_action(widget, action);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::show(const gui_widget widget, const bool value) noexcept
{
    try
    {
        const auto event = new qtgui_event_show(widget, value);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::text(const gui_widget widget, const string_view text) noexcept
{
    try
    {
        const auto event = new qtgui_event_text(widget, make_string(text));
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::bind(const gui_action action, const gui_widget widget) noexcept
{
    try
    {
        const auto event = new qtgui_event_bind_action(action, widget);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::enable(const gui_action action, const bool value) noexcept
{
    try
    {
        const auto event = new qtgui_event_enable_action(action, value);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
std::errc qtgui_system::process(const qtgui_event_create_widget& event)
{
    if (event.widget.index >= m_widgets.size())
        return std::errc::invalid_argument;

    if (event.parent.index >= m_widgets.size())
        return std::errc::invalid_argument;

    auto parent = event.parent.index ? m_widgets[event.parent.index] : &m_root;
    const auto parent_window = qobject_cast<QMainWindow*>(parent);
    if (parent_window)
        parent = parent_window->centralWidget();

    const auto index = event.widget.index;
    const auto update_func = [this, index](const QVariant& var)
    {
        auto node = new qtgui_event_node;
        node->type = event_type::gui_update;
        node->args.widget.index = index;
        const auto cerr = qtgui_make_variant(var, node->args.data);
        m_queue.push(node);
        qtgui_exit(cerr);
    };

    QWidget* widget = nullptr;
    switch (event.wtype)
    {
    case gui_widget_type::none:
        widget = new QWidget(parent);
        make_layout(event.flags, *widget);
        break;

    case gui_widget_type::window:
    {
        const auto new_window = new QMainWindow(event.parent.index ? parent_window : nullptr);
        widget = new_window;
        new_window->setCentralWidget(new QWidget(new_window));
        make_layout(event.flags, *new_window->centralWidget());
        if (event.parent.index)
            new_window->setWindowModality(Qt::WindowModality::ApplicationModal);
        break;
    }    
    
    case gui_widget_type::vsplitter:
        widget = new qtgui_splitter(Qt::Vertical, parent);
        break;

    case gui_widget_type::hsplitter:
        widget = new qtgui_splitter(Qt::Horizontal, parent);
        break;

    case gui_widget_type::label:
        widget = new QLabel(parent);
        break;

    case gui_widget_type::line_edit:
    {
        const auto line_edit = new QLineEdit(parent);
        widget = line_edit;
        if (event.flags & gui_widget_flag::read_only)
            line_edit->setReadOnly(true);

        QObject::connect(line_edit, &QLineEdit::editingFinished,
            [update_func, line_edit]() { update_func(line_edit->text()); });
        break;
    }

    case gui_widget_type::text_edit:
    {
        const auto text_edit = new QTextEdit(parent);
        widget = text_edit;
        if (event.flags & gui_widget_flag::read_only)
            text_edit->setReadOnly(true);

        QObject::connect(text_edit, &QTextEdit::textChanged,
            [update_func, text_edit]() { update_func(text_edit->toPlainText()); });
        break;
    }

    case gui_widget_type::menu:
        widget = new QMenu(parent);
        break;

    case gui_widget_type::menubar:
        widget = new QMenuBar(parent);
        if (parent_window)
            parent_window->setMenuBar(qobject_cast<QMenuBar*>(widget));
        break;

    case gui_widget_type::combo_box:
    {
        const auto combo_box = new QComboBox(parent);
        widget = combo_box;
        QObject::connect(combo_box, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            [update_func](const int index) { update_func(index); });
        break;
    }

    case gui_widget_type::list_view:
        widget = new QListView(parent);
        break;

    case gui_widget_type::tree_view:
        widget = new QTreeView(parent);
        break;

    case gui_widget_type::grid_view:
        widget = new QTableView(parent);
        break;

    case gui_widget_type::message:
        widget = new QMessageBox(parent);
        break;

    case gui_widget_type::progress:
        widget = new QProgressBar(parent);
        break;

    case gui_widget_type::text_button:
        widget = new QPushButton(parent);
        break;

    case gui_widget_type::tool_button:
        widget = new QToolButton(parent);
        break;

    case gui_widget_type::dialog_open_file:
    case gui_widget_type::dialog_save_file:
    case gui_widget_type::dialog_select_directory:
    {
        const auto dialog = new QFileDialog(parent);
        if (event.wtype == gui_widget_type::dialog_open_file)
            dialog->setFileMode(QFileDialog::FileMode::ExistingFile);
        else if (event.wtype == gui_widget_type::dialog_select_directory)
            dialog->setOption(QFileDialog::Option::ShowDirsOnly);
        else
            dialog->setAcceptMode(QFileDialog::AcceptMode::AcceptSave);

        QObject::connect(dialog, &QInputDialog::finished,
            [update_func, dialog](int result) 
            {
                const auto files = dialog->selectedFiles();
                if (result && !files.empty())
                    update_func(files.first()); 
            });
        widget = dialog;
        break;
    }

    case gui_widget_type::dialog_select_color:
        widget = new QColorDialog(parent);
        break;

    case gui_widget_type::dialog_select_font:
        widget = new QFontDialog(parent);
        break;

    case gui_widget_type::dialog_input_line:
    case gui_widget_type::dialog_input_text:
    case gui_widget_type::dialog_input_integer:
    case gui_widget_type::dialog_input_double:
    {
        const auto dialog = new QInputDialog(parent);
        if (event.wtype == gui_widget_type::dialog_input_text)
        {
            dialog->setOption(QInputDialog::InputDialogOption::UsePlainTextEditForTextInput);
            QObject::connect(dialog, &QInputDialog::finished,
                [update_func, dialog](int result) { if (result) update_func(dialog->textValue()); });
        }
        else if (event.wtype == gui_widget_type::dialog_input_integer)
        {
            dialog->setInputMode(QInputDialog::InputMode::IntInput);
            QObject::connect(dialog, &QInputDialog::finished,
                [update_func, dialog](int result) { if (result) update_func(dialog->intValue()); });
        }
        else if (event.wtype == gui_widget_type::dialog_input_double)
        {
            dialog->setInputMode(QInputDialog::InputMode::DoubleInput);
            QObject::connect(dialog, &QInputDialog::finished,
                [update_func, dialog](int result) { if (result) update_func(dialog->doubleValue()); });
        }
        else
        {
            QObject::connect(dialog, &QInputDialog::finished,
                [update_func, dialog](int result) { if (result) update_func(dialog->textValue()); });
        }
        widget = dialog;
        break;
    }

    default:
        return std::errc::function_not_supported;
    }

    if (const auto dialog = qobject_cast<QDialog*>(widget))
        dialog->setModal(true);

    if (event.flags & gui_widget_flag::auto_insert)
    {
        if (const auto box = qobject_cast<QBoxLayout*>(parent->layout()))
            box->addWidget(widget);
    }

    widget->setProperty(qtgui_property_name, index);
    m_widgets[event.widget.index] = widget;
    return {};
}
std::errc qtgui_system::process(const qtgui_event_create_action& event)
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
            qtgui_exit();
        });
    return {};
}
std::errc qtgui_system::process(const qtgui_event_insert_widget& event)
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
std::errc qtgui_system::process(const qtgui_event_insert_action& event)
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
    else if (const auto menubar = qobject_cast<QMenuBar*>(widget))
    {
        menubar->addAction(action);
    }
    return {};
}
std::errc qtgui_system::process(const qtgui_event_show& event)
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
std::errc qtgui_system::process(const qtgui_event_text& event)
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
std::errc qtgui_system::process(const qtgui_event_bind_action& event)
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
std::errc qtgui_system::process(const qtgui_event_bind_widget& event)
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
std::errc qtgui_system::process(const qtgui_event_enable_action& event)
{
    if (event.action.index >= m_actions.size())
        return std::errc::invalid_argument;

    const auto action = m_actions[event.action.index];
    if (!action)
        return std::errc::invalid_argument;

    action->setEnabled(event.value);
    return {};
}

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

#undef ICY_GUI_ERROR
#define ICY_GUI_ERROR(X) if (const auto error = (X)){ if (error.source == error_source_stdlib) return error.code; else return 0xFFFF'FFFFui32; }

extern "C" __declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned CodePage, unsigned long dwFlags,
    const wchar_t* lpWideCharStr, int cchWideChar, char* lpMultiByteStr, int cbMultiByte, char* = nullptr, int* = 0);

uint32_t ICY_QTGUI_API icy_gui_system_create(const int version, gui_system** system) noexcept
{
    if (!system || *system || version != ICY_GUI_VERSION)
        return static_cast<uint32_t>(std::errc::invalid_argument);

    static char path[0x1000] = {};
    static char* args[1] = { path };
    if (__argc && __wargv)
    {
        const auto ptr = __wargv[0];
        const auto len = wcslen(ptr);
        WideCharToMultiByte(65001, 0, ptr, len, path, _countof(path) - 1);
    }
    else if (__argc && __argv)
    {
        const auto max_len = std::min(strlen(__argv[0]), _countof(path) - 1);
        memcpy(path, __argv[0], max_len);
    }
    else
    {
        return uint32_t(std::errc::invalid_argument);
    }

    try
    {
        qtapp_argc = 1;
        qtapp_argv = args;
        *system = new qtgui_system;
        return 0;
    }
    ICY_CATCH;
}