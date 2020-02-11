#include <icy_qtgui/icy_qtgui.hpp>
#include <icy_engine/core/icy_input.hpp>
#include <QtCore/qtimer.h>
#include <QtCore/qmutex.h>
#include <QtCore/qthread.h>
#include <QtCore/qplugin.h>
#include <QtCore/qqueue.h>
#include <QtCore/quuid.h>
#include <QtCore/qthreadpool.h>
#include <QtCore/qjsondocument.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtGui/qwindow.h>
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
#include <QtWidgets/qheaderview.h>
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
#include <deque>
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
#include "icons/BranchClosed.h"
#include "icons/BranchClosedBegin.h"
#include "icons/BranchClosedEnd.h"
#include "icons/BranchEnd.h"
#include "icons/BranchMore.h"
#include "icons/BranchOpen.h"
#include "icons/BranchOpenBegin.h"
#include "icons/BranchOpenEnd.h"
#include "icons/BranchVLine.h"
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
static const auto qtgui_property_hwnd_window = "hwnd_window";
static const auto qtgui_property_hwnd_xstyle = "hwnd_xstyle";
struct qtgui_event_node
{
    void* unused = nullptr;
    event_type type = event_type::none; 
    gui_event args;
};
QLayout* make_layout(gui_widget_flag flags, QWidget& parent)
{
    const auto layout = uint32_t(flags) & uint32_t(gui_widget_flag::layout_grid);
    switch (gui_widget_flag(layout))
    {
    case icy::gui_widget_flag::layout_hbox:
        return new QHBoxLayout(&parent);
    case icy::gui_widget_flag::layout_vbox:
        return new QVBoxLayout(&parent);
    case icy::gui_widget_flag::layout_grid:
        return new QGridLayout(&parent);
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
    create_win32,
    create_action,
    create_model,
    create_image,
    insert_widget,
    insert_action,
    insert_model_rows,
    insert_model_cols,
    remove_model_rows,
    remove_model_cols,
    move_model_rows,
    move_model_cols,
    show,
    text_widget,
    text_model,
    text_header,
    text_tabs,
    udata_model,
    icon_model,
    icon_widget,
    bind_action,
    bind_widget,
    enable_action,
    enable_widget,
    modify_widget,
    destroy_widget,
    destroy_action,
    destroy_model,
    destroy_image,
    clear_model,
    scroll,
    scroll_tabs,
    input,
};

class qtgui_event_create_widget : public QEvent
{
public:
    qtgui_event_create_widget(const gui_widget widget, const gui_widget_type wtype, const gui_widget parent, const gui_widget_flag flags) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::create_widget))),
        wtype(wtype), flags(flags), parent(parent), widget(widget)
    {

    }
public:
    const gui_widget_type wtype;
    const gui_widget_flag flags;
    const gui_widget parent;
    const gui_widget widget;
};
class qtgui_event_create_win32 : public QEvent
{
public:
    qtgui_event_create_win32(const gui_widget widget, HWND__* const win32, const gui_widget parent, const gui_widget_flag flags) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::create_win32))),
        win32(win32), flags(flags), parent(parent), widget(widget)
    {

    }
public:
    HWND__* const win32;
    const gui_widget_flag flags;
    const gui_widget parent;
    const gui_widget widget;
};
class qtgui_event_create_action : public QEvent
{
public:
    qtgui_event_create_action(const gui_action action, const QString text) : 
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::create_action))), 
        action(action), text(text)
    {

    }
public:
    const gui_action action;
    const QString text;
};
class qtgui_event_create_model : public QEvent
{
public:
    qtgui_event_create_model(const gui_node root) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::create_model))),
        root(root)
    {

    }
public:
    const gui_node root;
};
class qtgui_event_create_image : public QEvent
{
public:
    qtgui_event_create_image(const gui_image image, QImage&& data) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::create_image))),
        image(image), data(std::move(data))
    {

    }
public:
    const gui_image image;
    QImage data;
};
class qtgui_event_insert_widget : public QEvent
{
public:
    qtgui_event_insert_widget(const gui_widget widget, gui_insert args) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::insert_widget))),
        widget(widget), args(args)
    {

    }
public:
    const gui_widget widget;
    const gui_insert args;
};
class qtgui_event_insert_action : public QEvent
{
public:
    qtgui_event_insert_action(const gui_widget widget, const gui_action action) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::insert_action))), 
        widget(widget), action(action)
    {

    }
public:
    const gui_widget widget;
    const gui_action action;
};
class qtgui_event_insert_model_rows : public QEvent
{
public:
    qtgui_event_insert_model_rows(const gui_node node, const int offset, const int count) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::insert_model_rows))),
        node(node), offset(offset), count(count)
    {

    }
public:
    const gui_node node;
    const int offset;
    const int count;
};
class qtgui_event_insert_model_cols : public QEvent
{
public:
    qtgui_event_insert_model_cols(const gui_node node, const int offset, const int count) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::insert_model_cols))),
        node(node), offset(offset), count(count)
    {

    }
public:
    const gui_node node;
    const int offset;
    const int count;
};
class qtgui_event_remove_model_rows : public QEvent
{
public:
    qtgui_event_remove_model_rows(const gui_node node, const int offset, const int count) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::remove_model_rows))),
        node(node), offset(offset), count(count)
    {

    }
public:
    const gui_node node;
    const int offset;
    const int count;
};
class qtgui_event_remove_model_cols : public QEvent
{
public:
    qtgui_event_remove_model_cols(const gui_node node, const int offset, const int count) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::remove_model_cols))),
        node(node), offset(offset), count(count)
    {

    }
public:
    const gui_node node;
    const int offset;
    const int count;
};
class qtgui_event_move_model_rows : public QEvent
{
public:
    qtgui_event_move_model_rows(const gui_node node_src, const int offset_src,
        const int count, const gui_node node_dst, const int offset_dst) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::move_model_rows))),
        node_src(node_src), offset_src(offset_src), count(count), node_dst(node_dst), offset_dst(offset_dst)
    {
    }
public:
    const gui_node node_src;
    const int offset_src;
    const int count;
    const gui_node node_dst;
    const int offset_dst;
};
class qtgui_event_move_model_cols : public QEvent
{
public:
    qtgui_event_move_model_cols(const gui_node node_src, const int offset_src,
        const int count, const gui_node node_dst, const int offset_dst) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::move_model_cols))),
        node_src(node_src), offset_src(offset_src), count(count), node_dst(node_dst), offset_dst(offset_dst)
    {

    }
public:
    const gui_node node_src;
    const int offset_src;
    const int count;
    const gui_node node_dst;
    const int offset_dst;
};
class qtgui_event_show : public QEvent
{
public:
    qtgui_event_show(const gui_widget widget, const bool value) : 
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::show))), 
        widget(widget), value(value)
    {

    }
public:
    const gui_widget widget;
    const bool value;
};
class qtgui_event_text_widget : public QEvent
{
public:
    qtgui_event_text_widget(const gui_widget widget, const QString text) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::text_widget))),
        widget(widget), text(text)
    {

    }
public:
    const gui_widget widget;
    const QString text;
};
class qtgui_event_text_model : public QEvent
{
public:
    qtgui_event_text_model(const gui_node node, const QString text) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::text_model))),
        node(node), text(text)
    {

    }
public:
    const gui_node node;
    const QString text;
};
class qtgui_event_text_tabs : public QEvent
{
public:
    qtgui_event_text_tabs(const gui_widget tabs, const gui_widget widget, const QString text) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::text_tabs))),
        tabs(tabs), widget(widget), text(text)
    {

    }
public:
    const gui_widget tabs;
    const gui_widget widget;
    const QString text;
};
class qtgui_event_udata_model : public QEvent
{
public:
    qtgui_event_udata_model(const gui_node node, const QVariant& var) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::udata_model))),
        node(node), var(var)
    {

    }
public:
    const gui_node node;
    const QVariant var;
};
class qtgui_event_icon_model : public QEvent
{
public:
    qtgui_event_icon_model(const gui_node node, const gui_image image) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::icon_model))),
        node(node), image(image)
    {

    }
public:
    const gui_node node;
    const gui_image image;
};
class qtgui_event_icon_widget : public QEvent
{
public:
    qtgui_event_icon_widget(const gui_widget widget, const gui_image image) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::icon_widget))),
        widget(widget), image(image)
    {

    }
public:
    const gui_widget widget;
    const gui_image image;
};
class qtgui_event_text_header : public QEvent
{
public:
    qtgui_event_text_header(const gui_node node, const uint32_t index, const Qt::Orientation orientation, const QString text) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::text_header))),
        node(node), index(index), orientation(orientation), text(text)
    {

    }
public:
    const gui_node node;
    const uint32_t index;
    const Qt::Orientation orientation;
    const QString text;
};
class qtgui_event_bind_action : public QEvent
{
public:
    qtgui_event_bind_action(const gui_action action, const gui_widget widget) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::bind_action))),
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
    qtgui_event_bind_widget(const gui_widget widget, const gui_node node) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::bind_widget))),
        widget(widget), node(node)
    {

    }
public:
    const gui_widget widget;
    const gui_node node;
};
class qtgui_event_enable_action : public QEvent
{
public:
    qtgui_event_enable_action(const gui_action action, const bool value) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::enable_action))),
        action(action), value(value)
    {

    }
public:
    const gui_action action;
    const bool value;
};
class qtgui_event_enable_widget : public QEvent
{
public:
    qtgui_event_enable_widget(const gui_widget widget, const bool value) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::enable_widget))),
        widget(widget), value(value)
    {

    }
public:
    const gui_widget widget;
    const bool value;
};
class qtgui_event_modify_widget : public QEvent
{
public:
    qtgui_event_modify_widget(const gui_widget widget, QVariantMap&& args) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::modify_widget))),
        widget(widget), args(std::move(args))
    {

    }
public:
    const gui_widget widget;
    const QVariantMap args;
};
class qtgui_event_destroy_widget : public QEvent
{
public:
    qtgui_event_destroy_widget(const gui_widget widget) : 
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::destroy_widget))),
        widget(widget)
    {

    }
public:
    const gui_widget widget;
};
class qtgui_event_destroy_action : public QEvent
{
public:
    qtgui_event_destroy_action(const gui_action action) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::destroy_action))),
        action(action)
    {

    }
public:
    const gui_action action;
};
class qtgui_event_destroy_model : public QEvent
{
public:
    qtgui_event_destroy_model(const gui_node root) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::destroy_model))), root(root)
    {

    }
public:
    const gui_node root;
};
class qtgui_event_destroy_image : public QEvent
{
public:
    qtgui_event_destroy_image(const gui_image image) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::destroy_image))), 
        image(image)
    {

    }
public:
    const gui_image image;
};
class qtgui_event_clear_model : public QEvent
{
public:
    qtgui_event_clear_model(const gui_node root) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::clear_model))), root(root)
    {

    }
public:
    const gui_node root;
};
class qtgui_event_scroll : public QEvent
{
public:
    qtgui_event_scroll(const gui_widget widget, const gui_node node) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::scroll))),
        widget(widget), node(node)
    {

    }
public:
    const gui_widget widget;
    const gui_node node;
};
class qtgui_event_scroll_tabs : public QEvent
{
public:
    qtgui_event_scroll_tabs(const gui_widget tabs, const gui_widget widget) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::scroll_tabs))),
        tabs(tabs), widget(widget)
    {

    }
public:
    const gui_widget tabs;
    const gui_widget widget;
};
class qtgui_event_input : public QEvent
{
public:
    qtgui_event_input(const gui_widget widget, const input_message& msg) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + uint32_t(qtgui_event_type::input))),
        widget(widget), msg(msg)
    {

    }
public:
    const gui_widget widget;
    const input_message msg;
};

class qtgui_splitter : public QSplitter
{
    using QSplitter::QSplitter;
};
class qtgui_tree_view : public QTreeView
{
public:
    using QTreeView::QTreeView;
private:    
    void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override;
};
class qtgui_node_data : public gui_node::data_type
{
public:
    qtgui_node_data(const uint64_t model, const gui_node parent, const int32_t row, const int32_t col, const gui_variant udata) noexcept :
        m_model(model), m_parent(parent), m_row(row), m_col(col), m_udata(udata)
    {

    }
private:
    void add_ref() noexcept override 
    {
        m_ref.fetch_add(1, std::memory_order_release);
    }
    void release() noexcept override
    {
        if (m_ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
            delete this;
    }
    gui_node parent() const noexcept override
    {
        return m_parent;
    }
    int32_t row() const noexcept override
    {
        return m_row;
    }
    int32_t col() const noexcept override
    {
        return m_col;
    }
    uint64_t model() const noexcept override
    {
        return m_model;
    }
    gui_variant udata() const noexcept override
    {
        return m_udata;
    }
private:
    std::atomic<uint32_t> m_ref = 1;
    const uint64_t m_model;
    const gui_node m_parent;
    const int32_t m_row;
    const int32_t m_col;
    const gui_variant m_udata;
};
struct qtgui_node
{
    int32_t parent = 0;
    int32_t row = 0;
    int32_t col = 0;
    int32_t rowCount = 0;
    int32_t colCount = 0;
    QString text;
    QIcon icon;
    QVariant udata;
    QVector<int32_t> nodes;
};
class qtgui_model : public QAbstractItemModel
{
    friend qtgui_tree_view;
public:
    qtgui_model(QObject* const parent) : QAbstractItemModel(parent)
    {
        m_nodes.push_back(qtgui_node());        
    }
    QModelIndex index(const gui_node index) const;
    gui_node node(const QModelIndex& qIndex) const;
    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& index) const override;
    int columnCount(const QModelIndex& index) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& var, int role) override;
    bool insertRows(int row, int count, const QModelIndex& parent) override;
    bool insertColumns(int column, int count, const QModelIndex& parent) override;
    bool removeRows(int row, int count, const QModelIndex& parent) override;
    bool removeColumns(int column, int count, const QModelIndex& parent) override;
    bool moveRows(const QModelIndex& sourceParent, int sourceRow, int count, const QModelIndex& destinationParent, int destinationChild) override;
    bool moveColumns(const QModelIndex& sourceParent, int sourceColumn, int count, const QModelIndex& destinationParent, int destinationChild) override;
    void removeChild(const int32_t index);
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant& value, int role) override;
    void reset();
    void updateChildren(const qtgui_node& parent);
private:
    QList<qtgui_node> m_nodes;
    QQueue<int32_t> m_free;
    QMap<int, QString> m_vheader;
    QMap<int, QString> m_hheader;
};
class qtgui_system : public gui_system, public QApplication
{
public:
    qtgui_system();   
public:
    QImage branch_closed;
    QImage branch_closed_begin;
    QImage branch_closed_end;
    QImage branch_end;
    QImage branch_more;
    QImage branch_open;
    QImage branch_open_begin;
    QImage branch_open_end;
    QImage branch_vline;
private:
    void release() noexcept override;
    bool notify(QObject* object, QEvent* event) noexcept override;
    uint32_t wake() noexcept override;
    uint32_t loop(event_type& type, gui_event& args) noexcept override;
    uint32_t create(gui_widget& widget, const gui_widget_type type, const gui_widget parent, const gui_widget_flag flags) noexcept override;
    uint32_t create(gui_widget& widget, HWND__* const hwnd, const gui_widget parent, const gui_widget_flag flags) noexcept override;
    uint32_t create(gui_action& action, const string_view text) noexcept override;
    uint32_t create(gui_node& root) noexcept override;
    uint32_t create(gui_image& image, const const_matrix_view<color> colors) noexcept override;
    uint32_t insert(const gui_widget widget, const gui_insert args) noexcept override;
    uint32_t insert(const gui_widget widget, const gui_action action) noexcept override;
    uint32_t insert_rows(const gui_node parent, const size_t offset, const size_t count) noexcept override;
    uint32_t insert_cols(const gui_node parent, const size_t offset, const size_t count) noexcept override;
    uint32_t remove_rows(const gui_node parent, const size_t offset, const size_t count) noexcept override;
    uint32_t remove_cols(const gui_node parent, const size_t offset, const size_t count) noexcept override;
    uint32_t move_rows(const gui_node node_src, const size_t offset_src, const size_t count, const gui_node node_dst, const size_t offset_dst) noexcept override;
    uint32_t move_cols(const gui_node node_src, const size_t offset_src, const size_t count, const gui_node node_dst, const size_t offset_dst) noexcept override;
    gui_node node(const gui_node parent, const size_t row, const size_t col) noexcept;
    uint32_t show(const gui_widget widget, const bool value) noexcept override;
    uint32_t text(const gui_widget widget, const string_view text) noexcept override;
    uint32_t text(const gui_node node, const string_view text) noexcept override;
    uint32_t text(const gui_widget tabs, const gui_widget widget, const string_view text) noexcept;
    uint32_t udata(const gui_node node, const gui_variant& var) noexcept override;
    uint32_t icon(const gui_node node, const gui_image image) noexcept override;
    uint32_t icon(const gui_widget widget, const gui_image icon) noexcept override;
    uint32_t vheader(const gui_node node, const uint32_t index, const string_view text) noexcept override;
    uint32_t hheader(const gui_node node, const uint32_t index, const string_view text) noexcept override;
    uint32_t bind(const gui_action action, const gui_widget widget) noexcept override;
    uint32_t bind(const gui_widget widget, const gui_node node) noexcept override;
    uint32_t enable(const gui_action action, const bool value) noexcept override;
    uint32_t enable(const gui_widget widget, const bool value) noexcept override;
    uint32_t modify(const gui_widget widget, const string_view args) noexcept override;
    uint32_t destroy(const gui_widget widget) noexcept override;
    uint32_t destroy(const gui_action action) noexcept override;
    uint32_t destroy(const gui_node root) noexcept override;
    uint32_t destroy(const gui_image image) noexcept override;
    uint32_t clear(const gui_node root) noexcept override;
    uint32_t scroll(const gui_widget widget, const gui_node node) noexcept override;
    uint32_t scroll(const gui_widget tabs, const gui_widget widget) noexcept override;
    uint32_t input(const gui_widget widget, const input_message& msg) noexcept override;
private:
    std::errc process(const qtgui_event_create_widget& event);
    std::errc process(const qtgui_event_create_win32& event);
    std::errc process(const qtgui_event_create_action& event);
    std::errc process(const qtgui_event_create_model& event);
    std::errc process(const qtgui_event_create_image& event);
    std::errc process(const qtgui_event_insert_widget& event);
    std::errc process(const qtgui_event_insert_action& event);
    std::errc process(const qtgui_event_insert_model_rows& event);
    std::errc process(const qtgui_event_insert_model_cols& event);
    std::errc process(const qtgui_event_remove_model_rows& event);
    std::errc process(const qtgui_event_remove_model_cols& event);
    std::errc process(const qtgui_event_move_model_rows& event);
    std::errc process(const qtgui_event_move_model_cols& event);
    std::errc process(const qtgui_event_show& event);
    std::errc process(const qtgui_event_text_widget& event);
    std::errc process(const qtgui_event_text_model& event);
    std::errc process(const qtgui_event_text_tabs& event);
    std::errc process(const qtgui_event_udata_model& event);
    std::errc process(const qtgui_event_icon_model& event);
    std::errc process(const qtgui_event_icon_widget& event);
    std::errc process(const qtgui_event_text_header& event);
    std::errc process(const qtgui_event_bind_action& event);
    std::errc process(const qtgui_event_bind_widget& event);
    std::errc process(const qtgui_event_enable_action& event);
    std::errc process(const qtgui_event_enable_widget& event);
    std::errc process(const qtgui_event_modify_widget& event);
    std::errc process(const qtgui_event_destroy_widget& event);
    std::errc process(const qtgui_event_destroy_action& event);
    std::errc process(const qtgui_event_destroy_model& event);
    std::errc process(const qtgui_event_destroy_image& event);
    std::errc process(const qtgui_event_clear_model& event);
    std::errc process(const qtgui_event_scroll& event);
    std::errc process(const qtgui_event_scroll_tabs& event);
    std::errc process(const qtgui_event_input& event);
private:
    detail::intrusive_mpsc_queue m_queue;
    QWidget m_root;
    QMutex m_lock;
    QList<QWidget*> m_widgets;
    QList<QAction*> m_actions;
    QList<qtgui_model*> m_models;
    QList<QIcon*> m_images;
    QQueue<uint64_t> m_free_widgets;
    QQueue<uint64_t> m_free_actions;
    QQueue<uint64_t> m_free_models;
    QQueue<uint64_t> m_free_images;
    //SendNotifyMessageW
    using func_send_type = int (__stdcall*)(HWND__*, uint32_t msg, size_t wParam, ptrdiff_t lParam);
    using func_style_type = ptrdiff_t(__stdcall*)(HWND__*, int, ptrdiff_t);
    library m_win32_library = "user32"_lib;
    func_send_type m_win32_send = nullptr;
    func_style_type m_win32_style = nullptr;
};
ICY_STATIC_NAMESPACE_END

qtgui_system::qtgui_system() : QApplication(qtapp_argc, qtapp_argv)
{
    m_widgets.push_back(nullptr);
    m_actions.push_back(nullptr);
    m_models.push_back(nullptr);
    m_images.push_back(nullptr);
    const auto style = QStyleFactory::create("Fusion");
    QApplication::setStyle(style);

#define ICY_QTGUI_LOAD_BRANCH(X, Y) X.loadFromData(Y, _countof(Y));
    ICY_QTGUI_LOAD_BRANCH(branch_closed, gCpBytesBranchClosed);
    ICY_QTGUI_LOAD_BRANCH(branch_closed_begin, gCpBytesBranchClosedBegin);
    ICY_QTGUI_LOAD_BRANCH(branch_closed_end, gCpBytesBranchClosedEnd);
    ICY_QTGUI_LOAD_BRANCH(branch_end, gCpBytesBranchEnd);
    ICY_QTGUI_LOAD_BRANCH(branch_more, gCpBytesBranchMore);
    ICY_QTGUI_LOAD_BRANCH(branch_open, gCpBytesBranchOpen);
    ICY_QTGUI_LOAD_BRANCH(branch_open_begin, gCpBytesBranchOpenBegin);
    ICY_QTGUI_LOAD_BRANCH(branch_open_end, gCpBytesBranchOpenEnd);
    ICY_QTGUI_LOAD_BRANCH(branch_vline, gCpBytesBranchVLine);
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
            const auto calcMods = [](const Qt::KeyboardModifiers keyMods)
            {
                auto mods = key_mod::none;
                if (keyMods.testFlag(Qt::KeyboardModifier::ShiftModifier))
                    mods = mods | (key_mod::lshift | key_mod::rshift);
                if (keyMods.testFlag(Qt::KeyboardModifier::ControlModifier))
                    mods = mods | (key_mod::lctrl | key_mod::rctrl);
                if (keyMods.testFlag(Qt::KeyboardModifier::AltModifier))
                    mods = mods | (key_mod::lalt | key_mod::ralt);
                return mods;
            };

            const auto isWindow = qobject_cast<QMainWindow*>(object);
            const auto isDialog = qobject_cast<QDialog*>(object);
            const auto event_type = event->type();
            
            if (event_type == QEvent::Type::Close && (isWindow || isDialog))
            {
                event->ignore();
                const auto index = object->property(qtgui_property_name).toULongLong();

                auto node_0 = new qtgui_event_node;
                node_0->type = event_type::window_close;
                node_0->args.widget.index = index;
                m_queue.push(node_0);

                auto node_1 = new qtgui_event_node;
                node_1->type = event_type::gui_action;
                node_1->args.widget.index = index;
                m_queue.push(node_1);
                qtgui_exit();
                return 0;
            }
            else if (event->type() == QEvent::Type::FocusIn || event->type() == QEvent::Type::FocusOut)
            {
                const auto widget = qobject_cast<QWidget*>(object);
                uint64_t index = widget ? widget->property(qtgui_property_name).toULongLong() : 0;
                
                auto node = new qtgui_event_node;
                node->type = event_type::window_active;
                node->args.widget.index = index;
                node->args.data = event->type() == QEvent::Type::FocusIn;
                m_queue.push(node);
                qtgui_exit();

                output = QApplication::notify(object, event);
                return 0;
            }
            else if (event->type() == QEvent::Type::Quit)
            {
                qtgui_exit();
                return 0;
            }
            else if (m_win32_send && event->isAccepted() && (false
                || event_type == QEvent::Type::MouseMove 
                || event_type == QEvent::Type::MouseButtonRelease
                || event_type == QEvent::Type::MouseButtonPress
                || event_type == QEvent::Type::MouseButtonDblClick
                || event_type == QEvent::Type::Wheel
                || event_type == QEvent::Type::KeyPress 
                || event_type == QEvent::Type::KeyRelease))
            {
                const auto widget = qobject_cast<QWidget*>(object);
                uint64_t index = widget ? widget->property(qtgui_property_name).toULongLong() : 0;

                auto count = 1;
                input_message msg;
                if (event_type == QEvent::Type::Wheel)
                {
                    const auto wheelEvent = static_cast<QWheelEvent*>(event);
                    msg = input_message(input_type::mouse_wheel, key::none, calcMods(wheelEvent->modifiers()));
                    msg.wheel = wheelEvent->angleDelta().y();
                }
                else if (
                    event_type == QEvent::Type::KeyPress || 
                    event_type == QEvent::Type::KeyRelease)
                {
                    const auto keyEvent = static_cast<QKeyEvent*>(event);
                    count = keyEvent->count();
                    const auto type = event_type == QEvent::Type::KeyRelease ? 
                        input_type::key_release : keyEvent->isAutoRepeat() ? input_type::key_hold : input_type::key_press;
                    
                    auto key = key::none;
                    switch (auto qKey = keyEvent->key())
                    {
                    case Qt::Key::Key_Shift: key = key::left_shift; break;
                    case Qt::Key::Key_Control: key = key::left_ctrl; break;
                    case Qt::Key::Key_Alt: key = key::left_alt; break;
                    default: key = icy::key(keyEvent->nativeVirtualKey()); break;
                    }
                    if (to_string(key).empty())
                        count = 0;

                    msg = input_message(type, key, calcMods(keyEvent->modifiers()));
                }
                else
                {
                    const auto mouseEvent = static_cast<QMouseEvent*>(event);
                    auto type = input_type::none;
                    switch (event_type)
                    {
                    case QEvent::Type::MouseButtonPress: type = input_type::mouse_press; break;
                    case QEvent::Type::MouseButtonRelease: type = input_type::mouse_release; break;
                    case QEvent::Type::MouseButtonDblClick: type = input_type::mouse_double; break;
                    case QEvent::Type::MouseMove: type = input_type::mouse_move; break;
                    }
                    auto key = key::none;
                    switch (mouseEvent->button())
                    {
                    case Qt::MouseButton::LeftButton: key = key::mouse_left; break;
                    case Qt::MouseButton::RightButton: key = key::mouse_right; break;
                    case Qt::MouseButton::MiddleButton: key = key::mouse_mid; break;
                    case Qt::MouseButton::XButton1: key = key::mouse_x1; break;
                    case Qt::MouseButton::XButton2: key = key::mouse_x2; break;
                    }
                    auto mods = calcMods(mouseEvent->modifiers());
                    if (mouseEvent->buttons().testFlag(Qt::MouseButton::LeftButton)) mods = mods | key_mod::lmb;
                    if (mouseEvent->buttons().testFlag(Qt::MouseButton::RightButton)) mods = mods | key_mod::rmb;
                    if (mouseEvent->buttons().testFlag(Qt::MouseButton::MiddleButton)) mods = mods | key_mod::mmb;
                    if (mouseEvent->buttons().testFlag(Qt::MouseButton::XButton1)) mods = mods | key_mod::x1mb;
                    if (mouseEvent->buttons().testFlag(Qt::MouseButton::XButton2)) mods = mods | key_mod::x2mb;
                    msg = input_message(type, key, mods);
                    msg.point_x = mouseEvent->localPos().toPoint().x();
                    msg.point_y = mouseEvent->localPos().toPoint().y();
                }
                if (index)
                {
                    for (auto k = 0; k < count; ++k)
                    {
                        auto node = new qtgui_event_node;
                        node->type = event_type::window_input;
                        node->args.widget.index = index;
                        node->args.data = msg;
                        m_queue.push(node);
                    }
                    if (count)
                    {
                        qtgui_exit();
                        //event->ignore();
                    }
                }
                output = QApplication::notify(object, event);
                return 0;
            }
            else if (object == this && event->type() >= QEvent::Type::User && event->type() < QEvent::Type::MaxUser)
            {
                auto error = static_cast<std::errc>(0);
                const auto type = static_cast<qtgui_event_type>(event->type() - QEvent::Type::User);
                QMutexLocker lk(&m_lock);
                switch (type)
                {
                case qtgui_event_type::create_widget:
                    error = process(*static_cast<qtgui_event_create_widget*>(event));
                    break;
                case qtgui_event_type::create_win32:
                    error = process(*static_cast<qtgui_event_create_win32*>(event));
                    break;
                case qtgui_event_type::create_action:
                    error = process(*static_cast<qtgui_event_create_action*>(event));
                    break;
                case qtgui_event_type::create_model:
                    error = process(*static_cast<qtgui_event_create_model*>(event));
                    break;
                case qtgui_event_type::create_image:
                    error = process(*static_cast<qtgui_event_create_image*>(event));
                    break;
                case qtgui_event_type::insert_widget:
                    error = process(*static_cast<qtgui_event_insert_widget*>(event));
                    break;
                case qtgui_event_type::insert_action:
                    error = process(*static_cast<qtgui_event_insert_action*>(event));
                    break;
                case qtgui_event_type::insert_model_rows:
                    error = process(*static_cast<qtgui_event_insert_model_rows*>(event));
                    break;
                case qtgui_event_type::insert_model_cols:
                    error = process(*static_cast<qtgui_event_insert_model_cols*>(event));
                    break;
                case qtgui_event_type::remove_model_rows:
                    error = process(*static_cast<qtgui_event_remove_model_rows*>(event));
                    break;
                case qtgui_event_type::remove_model_cols:
                    error = process(*static_cast<qtgui_event_remove_model_cols*>(event));
                    break;
                case qtgui_event_type::move_model_rows:
                    error = process(*static_cast<qtgui_event_move_model_rows*>(event));
                    break;
                case qtgui_event_type::move_model_cols:
                    error = process(*static_cast<qtgui_event_move_model_cols*>(event));
                    break;
                case qtgui_event_type::show:
                    error = process(*static_cast<qtgui_event_show*>(event));
                    break;
                case qtgui_event_type::text_widget:
                    error = process(*static_cast<qtgui_event_text_widget*>(event));
                    break;
                case qtgui_event_type::text_tabs:
                    error = process(*static_cast<qtgui_event_text_tabs*>(event));
                    break;
                case qtgui_event_type::text_model:
                    error = process(*static_cast<qtgui_event_text_model*>(event));
                    break;
                case qtgui_event_type::udata_model:
                    error = process(*static_cast<qtgui_event_udata_model*>(event));
                    break;
                case qtgui_event_type::icon_model:
                    error = process(*static_cast<qtgui_event_icon_model*>(event));
                    break;
                case qtgui_event_type::icon_widget:
                    error = process(*static_cast<qtgui_event_icon_widget*>(event));
                    break;
                case qtgui_event_type::text_header:
                    error = process(*static_cast<qtgui_event_text_header*>(event));
                    break;
                case qtgui_event_type::bind_action:
                    error = process(*static_cast<qtgui_event_bind_action*>(event));
                    break;
                case qtgui_event_type::bind_widget:
                    error = process(*static_cast<qtgui_event_bind_widget*>(event));
                    break;
                case qtgui_event_type::enable_action:
                    error = process(*static_cast<qtgui_event_enable_action*>(event));
                    break;
                case qtgui_event_type::enable_widget:
                    error = process(*static_cast<qtgui_event_enable_widget*>(event));
                    break;
                case qtgui_event_type::modify_widget:
                    error = process(*static_cast<qtgui_event_modify_widget*>(event));
                    break;
                case qtgui_event_type::destroy_widget:
                    error = process(*static_cast<qtgui_event_destroy_widget*>(event));
                    break;
                case qtgui_event_type::destroy_action:
                    error = process(*static_cast<qtgui_event_destroy_action*>(event));
                    break;
                case qtgui_event_type::destroy_model:
                    error = process(*static_cast<qtgui_event_destroy_model*>(event));
                    break;
                case qtgui_event_type::destroy_image:
                    error = process(*static_cast<qtgui_event_destroy_image*>(event));
                    break;
                case qtgui_event_type::clear_model:
                    error = process(*static_cast<qtgui_event_clear_model*>(event));
                    break;
                case qtgui_event_type::scroll:
                    error = process(*static_cast<qtgui_event_scroll*>(event));
                    break;
                case qtgui_event_type::scroll_tabs:
                    error = process(*static_cast<qtgui_event_scroll_tabs*>(event));
                    break;
                case qtgui_event_type::input:
                    error = process(*static_cast<qtgui_event_input*>(event));
                    break;
                default:
                    error = std::errc::function_not_supported;
                    break;
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
            auto index = m_widgets.size();
            if (m_free_widgets.empty())
            {
                m_widgets.push_back(nullptr);
            }
            else
            {
                index = m_free_widgets.takeFirst();
            }
            widget.index = uint64_t(index);
        }
        const auto event = new qtgui_event_create_widget(widget, type, parent, flags);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::create(gui_widget& widget, HWND__* const win32, const gui_widget parent, const gui_widget_flag flags) noexcept
{
    try
    {
        {
            QMutexLocker lk(&m_lock);
            auto index = m_widgets.size();
            if (m_free_widgets.empty())
            {
                m_widgets.push_back(nullptr);
            }
            else
            {
                index = m_free_widgets.takeFirst();
            }
            widget.index = uint64_t(index);
        }
        const auto event = new qtgui_event_create_win32(widget, win32, parent, flags);
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
            auto index = m_actions.size();
            if (m_free_actions.empty())
            {
                m_actions.push_back(nullptr);
            }
            else
            {
                index = m_free_actions.takeFirst();
            }
            action.index = uint64_t(index);
        }
        const auto event = new qtgui_event_create_action(action, make_string(text));
        qApp->postEvent(this, event);
        return {};
    }
    ICY_CATCH;
}
uint32_t qtgui_system::create(gui_node& root) noexcept
{
    try
    {
        root = {};
        {
            QMutexLocker lk(&m_lock);
            auto index = m_models.size();
            if (m_free_models.empty())
            {
                m_models.push_back(nullptr);
            }
            else
            {
                index = m_free_models.takeFirst();
            }
            root._ptr = new qtgui_node_data(index, gui_node(), 0, 0, gui_variant());
        }
        const auto event = new qtgui_event_create_model(root);
        qApp->postEvent(this, event);
        return {};
    }
    ICY_CATCH;
}
uint32_t qtgui_system::create(gui_image& image, const const_matrix_view<color> colors) noexcept
{
    try
    {
        QImage qImage(int(colors.cols()), int(colors.rows()), QImage::Format::Format_RGBA8888);
        for (auto row = 0; row < qImage.height(); ++row)
        {
            const auto src = colors.row(row);
            auto dst = qImage.scanLine(row);
            for (auto col = 0; col < qImage.width(); ++col)
            {
                (*dst++) = src[col].r;
                (*dst++) = src[col].g;
                (*dst++) = src[col].b;
                (*dst++) = src[col].a;
            }
        }        
        {
            QMutexLocker lk(&m_lock);
            auto index = m_images.size();
            if (m_free_images.empty())
            {
                m_images.push_back(nullptr);
            }
            else
            {
                index = m_free_images.takeFirst();
            }
            image.index = index;
        }
        const auto event = new qtgui_event_create_image(image, std::move(qImage));
        qApp->postEvent(this, event);
        return {};
    }
    ICY_CATCH;
}
uint32_t qtgui_system::insert(const gui_widget widget, const gui_insert args) noexcept
{
    try
    {
        const auto event = new qtgui_event_insert_widget(widget, args);
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
uint32_t qtgui_system::insert_rows(const gui_node parent, const size_t offset, const size_t count) noexcept
{
    try
    {
        if (!parent || offset >= INT32_MAX || count >= INT32_MAX)
            return uint32_t(std::errc::invalid_argument);
        const auto event = new qtgui_event_insert_model_rows(parent, int32_t(offset), int32_t(count));
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::insert_cols(const gui_node parent, const size_t offset, const size_t count) noexcept
{
    try
    {
        if (!parent || offset >= INT32_MAX || count >= INT32_MAX)
            return uint32_t(std::errc::invalid_argument);
        const auto event = new qtgui_event_insert_model_cols(parent, int32_t(offset), int32_t(count));
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::remove_rows(const gui_node parent, const size_t offset, const size_t count) noexcept
{
    try
    {
        if (!parent || offset >= INT32_MAX || count >= INT32_MAX)
            return uint32_t(std::errc::invalid_argument);
        const auto event = new qtgui_event_remove_model_rows(parent, int32_t(offset), int32_t(count));
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::remove_cols(const gui_node parent, const size_t offset, const size_t count) noexcept
{
    try
    {
        if (!parent || offset >= INT32_MAX || count >= INT32_MAX)
            return uint32_t(std::errc::invalid_argument);
        const auto event = new qtgui_event_remove_model_rows(parent, int32_t(offset), int32_t(count));
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::move_rows(const gui_node node_src, const size_t offset_src, const size_t count, const gui_node node_dst, const size_t offset_dst) noexcept
{
    try
    {
        if (!node_src || !node_dst || offset_src >= INT32_MAX || count >= INT32_MAX || offset_dst >= INT32_MAX)
            return uint32_t(std::errc::invalid_argument);
        const auto event = new qtgui_event_move_model_rows(node_src, 
            int32_t(offset_src), int32_t(count), node_dst, int32_t(offset_dst));
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::move_cols(const gui_node node_src, const size_t offset_src, const size_t count, const gui_node node_dst, const size_t offset_dst) noexcept
{
    try
    {
        if (!node_src || !node_dst || offset_src >= INT32_MAX || count >= INT32_MAX || offset_dst >= INT32_MAX)
            return uint32_t(std::errc::invalid_argument);
        const auto event = new qtgui_event_move_model_cols(node_src,
            int32_t(offset_src), int32_t(count), node_dst, int32_t(offset_dst));
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
gui_node qtgui_system::node(const gui_node parent, const size_t row, const size_t col) noexcept
{
    try
    {
        if (!parent || row >= INT32_MAX || col >= INT32_MAX)
            return {};
        gui_node node;
        node._ptr = new qtgui_node_data(parent.model(), parent, uint32_t(row), uint32_t(col), gui_variant());
        return node;
    }
    catch (...)
    {
        return {};
    }
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
        const auto event = new qtgui_event_text_widget(widget, make_string(text));
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::text(const gui_widget tabs, const gui_widget widget, const string_view text) noexcept
{
    try
    {
        const auto event = new qtgui_event_text_tabs(tabs, widget, make_string(text));
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::text(const gui_node node, const string_view text) noexcept
{
    try
    {
        const auto event = new qtgui_event_text_model(node, make_string(text));
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::udata(const gui_node node, const gui_variant& var) noexcept
{
    try
    {
        const auto event = new qtgui_event_udata_model(node, qtgui_make_variant(var));
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::icon(const gui_node node, const gui_image image) noexcept
{
    try
    {
        const auto event = new qtgui_event_icon_model(node, image);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::icon(const gui_widget widget, const gui_image image) noexcept
{
    try
    {
        const auto event = new qtgui_event_icon_widget(widget, image);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::vheader(const gui_node node, const uint32_t index, const string_view text) noexcept
{
    try
    {
        const auto event = new qtgui_event_text_header(node, index, Qt::Vertical, make_string(text));
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::hheader(const gui_node node, const uint32_t index, const string_view text) noexcept
{
    try
    {
        const auto event = new qtgui_event_text_header(node, index, Qt::Horizontal, make_string(text));
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
uint32_t qtgui_system::bind(const gui_widget widget, const gui_node node) noexcept
{
    try
    {
        const auto event = new qtgui_event_bind_widget(widget, node);
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
uint32_t qtgui_system::enable(const gui_widget widget, const bool value) noexcept
{
    try
    {
        const auto event = new qtgui_event_enable_widget(widget, value);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::modify(const gui_widget widget, const string_view args) noexcept
{
    try
    {
        auto doc = QJsonDocument::fromJson(QByteArray(args.bytes().data(), int(args.bytes().size())));
        const auto event = new qtgui_event_modify_widget(widget, doc.toVariant().toMap());
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::destroy(const gui_widget widget) noexcept
{
    try
    {
        const auto event = new qtgui_event_destroy_widget(widget);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::destroy(const gui_action action) noexcept
{
    try
    {
        const auto event = new qtgui_event_destroy_action(action);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::destroy(const gui_node root) noexcept
{
    try
    {
        const auto event = new qtgui_event_destroy_model(root);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::destroy(const gui_image image) noexcept
{
    try
    {
        const auto event = new qtgui_event_destroy_image(image);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::clear(const gui_node root) noexcept
{
    try
    {
        const auto event = new qtgui_event_clear_model(root);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::scroll(const gui_widget widget, const gui_node node) noexcept
{
    try
    {
        const auto event = new qtgui_event_scroll(widget, node);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::scroll(const gui_widget tabs, const gui_widget widget) noexcept
{
    try
    {
        const auto event = new qtgui_event_scroll_tabs(tabs, widget);
        qApp->postEvent(this, event);
    }
    ICY_CATCH;
    return {};
}
uint32_t qtgui_system::input(const gui_widget widget, const input_message& msg) noexcept
{
    try
    {
        const auto event = new qtgui_event_input(widget, msg);
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
    const auto func = [this, index](const QVariant& var, const event_type type)
    {
        auto node = new qtgui_event_node;
        node->type = type;
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
    case gui_widget_type::dialog:
    {
        const auto new_window = new QMainWindow(event.parent.index ? parent_window : nullptr);
        widget = new_window;
        new_window->setCentralWidget(new QWidget(new_window));
        make_layout(event.flags, *new_window->centralWidget());
        if (event.wtype == gui_widget_type::dialog)
        {
            new_window->setWindowFlag(Qt::WindowType::Dialog, true);
            new_window->setWindowModality(Qt::WindowModality::ApplicationModal);
        }
        break;
    }    
        
    case gui_widget_type::vsplitter:
        widget = new qtgui_splitter(Qt::Vertical, parent);
        break;

    case gui_widget_type::hsplitter:
        widget = new qtgui_splitter(Qt::Horizontal, parent);
        break;

    case gui_widget_type::tabs:
    {
        auto tabs = new QTabWidget(parent);
        widget = tabs;
        tabs->setTabsClosable(true);
        QObject::connect(tabs, &QTabWidget::tabCloseRequested, [func, tabs](int index)
            {
                const auto widget = tabs->widget(index);
                if (widget)
                    func(widget->property(qtgui_property_name), event_type::gui_update);
            });
        QObject::connect(tabs, &QTabWidget::currentChanged, [func, tabs](int index)
            {
                const auto widget = tabs->widget(index);
                if (widget)
                    func(widget->property(qtgui_property_name), event_type::gui_select);
            });
        break;
    }

    case gui_widget_type::label:
        widget = new QLabel(parent);
        break;

    case gui_widget_type::frame:
        widget = new QFrame(parent);
        make_layout(event.flags, *widget);
        break;

    case gui_widget_type::line_edit:
    {
        const auto line_edit = new QLineEdit(parent);
        widget = line_edit;
        QObject::connect(line_edit, &QLineEdit::editingFinished,
            [func, line_edit]() { func(line_edit->text(), event_type::gui_update); });
        break;
    }

    case gui_widget_type::text_edit:
    {
        const auto text_edit = new QTextEdit(parent);
        widget = text_edit;
        QObject::connect(text_edit, &QTextEdit::textChanged,
            [func, text_edit]() { func(text_edit->toPlainText(), event_type::gui_update); });
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
        //combo_box->setEditable(true);
        //combo_box->lineEdit()->setReadOnly(true);
        widget = combo_box;
        QObject::connect(combo_box, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            [combo_box, func](const int index)
            {
                if (const auto model = combo_box->model())
                {
                    const auto modelIndex = model->index(index, 0, combo_box->rootModelIndex());
                    func(modelIndex, event_type::gui_select);
                }
            });
        break;
    }

    case gui_widget_type::list_view:
        widget = new QListView(parent);
        break;

    case gui_widget_type::tree_view:
    {
        const auto view = new qtgui_tree_view(parent);
        widget = view;
        view->header()->setHidden(true);
        break;
    }

    case gui_widget_type::grid_view:
    {
        const auto view = new QTableView(parent);
        widget = view;
        view->horizontalHeader()->setHidden(true);
        view->verticalHeader()->setHidden(true);
        break;
    }

    case gui_widget_type::message:
    {
        const auto message = new QMessageBox(parent);
        widget = message;
        QObject::connect(message, &QMessageBox::buttonClicked,
            [func, message](QAbstractButton*) { func(QVariant(), event_type::window_close); });
        break;
    }

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
        const auto dialog = new QFileDialog(nullptr);
        if (event.wtype == gui_widget_type::dialog_open_file)
            dialog->setFileMode(QFileDialog::FileMode::ExistingFile);
        else if (event.wtype == gui_widget_type::dialog_select_directory)
            dialog->setOption(QFileDialog::Option::ShowDirsOnly);
        else
            dialog->setAcceptMode(QFileDialog::AcceptMode::AcceptSave);

        QObject::connect(dialog, &QInputDialog::finished,
            [func, dialog](int result) 
            {
                const auto files = dialog->selectedFiles();
                if (result && !files.empty())
                    func(files.first(), event_type::gui_select); 
            });
        widget = dialog;
        break;
    }

    case gui_widget_type::dialog_select_color:
        widget = new QColorDialog(nullptr);
        break;

    case gui_widget_type::dialog_select_font:
        widget = new QFontDialog(nullptr);
        break;

    case gui_widget_type::dialog_input_line:
    case gui_widget_type::dialog_input_text:
    case gui_widget_type::dialog_input_integer:
    case gui_widget_type::dialog_input_double:
    {
        const auto dialog = new QInputDialog(nullptr);
        dialog->setWindowFlag(Qt::WindowType::MSWindowsFixedSizeDialogHint);
        if (event.wtype == gui_widget_type::dialog_input_text)
        {
            dialog->setOption(QInputDialog::InputDialogOption::UsePlainTextEditForTextInput);
            QObject::connect(dialog, &QInputDialog::finished,
                [func, dialog](int result) { func(result ? dialog->textValue() : QVariant(), event_type::gui_update); });
        }
        else if (event.wtype == gui_widget_type::dialog_input_integer)
        {
            dialog->setInputMode(QInputDialog::InputMode::IntInput);
            QObject::connect(dialog, &QInputDialog::finished,
                [func, dialog](int result) { func(result ? dialog->intValue() : QVariant(), event_type::gui_update); });
        }
        else if (event.wtype == gui_widget_type::dialog_input_double)
        {
            dialog->setInputMode(QInputDialog::InputMode::DoubleInput);
            QObject::connect(dialog, &QInputDialog::finished,
                [func, dialog](int result) { func(result ? dialog->doubleValue() : QVariant(), event_type::gui_update); });
        }
        else
        {
            QObject::connect(dialog, &QInputDialog::finished,
                [func, dialog](int result) { func(result ? dialog->textValue() : QVariant(), event_type::gui_update); });
        }
        widget = dialog;
        break;
    }

    default:
        return std::errc::function_not_supported;
    }

    widget->setProperty(qtgui_property_name, index);

    if (const auto dialog = qobject_cast<QDialog*>(widget))
    {
        dialog->setModal(true);
    }
    if (const auto view = qobject_cast<QAbstractItemView*>(widget))
    {
        view->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
        view->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
        view->setAutoScroll(true);
        QObject::connect(view, &QAbstractItemView::customContextMenuRequested, 
            [view, func](const QPoint& point){ func(view->indexAt(point), event_type::gui_context); });        
    }
    if (const auto tabs = qobject_cast<QTabWidget*>(parent))
    {
        tabs->addTab(widget, QString::number(tabs->count()));
    }
    if (const auto button = qobject_cast<QAbstractButton*>(widget))
    {
        QObject::connect(button, &QAbstractButton::clicked, 
            [button, func](bool checked) { func(checked, event_type::gui_update); });
    }

    if (event.flags & gui_widget_flag::auto_insert)
    {
        if (const auto box = qobject_cast<QBoxLayout*>(parent->layout()))
            box->addWidget(widget);
    }

    m_widgets[event.widget.index] = widget;
    return {};
}
std::errc qtgui_system::process(const qtgui_event_create_win32& event)
{
    if (event.widget.index >= m_widgets.size())
        return std::errc::invalid_argument;

    if (event.parent.index >= m_widgets.size())
        return std::errc::invalid_argument;

    auto parent = event.parent.index ? m_widgets[event.parent.index] : &m_root;
    const auto parent_window = qobject_cast<QMainWindow*>(parent);
    if (parent_window)
        parent = parent_window->centralWidget();

    const auto window = QWindow::fromWinId(WId(event.win32));

    //using func_enable_type = int(__stdcall*)(HWND__*, int);
    //using func_get_window_long_ptr_type = ptrdiff_t(__stdcall*)(HWND__*, int);
    //using func_set_window_long_ptr_type = ptrdiff_t(__stdcall*)(HWND__*, int, ptrdiff_t);
    if (!m_win32_send)
    {
        m_win32_library.initialize();
        m_win32_send = m_win32_library.find<func_send_type>("SendNotifyMessageW");
        m_win32_style = m_win32_library.find<func_style_type>("SetWindowLongPtrW");
    }
    //const auto func_enable = m_win32.find<func_enable_type>("EnableWindow");
    //const auto func_get = m_win32.find<func_get_window_long_ptr_type>("GetWindowLongPtrW");
    //const auto func_set = m_win32.find<func_set_window_long_ptr_type>("SetWindowLongPtrW");

    if (!m_win32_send || !m_win32_style)
        return std::errc::function_not_supported;

    //func_enable(event.win32, 0);
    //const auto old_ptr = func_get(event.win32, id_exstyle);
    const auto old_ptr = m_win32_style(event.win32, -20, 0x08000000);
    QWidget* widget = nullptr;
    if (event.parent.index)
    {
        widget = QWidget::createWindowContainer(window, parent);
    }
    else
    {
        auto mainWindow = new QMainWindow;
        widget = mainWindow;
        mainWindow->setCentralWidget(QWidget::createWindowContainer(window, widget));
        make_layout(event.flags, *mainWindow->centralWidget());
    }

    if (!widget)
        return {};

    if (event.flags & gui_widget_flag::auto_insert)
    {
        if (const auto box = qobject_cast<QBoxLayout*>(parent->layout()))
            box->addWidget(widget);
    }
    if (const auto tabs = qobject_cast<QTabWidget*>(parent))
    {
        tabs->addTab(widget, QString::number(tabs->count()));
    }
    
    widget->setProperty(qtgui_property_name, event.widget.index);
    widget->setProperty(qtgui_property_hwnd_window, QVariant::fromValue(window));
    widget->setProperty(qtgui_property_hwnd_xstyle, old_ptr);
    //widget->setAttribute(Qt::WA_TransparentForMouseEvents);
    //widget->setEnabled(false);
    //widget->setFocusPolicy(Qt::FocusPolicy::NoFocus);

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
std::errc qtgui_system::process(const qtgui_event_create_model& event)
{
    if (event.root.model() >= m_models.size())
        return std::errc::invalid_argument;

    auto model = new qtgui_model(&m_root);
    model->setProperty(qtgui_property_name, event.root.model());
    m_models[event.root.model()] = model;
    return {};
}
std::errc qtgui_system::process(const qtgui_event_create_image& event)
{
    if (event.image.index >= m_images.size())
        return std::errc::invalid_argument;

    auto image = new QIcon;
    m_images[event.image.index] = image;
    image->addPixmap(QPixmap::fromImage(event.data));
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
        grid->addWidget(widget, event.args.y, event.args.x, event.args.dy, event.args.dx);
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
std::errc qtgui_system::process(const qtgui_event_insert_model_rows& event)
{
    if (event.node.model() >= m_models.size())
        return std::errc::invalid_argument;

    const auto model = m_models[event.node.model()];
    if (!model)
        return std::errc::invalid_argument;

    if (!model->insertRows(event.offset, event.count, model->index(event.node)))
        return std::errc::invalid_argument;
    
    return {};
}
std::errc qtgui_system::process(const qtgui_event_insert_model_cols& event)
{
    if (event.node.model() >= m_models.size())
        return std::errc::invalid_argument;

    const auto model = m_models[event.node.model()];
    if (!model)
        return std::errc::invalid_argument;

    if (!model->insertColumns(event.offset, event.count, model->index(event.node)))
        return std::errc::invalid_argument;

    return {};
}
std::errc qtgui_system::process(const qtgui_event_remove_model_rows& event)
{
    if (event.node.model() >= m_models.size())
        return std::errc::invalid_argument;

    const auto model = m_models[event.node.model()];
    if (!model)
        return std::errc::invalid_argument;

    if (!model->removeRows(event.offset, event.count, model->index(event.node)))
        return std::errc::invalid_argument;

    return {};
}
std::errc qtgui_system::process(const qtgui_event_remove_model_cols& event)
{
    if (event.node.model() >= m_models.size())
        return std::errc::invalid_argument;

    const auto model = m_models[event.node.model()];
    if (!model)
        return std::errc::invalid_argument;

    if (!model->removeColumns(event.offset, event.count, model->index(event.node)))
        return std::errc::invalid_argument;

    return {};
}
std::errc qtgui_system::process(const qtgui_event_move_model_rows& event)
{
    if (event.node_src.model() >= m_models.size() ||
        event.node_dst.model() >= m_models.size() ||
        event.node_src.model() != event.node_dst.model())
        return std::errc::invalid_argument;

    const auto model = m_models[event.node_src.model()];
    if (!model)
        return std::errc::invalid_argument;

    if (!model->moveRows(model->index(event.node_src),
        event.offset_src, event.count, model->index(event.node_dst), event.offset_dst))
        return std::errc::invalid_argument;

    return {};
}
std::errc qtgui_system::process(const qtgui_event_move_model_cols& event)
{
    if (event.node_src.model() >= m_models.size() ||
        event.node_dst.model() >= m_models.size() ||
        event.node_src.model() != event.node_dst.model())
        return std::errc::invalid_argument;

    const auto model = m_models[event.node_src.model()];
    if (!model)
        return std::errc::invalid_argument;

    if (!model->moveColumns(model->index(event.node_src),
        event.offset_src, event.count, model->index(event.node_dst), event.offset_dst))
        return std::errc::invalid_argument;

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
    {
        //menu->move(QCursor::pos());
        auto action = menu->exec(QCursor::pos());
        if (!action)
        {
            auto node = new qtgui_event_node;
            node->args.widget = event.widget;
            node->type = event_type::gui_action;
            m_queue.push(node);
            qtgui_exit();
        }        
    }
    else
    {
        widget->setVisible(event.value);
    }
    /*if (const auto window = qobject_cast<QMainWindow*>(widget))
    {
        if (!event.value)
            window->close();
        else
            window->show();
    }
    else
    {*/
    //}
    return {};
}
std::errc qtgui_system::process(const qtgui_event_text_widget& event)
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
    else if (const auto label = qobject_cast<QLabel*>(widget))
    {
        label->setText(event.text);
    }
    else if (const auto lineEdit = qobject_cast<QLineEdit*>(widget))
    {
        lineEdit->setText(event.text);
    }
    else if (const auto textEdit = qobject_cast<QTextEdit*>(widget))
    {
        textEdit->setText(event.text);
    }
    else if (const auto button = qobject_cast<QPushButton*>(widget))
    {
        button->setText(event.text);
    }
    else if (const auto window = qobject_cast<QMainWindow*>(widget))
    {
        widget->setWindowTitle(event.text);
    }
    return {};
}
std::errc qtgui_system::process(const qtgui_event_text_tabs& event)
{
    if (event.tabs.index >= m_widgets.size() ||
        event.widget.index >= m_widgets.size())
        return std::errc::invalid_argument;

    const auto tabs = qobject_cast<QTabWidget*>(m_widgets[event.tabs.index]);
    const auto widget = m_widgets[event.widget.index];
    if (!tabs || !widget)
        return std::errc::invalid_argument;
    
    for (auto k = 0; k < tabs->count(); ++k)
    {
        if (tabs->widget(k) == widget)
        {
            tabs->setTabText(k, event.text);
            return {};
        }
    }
    return std::errc::invalid_argument;
}
std::errc qtgui_system::process(const qtgui_event_text_model& event)
{
    if (event.node.model() >= m_models.size())
        return std::errc::invalid_argument;

    const auto model = m_models[event.node.model()];
    if (!model)
        return std::errc::invalid_argument;

    const auto qIndex = model->index(event.node);
    if (!model->setData(qIndex, event.text, Qt::ItemDataRole::DisplayRole))
        return std::errc::invalid_argument;
    return {};
}
std::errc qtgui_system::process(const qtgui_event_udata_model& event)
{
    if (event.node.model() >= m_models.size())
        return std::errc::invalid_argument;

    const auto model = m_models[event.node.model()];
    if (!model)
        return std::errc::invalid_argument;

    const auto qIndex = model->index(event.node);
    if (!model->setData(qIndex, event.var, Qt::ItemDataRole::UserRole))
        return std::errc::invalid_argument;
    return {};
}
std::errc qtgui_system::process(const qtgui_event_icon_model& event)
{
    if (event.node.model() >= m_models.size() ||
        event.image.index >= m_images.size())
        return std::errc::invalid_argument;

    const auto model = m_models[event.node.model()];
    const auto image = m_images[event.image.index];
    if (!model || !image)
        return std::errc::invalid_argument;

    const auto qIndex = model->index(event.node);
    if (!model->setData(qIndex, *image, Qt::ItemDataRole::DecorationRole))
        return std::errc::invalid_argument;
    return {};
}
std::errc qtgui_system::process(const qtgui_event_icon_widget& event)
{
    if (event.widget.index >= m_widgets.size() ||
        event.image.index >= m_images.size())
        return std::errc::invalid_argument;

    const auto widget = m_widgets[event.widget.index];
    const auto image = m_images[event.image.index];
    if (!widget || !image)
        return std::errc::invalid_argument;

    if (const auto button = qobject_cast<QAbstractButton*>(widget))
    {
        button->setIcon(*image);
    }
    else if (const auto window = qobject_cast<QMainWindow*>(widget))
    {
        window->setWindowIcon(*image);
    }
    else
    {
        return std::errc::invalid_argument;
    }
    return {};
}
std::errc qtgui_system::process(const qtgui_event_text_header& event)
{
    if (event.node.model() >= m_models.size())
        return std::errc::invalid_argument;

    const auto model = m_models[event.node.model()];
    if (!model)
        return std::errc::invalid_argument;

    if (!model->setHeaderData(event.index, event.orientation, event.text, Qt::ItemDataRole::DisplayRole))
        return std::errc::invalid_argument;
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
    if (event.widget.index >= m_widgets.size() ||
        event.node.model() >= m_models.size())
        return std::errc::invalid_argument;

    const auto widget = m_widgets[event.widget.index];
    const auto model = m_models[event.node.model()];
    if (!widget || !model)
        return std::errc::invalid_argument;

    if (const auto view = qobject_cast<QAbstractItemView*>(widget))
    {
        const auto widgetIndex = event.widget.index;
        const auto modelIndex = event.node.model();
        view->setModel(model);
        view->setRootIndex(model->index(event.node));
        if (const auto table = qobject_cast<QTableView*>(view))
        {
            table->verticalHeader()->setVisible(!model->headerData(0, Qt::Vertical, Qt::DisplayRole).isNull());
            table->horizontalHeader()->setVisible(!model->headerData(0, Qt::Horizontal, Qt::DisplayRole).isNull());
        }
        else if (const auto tree = qobject_cast<QTreeView*>(view))
        {
            tree->header()->setVisible(!model->headerData(0, Qt::Horizontal, Qt::DisplayRole).isNull());
        }
        QObject::connect(view->selectionModel(), &QItemSelectionModel::currentChanged, 
            [this, widgetIndex, model](const QModelIndex& qIndex)
            {
                auto node = new (std::nothrow) qtgui_event_node;
                if (!node)
                {
                    qtgui_exit(std::errc::not_enough_memory);
                }
                else
                {
                    node->type = event_type::gui_select;
                    node->args.widget.index = widgetIndex;
                    node->args.data = model->node(qIndex);
                    m_queue.push(node);
                    qtgui_exit();
                }
            });
    }
    else if (const auto combo = qobject_cast<QComboBox*>(widget))
    {
        combo->setModel(model);
        combo->setRootModelIndex(model->index(event.node));
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
std::errc qtgui_system::process(const qtgui_event_enable_widget& event)
{
    if (event.widget.index >= m_widgets.size())
        return std::errc::invalid_argument;

    const auto widget = m_widgets[event.widget.index];
    if (!widget)
        return std::errc::invalid_argument;

    widget->setEnabled(event.value);

    if (const auto textEdit = qobject_cast<QTextEdit*>(widget))
    {
        textEdit->setReadOnly(!event.value);
    }
    if (const auto lineEdit = qobject_cast<QLineEdit*>(widget))
    {
        lineEdit->setReadOnly(!event.value);
    }

    return {};
}
std::errc qtgui_system::process(const qtgui_event_modify_widget& event)
{
    if (event.widget.index >= m_widgets.size())
        return std::errc::invalid_argument;

    const auto widget = m_widgets[event.widget.index];
    if (!widget)
        return std::errc::invalid_argument;

    const auto func = [](const QVariantMap& map, const string_view key)
    {
        const auto find = map.constFind(make_string(key));
        if (find != map.end())
            return find->toMap();
        else
            return QVariantMap();
    };
    
   /* const auto argsLayout = func(event.args, gui_widget_args_keys::layout);
    if (!argsLayout.isEmpty())
    {
        const auto argsStretch = func(argsLayout, gui_widget_args_keys::stretch);

        QLayout* layout = nullptr;
        if (const auto window = qobject_cast<QMainWindow*>(widget))
            layout = window->centralWidget() ? window->centralWidget()->layout() : nullptr;
        else
            layout = widget->layout();

        if (const auto box = qobject_cast<QBoxLayout*>(layout))
        {
            for (auto it = argsStretch.begin(); it != argsStretch.end(); ++it)
            {
                auto keyOk = false;
                auto valOk = false;
                const auto key = it.key().toInt(&keyOk);
                const auto val = it.value().toInt(&valOk);
                if (keyOk && valOk)
                    box->setStretch(key, val);
            }
        }
        else if (const auto grid = qobject_cast<QGridLayout*>(layout))
        {
            const auto argsRow = func(argsStretch, gui_widget_args_keys::row);
            const auto argsCol = func(argsStretch, gui_widget_args_keys::col);
            for (auto it = argsRow.begin(); it != argsRow.end(); ++it)
            {
                auto keyOk = false;
                auto valOk = false;
                const auto key = it.key().toInt(&keyOk);
                const auto val = it.value().toInt(&valOk);
                if (keyOk && valOk)
                    grid->setRowStretch(key, val);
            }
            for (auto it = argsCol.begin(); it != argsCol.end(); ++it)
            {
                auto keyOk = false;
                auto valOk = false;
                const auto key = it.key().toInt(&keyOk);
                const auto val = it.value().toInt(&valOk);
                if (keyOk && valOk)
                    grid->setColumnStretch(key, val);
            }
        }        
    }*/

/*    const auto readonly = event.args.find(make_string(gui_widget_args_keys::readonly));
    if (readonly != event.args.end())
    {
        const auto textEdit = qobject_cast<QTextEdit*>(widget);
        const auto lineEdit = qobject_cast<QLineEdit*>(widget);
        if (readonly->toString() == "true")
        {
            if (textEdit)
            {
                textEdit->setReadOnly(true);
            }
            if (lineEdit)
            {
                lineEdit->setReadOnly(true);
                lineEdit->setBackgroundRole(QPalette::ColorRole::Button);
            }
        }
        else if (readonly->toString() == "false")
        {
            if (textEdit) textEdit->setReadOnly(false);
            if (lineEdit) lineEdit->setReadOnly(false);
        }
    }
    */
    
    return {};
}
std::errc qtgui_system::process(const qtgui_event_destroy_widget& event)
{
    if (event.widget.index >= m_widgets.size())
        return std::errc::invalid_argument;

    const auto widget = m_widgets[event.widget.index];
    if (!widget)
        return std::errc::invalid_argument;

    if (const auto window = widget->property(qtgui_property_hwnd_window).value<QWindow*>())
    {
        window->setParent(nullptr);
        window->setFlag(Qt::WindowType::Window);        
        widget->setParent(nullptr);
        widget->setAttribute(Qt::WidgetAttribute::WA_DeleteOnClose);

        const auto hwnd = reinterpret_cast<HWND__*>(window->winId());
        m_win32_style(hwnd, -20, widget->property(qtgui_property_hwnd_xstyle).toLongLong());        
    }
    else
    {
        widget->hide();
        widget->setParent(nullptr);
        widget->deleteLater();
    }
    m_widgets[event.widget.index] = nullptr;
    m_free_widgets.push_back(event.widget.index);

    return {};
}
std::errc qtgui_system::process(const qtgui_event_destroy_action& event)
{
    if (event.action.index >= m_actions.size())
        return std::errc::invalid_argument;

    const auto action = m_actions[event.action.index];
    if (!action)
        return std::errc::invalid_argument;

    action->deleteLater();
    m_actions[event.action.index] = nullptr;
    m_free_actions.push_back(event.action.index);

    return {};
}
std::errc qtgui_system::process(const qtgui_event_destroy_model& event)
{
    if (event.root.model() >= m_models.size())
        return std::errc::invalid_argument;

    const auto model = m_models[event.root.model()];
    if (!model)
        return std::errc::invalid_argument;

    model->deleteLater();
    m_models[event.root.model()] = nullptr;
    m_free_models.push_back(event.root.model());
    return {};
}
std::errc qtgui_system::process(const qtgui_event_destroy_image& event)
{
    if (event.image.index >= m_images.size())
        return std::errc::invalid_argument;

    const auto image = m_images[event.image.index];
    if (!image)
        return std::errc::invalid_argument;

    delete image;
    m_images[event.image.index] = nullptr;
    m_free_models.push_back(event.image.index);
    return {};
}
std::errc qtgui_system::process(const qtgui_event_clear_model& event)
{
    if (event.root.model() >= m_models.size())
        return std::errc::invalid_argument;

    const auto model = m_models[event.root.model()];
    if (!model)
        return std::errc::invalid_argument;

    model->reset();
    return {};
}
std::errc qtgui_system::process(const qtgui_event_scroll& event)
{
    if (event.widget.index >= m_widgets.size() ||
        event.node.model() >= m_models.size())
        return std::errc::invalid_argument;

    const auto widget = m_widgets[event.widget.index];
    const auto model = m_models[event.node.model()];
    if (!widget || !model)
        return std::errc::invalid_argument;

    const auto qIndex = model->index(event.node);
    if (const auto view = qobject_cast<QAbstractItemView*>(widget))
    {
        view->scrollTo(qIndex);    
        view->selectionModel()->select(qIndex, QItemSelectionModel::SelectionFlag::ClearAndSelect);
    }
    else if (const auto combo = qobject_cast<QComboBox*>(widget))
    {
        const auto index = combo->findData(qIndex.data(Qt::UserRole));
        //combo->view()->selectionModel()->select(qIndex, QItemSelectionModel::SelectionFlag::Select);
        //auto oText = combo->currentText();
        //combo->blockSignals(true);
        combo->setCurrentIndex(index);
        //combo->blockSignals(false);
        //auto nText = combo->currentText();
        //combo->setEditText(combo->currentText());
        //combo->update();
        //oText == nText;
        //combo->setCurrentText(qIndex.data(Qt::DisplayRole).toString());
    }
    else
        return std::errc::invalid_argument;
    return {};
}
std::errc qtgui_system::process(const qtgui_event_scroll_tabs& event)
{
    if (event.tabs.index >= m_widgets.size() ||
        event.widget.index >= m_widgets.size())
        return std::errc::invalid_argument;

    const auto tabs = qobject_cast<QTabWidget*>(m_widgets[event.tabs.index]);
    const auto widget = m_widgets[event.widget.index];
    if (!tabs || !widget)
        return std::errc::invalid_argument;

    tabs->setCurrentWidget(widget);
    return {};
}
std::errc qtgui_system::process(const qtgui_event_input& event)
{
    if (event.widget.index >= m_widgets.size())
        return std::errc::invalid_argument;

    const auto widget = m_widgets[event.widget.index];
    if (!widget || !m_win32_send)
        return std::errc::invalid_argument;
    
    const auto window = widget->property(qtgui_property_hwnd_window).value<QWindow*>();
    if (!window)
        return std::errc::invalid_argument;

    auto input = event.msg;
    if (false
        //|| event.msg.type == input_type::mouse_move
        || event.msg.type == input_type::mouse_wheel
        || event.msg.type == input_type::mouse_release
        || event.msg.type == input_type::mouse_press
        || event.msg.type == input_type::mouse_hold
        || event.msg.type == input_type::mouse_double)
    {
        auto point = widget->mapFromGlobal(QPoint(event.msg.point_x, event.msg.point_y));
        input.point_x = point.x();
        input.point_y = point.y();
    }
    uint32_t msg = 0;
    size_t wParam = 0;
    ptrdiff_t lParam = 0;
    detail::to_winapi(input, msg, wParam, lParam);

    auto ok = m_win32_send(reinterpret_cast<HWND__*>(window->winId()), msg, wParam, lParam);
    if (!ok)
    {
        auto err = last_system_error();
        err = {};
    }

  /*  const auto input = m_input;
    m_input = false;
    ICY_SCOPE_EXIT{ m_input = input; };

    const auto keyMods = key_mod(event.msg.mods);
    auto kMods = Qt::KeyboardModifiers();
    auto mMods = Qt::MouseButtons();
    if (keyMods & (key_mod::lctrl | key_mod::rctrl)) kMods.setFlag(Qt::KeyboardModifier::ControlModifier);
    if (keyMods & (key_mod::lshift | key_mod::rshift)) kMods.setFlag(Qt::KeyboardModifier::ShiftModifier);
    if (keyMods & (key_mod::lalt | key_mod::ralt)) kMods.setFlag(Qt::KeyboardModifier::AltModifier);
    if (keyMods & (key_mod::lmb)) mMods.setFlag(Qt::MouseButton::LeftButton);
    if (keyMods & (key_mod::rmb)) mMods.setFlag(Qt::MouseButton::RightButton);
    if (keyMods & (key_mod::mmb)) mMods.setFlag(Qt::MouseButton::MidButton);
    if (keyMods & (key_mod::x1mb)) mMods.setFlag(Qt::MouseButton::XButton1);
    if (keyMods & (key_mod::x2mb)) mMods.setFlag(Qt::MouseButton::XButton2);

    QPointF point;
    point.setX(event.msg.point_x);
    point.setY(event.msg.point_y);

    auto button = Qt::MouseButton::NoButton;
    auto key = Qt::Key::Key_unknown;

    switch (event.msg.key)
    {
    case key::mouse_left: button = Qt::MouseButton::LeftButton; break;
    case key::mouse_right: button = Qt::MouseButton::RightButton; break;
    case key::mouse_mid: button = Qt::MouseButton::MidButton; break;
    case key::mouse_x1: button = Qt::MouseButton::XButton1; break;
    case key::mouse_x2: button = Qt::MouseButton::XButton2; break;
    }

    switch (event.msg.type)
    {
    case input_type::key_press:
    {
        QKeyEvent keyEvent(QEvent::Type::KeyPress, 0, kMods, 0, event.msg.key, QString(), );
        QApplication::sendEvent(widget, &keyEvent);
        break;
    }
    case input_type::key_release:
    {
        QKeyEvent keyEvent(QEvent::Type::KeyRelease, int(event.msg.key), kMods);
        QApplication::sendEvent(widget, &keyEvent);
        break;
    }
    case input_type::key_hold:
    {
        QKeyEvent keyEvent(QEvent::Type::KeyPress, int(event.msg.key), kMods, QString(), true);
        QApplication::sendEvent(widget, &keyEvent);
        break;
    }
    case input_type::mouse_move:
    {
        QMouseEvent mouseEvent(QEvent::Type::MouseMove, point, button, mMods, kMods);
    }
    default:
        break;
    }*/

    //QApplication::sendEvent(widget, event);
    return {};
}

QModelIndex qtgui_model::index(const gui_node gui_index) const
{
    if (!gui_index.parent())
        return QModelIndex();
    return index(int32_t(gui_index.row()), int32_t(gui_index.col()), qtgui_model::index(gui_index.parent()));
}
QModelIndex qtgui_model::index(int row, int column, const QModelIndex& parent) const
{
    const auto& parentNode = m_nodes[parent.internalId()];
    if (row >= 0 && row < parentNode.rowCount && column >= 0 && column < parentNode.colCount)
        return QAbstractItemModel::createIndex(row, column, parentNode.nodes[row * parentNode.colCount + column]);
    else
        return QModelIndex();    
}
QModelIndex qtgui_model::parent(const QModelIndex& index) const
{
    const auto& node = m_nodes[index.internalId()];
    if (!node.parent)
        return QModelIndex();

    const auto& parentNode = m_nodes[node.parent];
    return QAbstractItemModel::createIndex(parentNode.row, parentNode.col, node.parent);
}
int qtgui_model::rowCount(const QModelIndex& index) const
{
    return m_nodes[index.internalId()].rowCount;
}
int qtgui_model::columnCount(const QModelIndex& index) const
{
    return m_nodes[index.internalId()].colCount;
}
QVariant qtgui_model::data(const QModelIndex& index, int role) const
{
    const auto& node = m_nodes[index.internalId()];
    if (role == Qt::ItemDataRole::DisplayRole ||
        role == Qt::ItemDataRole::EditRole ||
        role == Qt::ItemDataRole::AccessibleTextRole)
        return node.text;
    else if (role == Qt::ItemDataRole::DecorationRole)
        return node.icon;
    else if (role == Qt::ItemDataRole::UserRole)
        return node.udata;
    return QVariant();
}
bool qtgui_model::setData(const QModelIndex& index, const QVariant& var, int role)
{
    auto& node = m_nodes[index.internalId()];
    if (role == Qt::ItemDataRole::DisplayRole)
    {
        node.text = var.toString();
        emit dataChanged(index, index);
        return true;
    }
    else if (role == Qt::ItemDataRole::DecorationRole)
    {
        node.icon = var.value<QIcon>();
        emit dataChanged(index, index);
        return true;
    }
    else if (role == Qt::ItemDataRole::UserRole)
    {
        m_nodes[index.internalId()].udata = var;
        return true;
    }
    return false;
}
void qtgui_model::removeChild(const int32_t index)
{
    m_free.push_back(index);
    for (auto&& node : m_nodes[index].nodes)
        removeChild(node);
    m_nodes[index] = {};
}
bool qtgui_model::insertRows(int row, int count, const QModelIndex& parent)
{
    if (count < 0 || row < 0)
        return false;
    auto& node = m_nodes[parent.internalId()];
    auto r0 = std::min(node.rowCount, row);
    auto r1 = r0 + count;
    if (r1 == r0)
        return true;

    QAbstractItemModel::beginInsertRows(parent, r0, r1 - 1);
    ICY_SCOPE_EXIT{ QAbstractItemModel::endInsertRows(); };
    decltype(node.nodes) newNodes;

    for (auto k = 0; k < r0; ++k)
    {
        for (auto col = 0; col < node.colCount; ++col)
        {
            const auto index = node.nodes[k * node.colCount + col];
            newNodes.push_back(index);
        }
    }
    for (auto k = 0; k < count; ++k)
    {
        for (auto col = 0; col < node.colCount; ++col)
        {
            auto index = 0;
            if (m_free.empty())
            {
                index = m_nodes.size();
                m_nodes.push_back(qtgui_node());                
            }
            else
            {
                index = m_free.front();
                m_free.pop_front();
            }
            m_nodes[index].parent = parent.internalId();
            m_nodes[index].row = r0 + k;
            m_nodes[index].col = col;
            newNodes.push_back(index);
        }
    }
    for (auto k = r0; k < node.rowCount; ++k)
    {
        for (auto col = 0; col < node.colCount; ++col)
        {
            const auto index = node.nodes[k * node.colCount + col];
            newNodes.push_back(index);
            m_nodes[index].row += count;
        }
    }
    node.nodes = std::move(newNodes);
    node.rowCount += r1 - r0;
    return true;
}
bool qtgui_model::insertColumns(int column, int count, const QModelIndex& parent)
{
    if (count < 0 || column < 0)
        return false;
    auto& node = m_nodes[parent.internalId()];
    auto c0 = std::min(node.colCount, column);
    auto c1 = c0 + count;
    if (c1 == c0)
        return true;

    QAbstractItemModel::beginInsertColumns(parent, c0, c1 - 1);
    ICY_SCOPE_EXIT{ QAbstractItemModel::endInsertColumns(); };
    decltype(node.nodes) newNodes;

    for (auto row = 0; row < node.rowCount; ++row)
    {
        for (auto k = 0; k < c0; ++k)
        {
            const auto index = node.nodes[row * node.colCount + k];
            newNodes.push_back(index);
        }
        for (auto k = 0; k < count; ++k)
        {
            auto index = 0;
            if (m_free.empty())
            {
                index = m_nodes.size();
                m_nodes.push_back(qtgui_node());
            }
            else
            {
                index = m_free.front();
                m_free.pop_front();
            }

            m_nodes[index].parent = parent.internalId();
            m_nodes[index].row = row;
            m_nodes[index].col = c0 + k;
            newNodes.push_back(index);
        }

        for (auto k = c0; k < node.colCount; ++k)
        {
            const auto index = node.nodes[row * node.colCount + k];
            newNodes.push_back(index);
            m_nodes[index].col += count;
        }
    }
    node.nodes = std::move(newNodes);
    node.colCount += c1 - c0;
    return true;
}
bool qtgui_model::removeRows(int row, int count, const QModelIndex& parent)
{
    if (count < 0 || row < 0)
        return false;

    auto& node = m_nodes[parent.internalId()];
    auto r0 = row;
    auto r1 = row + count;
    r0 = std::min(node.rowCount, r0);
    r1 = std::min(node.rowCount, r1);
    if (r1 == r0)
        return true;

    QAbstractItemModel::beginRemoveRows(parent, r0, r1 - 1);
    ICY_SCOPE_EXIT{ QAbstractItemModel::endRemoveRows(); };
    decltype(node.nodes) newNodes;

    for (auto k = 0; k < node.rowCount; ++k)
    {
        for (auto col = 0; col < node.colCount; ++col)
        {
            const auto index = node.nodes[k * node.colCount + col];
            auto& child = m_nodes[index];
            if (k < r0 || k >= r1)
            {
                newNodes.push_back(index);
                if (k >= r1)
                    child.row -= count;
            }
            else
            {
                removeChild(index);
            }
        }
    }
    node.nodes = std::move(newNodes);
    node.rowCount -= r1 - r0;
    return true;
}
bool qtgui_model::removeColumns(int column, int count, const QModelIndex& parent)
{
    if (count < 0 || column < 0)
        return false;

    auto& node = m_nodes[parent.internalId()];
    auto c0 = column;
    auto c1 = column + count;
    c0 = std::min(node.colCount, c0);
    c1 = std::min(node.rowCount, c1);
    if (c0 == c1)
        return true;

    QAbstractItemModel::beginRemoveColumns(parent, c0, c1 - 1);
    ICY_SCOPE_EXIT{ QAbstractItemModel::endRemoveColumns(); };
    decltype(node.nodes) newNodes;
    
    for (auto row = 0; row < node.rowCount; ++row)
    {
        for (auto k = 0; k < node.colCount; ++k)
        {
            const auto index = node.nodes[row * node.colCount + k];
            auto& child = m_nodes[index];
            if (k < c0 || k >= c1)
            {
                newNodes.push_back(index);
                if (k >= c1)
                    child.col -= count;
            }
            else
            {
                removeChild(index);
            }
        }
    }
    node.nodes = std::move(newNodes);
    node.colCount -= c1 - c0;
    
    return true;
}
bool qtgui_model::moveRows(const QModelIndex& sourceParent, int sourceRow, int count, const QModelIndex& destinationParent, int destinationChild)
{
    if (count < 0 || sourceRow < 0 || destinationChild < 0 || count <= 0)
        return false;

    auto& srcNode = m_nodes[sourceParent.internalId()];
    auto& dstNode = m_nodes[destinationParent.internalId()];
    const auto srcBeg = sourceRow;
    const auto srcEnd = srcBeg + count;
    const auto dstBeg = &srcNode == &dstNode && destinationChild >= srcEnd ? destinationChild + 1 : destinationChild;
    auto delta = dstBeg - srcBeg;

    if (!QAbstractItemModel::beginMoveRows(sourceParent, srcBeg, srcEnd - 1, destinationParent, dstBeg))
        return false;
    ICY_SCOPE_EXIT{ QAbstractItemModel::endMoveRows(); };
    if (&srcNode == &dstNode)
    {
        decltype(srcNode.nodes) newNodes;
        const auto copyRows = [&srcNode, &newNodes](const int beg, const int end)
        {
            for (auto row = beg; row < end; ++row)
            {
                for (auto col = 0; col < srcNode.colCount; ++col)
                    newNodes.push_back(srcNode.nodes[row * srcNode.colCount + col]);
            }
        };
        if (delta >= count)
        {
            delta -= count;
            copyRows(0, srcBeg);
            copyRows(srcEnd, srcEnd + delta);
            copyRows(srcBeg, srcEnd);
            copyRows(srcEnd + delta, srcNode.rowCount);
        }
        else // if (delta < 0)
        {
            delta = -delta;
            copyRows(0, srcBeg - delta);
            copyRows(srcBeg, srcEnd);
            copyRows(srcBeg - delta, srcBeg);
            copyRows(srcEnd, srcNode.rowCount);
        }
        srcNode.nodes = std::move(newNodes);
        updateChildren(srcNode);
    }
    else
    {
        updateChildren(srcNode);
        updateChildren(dstNode);
        return false;
    }
    return true;
}
bool qtgui_model::moveColumns(const QModelIndex& sourceParent, int sourceColumn, int count, const QModelIndex& destinationParent, int destinationChild)
{
    return false;
}
gui_node qtgui_model::node(const QModelIndex& index) const
{
    if (!index.isValid())
        return gui_node();

    const auto model = property(qtgui_property_name).toULongLong();
    gui_node new_node;
    gui_variant var;
    qtgui_make_variant(m_nodes[index.internalId()].udata, var);

    new_node._ptr = new qtgui_node_data(model, node(index.parent()), index.row(), index.column(), std::move(var));
    return new_node;
}
void qtgui_model::reset()
{
    beginResetModel();
    m_nodes.clear();
    m_nodes.push_back(qtgui_node());
    m_free.clear();
    endResetModel();
}
void qtgui_model::updateChildren(const qtgui_node& node)
{
    for (auto row = 0; row < node.rowCount; ++row)
    {
        for (auto col = 0; col < node.colCount; ++col)
        {
            const auto child = node.nodes[row * node.colCount + col];
            m_nodes[child].row = row;
            m_nodes[child].col = col;
        }
    }
}
QVariant qtgui_model::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        const auto& map = (orientation == Qt::Vertical ? m_vheader : m_hheader);
        const auto it = map.find(section);
        if (it != map.end())
            return it.value();
    }
    return QVariant();
}
bool qtgui_model::setHeaderData(int section, Qt::Orientation orientation, const QVariant& value, int role)
{
    if (role == Qt::DisplayRole && value.type() == QVariant::Type::String)
    {
        auto& map = (orientation == Qt::Vertical ? m_vheader : m_hheader);
        map[section] = value.toString();
        emit headerDataChanged(orientation, section, section);
        return true;
    }
    return false;
}

void qtgui_tree_view::drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const
{
    const auto model = static_cast<const qtgui_model*>(index.model());
    if (!model)
        return QTreeView::drawBranches(painter, rect, index);

    auto point = QPoint{ rect.right() - indentation() + 3, rect.top() };
    
    if (model->m_nodes[index.internalId()].parent == 0)
    {
        if (isExpanded(index))
            painter->drawImage(point, static_cast<qtgui_system*>(qApp)->branch_open_begin);
        else if (model->hasChildren(index))
            painter->drawImage(point, static_cast<qtgui_system*>(qApp)->branch_closed_begin);
        else
            painter->drawImage(point, static_cast<qtgui_system*>(qApp)->branch_vline);
        return;
    }

    const auto isEnd = index.row() + 1 == model->rowCount(index.parent());
    if (model->hasChildren(index))
    {
        if (isEnd)
        {
            if (isExpanded(index))
                painter->drawImage(point, static_cast<qtgui_system*>(qApp)->branch_open_end);
            else
                painter->drawImage(point, static_cast<qtgui_system*>(qApp)->branch_closed_end);
        }
        else if (isExpanded(index))
            painter->drawImage(point, static_cast<qtgui_system*>(qApp)->branch_open);
        else
            painter->drawImage(point, static_cast<qtgui_system*>(qApp)->branch_closed);
    }
    else if (isEnd)
    {
        painter->drawImage(point, static_cast<qtgui_system*>(qApp)->branch_end);
    }
    else
    {
        painter->drawImage(point, static_cast<qtgui_system*>(qApp)->branch_more);
    }

    auto offset = index.internalId();
    while (true)
    {
        offset = model->m_nodes[offset].parent;
        point.setX(point.x() - indentation());
        if (point.x() && offset)
            painter->drawImage(point, static_cast<qtgui_system*>(qApp)->branch_vline);
        else
            break;
    }
}

QVariant qtgui_make_variant(const gui_variant& var)
{
    switch (var.type())
    {
    case gui_variant_type::boolean:
        return var.as_boolean();
    case gui_variant_type::sinteger:
        return var.as_sinteger();
    case gui_variant_type::uinteger:
        return var.as_uinteger();
    case gui_variant_type::floating:
        return var.as_floating();
    case gui_variant_type::guid:
    {
        auto guid = var.as_guid();
        return QUuid(reinterpret_cast<const _GUID&>(guid));
    }
    case gui_variant_type::sstring:
    case gui_variant_type::lstring:
    {
        const auto bytes = var.as_string().bytes();
        return QString::fromUtf8(bytes.data(), static_cast<int>(bytes.size()));
    }
    }
    return {};
}
std::errc qtgui_make_variant(const QVariant& qvar, gui_variant& var)
{
    auto ok = false;
    switch (qvar.type())
    {
    case QVariant::Type::Invalid:
        ok = true;
        break;

    case QVariant::Type::Bool:
        ok = true;
        var = qvar.toBool();
        break;

    case QVariant::Type::Char:
    case QVariant::Type::Int:
    case QVariant::Type::LongLong:
        var = qvar.toLongLong(&ok);
        break;

    case QVariant::Type::UInt:
    case QVariant::Type::ULongLong:
        var = qvar.toULongLong(&ok);
        break;

    case QVariant::Type::Double:
        var = qvar.toDouble(&ok);
        break;

    case QVariant::Type::Uuid:
    {
        ok = true;
        const auto uuid = static_cast<_GUID>(qvar.toUuid());
        var = reinterpret_cast<const guid&>(uuid);
        break;
    }

    case QVariant::Type::ModelIndex:
    {
        ok = true;
        auto qModelIndex = qvar.toModelIndex();
        const auto qModel = static_cast<const qtgui_model*>(qModelIndex.model());
        if (qModel)
        {
            var = qModel->node(qModelIndex);
        }
        else
        {
            var = gui_node();
        }
        break;
    }

    case QVariant::Type::String:
    {
        ok = true;
        const auto str = qvar.toString();
        const auto bytes = str.toUtf8();
        return std::errc(make_variant(var, string_view(bytes)).code);
    }
    }
    if (!ok)
        return std::errc::invalid_argument;

    return {};
}

#undef ICY_GUI_ERROR
#define ICY_GUI_ERROR(X) if (const auto error = (X)){ if (error.source == error_source_stdlib) return error.code; else return 0xFFFF'FFFFui32; }

extern "C" __declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned CodePage, unsigned long dwFlags,
    const wchar_t* lpWideCharStr, int cchWideChar, char* lpMultiByteStr, int cbMultiByte, char* = nullptr, int* = 0);

uint32_t ICY_QTGUI_API icy_gui_system_create(const int version, gui_system*& system) noexcept
{
    if (system || version != ICY_GUI_VERSION)
        return uint32_t(std::errc::invalid_argument);

    static char path[0x1000] = {};
    static char* args[1] = { path };
    if (__argc && __wargv)
    {
        const auto ptr = __wargv[0];
        const auto len = wcslen(ptr);
        WideCharToMultiByte(65001, 0, ptr, int(len), path, _countof(path) - 1);
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
        system = new qtgui_system;
        return 0;
    }
    ICY_CATCH;
}