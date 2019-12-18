#pragma warning(disable:4127)
#pragma warning(disable:4201)
#include <icy_engine/core/icy_core.hpp>
#include <icy_qtgui/icy_qtgui_model.hpp>
#include <icy_qtgui/icy_qtgui_system.hpp>
#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qmutex.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qdatawidgetmapper.h>
#include <QtWidgets/qabstractitemdelegate.h>

using namespace icy;


extern uint32_t qtgui_model_bind(const QModelIndex& index, const gui_widget& widget);
extern QAbstractItemModel* qtgui_model_create(QObject& root, gui_model_base& base);
extern QVariant qtgui_make_variant(const gui_variant& var);
extern std::errc qtgui_make_variant(const QVariant& qvar, gui_variant& var);
extern void qtgui_exit(const std::errc code);

ICY_STATIC_NAMESPACE_BEG
enum qtgui_flag
{
    qtgui_flag_none =   0x00,
    qtgui_flag_hide =   0x01,
};
class qtgui_node
{
public:
    qtgui_node() noexcept : qtgui_node(0, 0, 0)
    {

    }
    qtgui_node(const uint64_t row, const uint64_t col, const uint64_t par) noexcept :
        m_row(row), m_col(col), m_par(par), m_max(0), m_flg(0), m_rows(0), m_cols(0)
    {

    }
    ICY_DEFAULT_COPY_ASSIGN(qtgui_node);
    uint64_t row() const noexcept { return m_row; }
    uint64_t col() const noexcept { return m_col; }
    uint64_t rows() const noexcept { return m_rows; }
    uint64_t cols() const noexcept { return m_cols; }
    uint64_t par() const noexcept { return m_par; }
    uint64_t max() const noexcept { return m_max; }
    bool flag(const qtgui_flag flag) const noexcept { return !!(m_flg & flag); }
    void flag(const qtgui_flag flag, const bool value) noexcept { value ? (m_flg |= flag) : (m_flg &= ~flag); }
    void add(const uint64_t index, const qtgui_node& node) noexcept
    {
        m_max = std::max(m_max, index + 1);
        m_rows = std::max(m_rows, node.row() + 1);
        m_cols = std::max(m_cols, node.col() + 1);
    }
private:
    struct
    {
        uint64_t m_row  :   0x11;
        uint64_t m_rows :   0x11;
        uint64_t m_col  :   0x09;
        uint64_t m_cols :   0x09;
    };
    struct
    {
        uint64_t m_par  :   0x15;
        uint64_t m_max  :   0x15;
        uint64_t m_flg  :   0x01;
    };
};
enum class qtgui_event_type : uint32_t
{
    none,
    init,
    bind,
    sort,
    update,
};
class qtgui_event : public QEvent
{
public:
    qtgui_event(const gui_node node, const qtgui_event_type type, const gui_widget widget = {}) :
        QEvent(static_cast<QEvent::Type>(QEvent::User + static_cast<uint32_t>(type))), node(node), type(type), widget(widget)
    {

    }
public:
    const gui_node node;
    const qtgui_event_type type;
    const gui_widget widget;
};
class qtgui_model : public QAbstractItemModel
{
public:
    qtgui_model(QObject& root, gui_model_base& base) : QAbstractItemModel(&root), m_base(base)
    {
        m_data.push_back(qtgui_node());

        base.add_ref();
        gui_model_base::vtbl_type vtbl;
        vtbl.init = [](const gui_model_view model, gui_node& node)
        { return static_cast<qtgui_model*>(model.ptr)->init(node); };
        vtbl.sort = [](const gui_model_view model, const gui_node node)
        { return static_cast<qtgui_model*>(model.ptr)->sort(node); };
        vtbl.bind = [](const gui_model_view model, const gui_node node, const gui_widget& widget) 
        { return static_cast<qtgui_model*>(model.ptr)->bind(node, widget); };
        vtbl.update = [](const gui_model_view model, const gui_node node)
        { return static_cast<qtgui_model*>(model.ptr)->update(node); };

        base.assign(vtbl);
    }
    ~qtgui_model() noexcept
    {
        m_base.release();
    }
    uint32_t init(gui_node& node) noexcept;
    uint32_t sort(const gui_node node) noexcept
    {
        try
        {
            const auto event = new qtgui_event(node, qtgui_event_type::sort);
            qApp->postEvent(this, event);
            return 0;
        }
        ICY_CATCH;
    }
    uint32_t bind(const gui_node node, const gui_widget& widget) noexcept
    {
        try
        {
            const auto event = new qtgui_event(node, qtgui_event_type::bind, widget);
            qApp->postEvent(this, event);
            return 0;
        }
        ICY_CATCH;
    }
    uint32_t update(const gui_node node) noexcept
    {
        try
        {
            const auto event = new qtgui_event(node, qtgui_event_type::update);
            qApp->postEvent(this, event);
            return 0;
        }
        ICY_CATCH;
    }
private:
    bool event(QEvent* event) override;
    QModelIndex index(int row, int col, const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& index) const override
    {
        return m_data[index.internalId()].rows();
    }
    int columnCount(const QModelIndex& index) const override
    {
        return m_data[index.internalId()].cols();
    }
    QVariant data(const QModelIndex& index, int qtrole) const override;
private:
    std::errc process_init(const qtgui_event& event)
    {        
        return {};
    }
    std::errc process_bind(const qtgui_event& event)
    {
        return static_cast<std::errc>(qtgui_model_bind(createIndex(event.node.row, event.node.col, event.node.idx), event.widget));
    }
    std::errc process_update(const qtgui_event& event)
    {
        const auto index = event.node ? createIndex(event.node.row, event.node.col, event.node.idx) : QModelIndex();
        emit dataChanged(index, index);
        return {};
    }
private:
    gui_model_base& m_base;
    QMutex m_lock;
    std::vector<qtgui_node> m_data;
};
ICY_STATIC_NAMESPACE_END

uint32_t qtgui_model::init(gui_node& node) noexcept
{
    try
    {
        if (node.row >= gui_model_max_row || node.col >= gui_model_max_col)
            return static_cast<uint32_t>(std::errc::invalid_argument);

        {
            QMutexLocker lk(&m_lock);
            if (node.par >= m_data.size())
                return static_cast<uint32_t>(std::errc::invalid_argument);

            const auto new_index = static_cast<uint64_t>(m_data.size());
            m_data.push_back(qtgui_node(node.row, node.col, node.par));
            node.idx = new_index;
            m_data[node.par].add(new_index, m_data.back());
        }
        const auto event = new qtgui_event(node, qtgui_event_type::init);
        qApp->postEvent(this, event);
        return 0;
    }
    ICY_CATCH;
}
bool qtgui_model::event(QEvent* event)
{
    if (event->type() >= QEvent::Type::User && event->type() < QEvent::Type::MaxUser)
    {
        auto error = static_cast<std::errc>(0);
        const auto type = static_cast<qtgui_event_type>(event->type() - QEvent::Type::User);
        QMutexLocker lk(&m_lock);
        const auto& gui_event = *static_cast<qtgui_event*>(event);


        switch (type)
        {
        case qtgui_event_type::init:
            error = process_init(gui_event);
            break;

        case qtgui_event_type::bind:
            error = process_bind(gui_event);
            break;

        case qtgui_event_type::update:
            error = process_update(gui_event);
            break;

        default:
            error = std::errc::function_not_supported;
            break;
        }
        if (error != std::errc(0))
            qtgui_exit(error);
    }
    return QAbstractItemModel::event(event);
}
QModelIndex qtgui_model::index(int row, int col, const QModelIndex& parent) const
{
    auto beg = 1;
    auto end = m_data.size();
    if (parent.internalId())
    {
        auto& node = m_data[parent.internalId()];
        beg = parent.internalId() + 1;
        end = node.max();
    }
    for (auto k = beg; k < end; ++k)
    {
        if (true
            && m_data[k].par() == parent.internalId()
            && m_data[k].row() == row
            && m_data[k].col() == col)
        {
            return createIndex(row, col, k);
        }
    }
    return QModelIndex();
}
QModelIndex qtgui_model::parent(const QModelIndex& child) const
{
    if (child.internalId())
    {
        auto& node = m_data[child.internalId()];
        if (node.par())
        {
            auto& par = m_data[node.par()];
            return createIndex(par.row(), par.col(), node.par());
        }
    }
    return QModelIndex();
}
QVariant qtgui_model::data(const QModelIndex& index, int qtrole) const
{
    auto role = gui_role::view;
    switch (qtrole)
    {
    case Qt::DisplayRole:
        break;
    default:
        return QVariant{};
    }

    gui_node node;
    node.idx = index.internalId();
    node.row = index.row();
    node.col = index.column();
    node.par = index.parent().internalId();
    gui_variant value;
    if (const auto error = m_base.data(node, role, value))
        qtgui_exit(std::errc(error));
    return qtgui_make_variant(value);
}

QAbstractItemModel* qtgui_model_create(QObject& root, gui_model_base& base)
{
    return new qtgui_model(root, base);
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
    auto ok = true;
    switch (qvar.type())
    {
        case QVariant::Type::Bool:
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

        case QVariant::Type::String:
        {
            const auto str = qvar.toString();
            const auto bytes = str.toUtf8();
            if (bytes.size() < gui_variant::max_length)
            {
                var = string_view(bytes);
            }
            else
            {
                if (const auto error = var.initialize(detail::global_heap.realloc,
                    detail::global_heap.user, bytes.data(), bytes.size(), gui_variant_type::lstring))
                {
                    return static_cast<std::errc>(error);
                }
            }
            break;
        }
    }
    if (!ok)
        return std::errc::invalid_argument;

    return {};
}
