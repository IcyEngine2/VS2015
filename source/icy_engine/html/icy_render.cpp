#include <icy_engine/html/icy_render.hpp>

using namespace icy;

error_type html_document_render::update(const html_document_css& doc) noexcept
{
    data.clear();

    for (auto k = 0u; k < doc.size(); ++k)
    {
        const auto node = doc.node(k);
        
    }
    return error_type();
}