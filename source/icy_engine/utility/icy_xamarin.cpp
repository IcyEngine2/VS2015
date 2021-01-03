#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/utility/icy_xamarin.hpp>
#include <icy_engine/utility/icy_com.hpp>
/*#pragma comment(lib, "ole32")
#pragma comment(lib, "user32")
#pragma comment(lib, "oleaut32")
#pragma comment(lib, "uuid")*/

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
class xamarin_system_data : public xamarin_system
{
public:
    ~xamarin_system_data() noexcept override;
    error_type initialize() noexcept;
    error_type exec() noexcept override
    {
        return error_type();
    }
    error_type signal(const event_data& event) noexcept override
    {
        return error_type();
    }
};
ICY_STATIC_NAMESPACE_END

error_type xamarin_system_data::initialize() noexcept
{
    library lib = "icy_xamarin_cpp"_lib;
    ICY_ERROR(lib.initialize());
    const auto proc = static_cast<int(*)()>(lib.find("x"));
    if (!proc)
        return make_stdlib_error(std::errc::function_not_supported);

    proc();


    filter(event_type::global_quit);
    return error_type();
}
xamarin_system_data::~xamarin_system_data() noexcept
{
    filter(0);
}

error_type icy::create_event_system(shared_ptr<xamarin_system>& system) noexcept
{
    shared_ptr<xamarin_system_data> ptr;
    ICY_ERROR(make_shared(ptr));
    ICY_ERROR(ptr->initialize());
    system = std::move(ptr);
    return error_type();
}

/*
class reg_key
{
public:
    enum class access
    {
        get_value = KEY_QUERY_VALUE,
        set_value = KEY_SET_VALUE,
        add_sub_key = KEY_CREATE_SUB_KEY,
        get_sub_key = KEY_ENUMERATE_SUB_KEYS,
    };
public:
    static reg_key current_user;
    static reg_key classes_root;
    reg_key() noexcept = default;
    ~reg_key() noexcept
    {
        close();
    }
    error_type create(const string_view name, const access access, reg_key& key) noexcept
    {
        key.close();
        array<wchar_t> wpath;
        ICY_ERROR(to_utf16(name, wpath));
        if (const auto code = RegCreateKeyExW(m_value, wpath.data(), 0, nullptr, 0, uint32_t(access), nullptr, &key.m_value, nullptr))
            return make_system_error(make_system_error_code(code));
        return error_type();
    }
    error_type open(const string_view name, const access access, reg_key& key) noexcept
    {
        key.close();
        array<wchar_t> wpath;
        ICY_ERROR(to_utf16(name, wpath));
        if (const auto code = RegOpenKeyExW(m_value, wpath.data(), 0, uint32_t(access), &key.m_value))
            return make_system_error(make_system_error_code(code));
        return error_type();
    }
    error_type override(reg_key& new_key) noexcept
    {
        if (new_key.m_override)
            return make_stdlib_error(std::errc::invalid_argument);

        if (const auto code = RegOverridePredefKey(m_value, new_key.m_value))
            return make_system_error(make_system_error_code(code));
        new_key.m_override = m_value;
        return error_type();
    }
    error_type set(const string_view name, const string_view value)
    {
        array<wchar_t> wname;
        array<wchar_t> wvalue;
        ICY_ERROR(to_utf16(name, wname));
        ICY_ERROR(to_utf16(value, wvalue));

        if (const auto code = RegSetKeyValueW(m_value, nullptr, name.empty() ? nullptr : wname.data(),
            REG_SZ, wvalue.data(), wvalue.size() * sizeof(wvalue[0])))
            return make_system_error(make_system_error_code(code));

        return error_type();
    }
private:
    explicit reg_key(HKEY value) noexcept : m_value(value)
    {

    }
    void close() noexcept
    {
        if (m_override)
        {
            RegOverridePredefKey(m_override, nullptr);
            m_override = nullptr;
        }
        if (m_value && (reinterpret_cast<uint64_t>(m_value) & 0x8000'0000) == 0)
        {
            RegCloseKey(m_value);
            m_value = nullptr;
        }
    }
private:
    HKEY m_value = nullptr;
    HKEY m_override = nullptr;
};
reg_key reg_key::current_user(HKEY_CURRENT_USER);
reg_key reg_key::classes_root(HKEY_CLASSES_ROOT);

error_type register_com_class(const guid& class_guid, const string_view class_name, const string_view assembly_name, const string_view assembly_path) noexcept
{
    string clsid_str;
    ICY_ERROR(to_string(class_guid, clsid_str));
    ICY_ERROR(clsid_str.insert(clsid_str.begin(), "{"_s));
    ICY_ERROR(clsid_str.append("}"_s));

    reg_key key_classes;
    ICY_ERROR(reg_key::current_user.open("Software\\Classes"_s, reg_key::access::get_sub_key, key_classes));

    reg_key key_clsid_dir;
    ICY_ERROR(key_classes.open("CLSID"_s,
        reg_key::access::get_sub_key, key_clsid_dir));

    reg_key key_clsid;
    ICY_ERROR(key_clsid_dir.create(clsid_str, reg_key::access(
            uint32_t(reg_key::access::add_sub_key) |
            uint32_t(reg_key::access::set_value)), key_clsid));
    ICY_ERROR(key_clsid.set(string_view(), class_name));

    reg_key key_inproc;
    ICY_ERROR(key_clsid.create("InprocServer32"_s, reg_key::access::set_value, key_inproc));
    ICY_ERROR(key_inproc.set(string_view(), "mscoree.dll"_s));

    string assembly_info;
    ICY_ERROR(assembly_info.appendf("%1, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null"_s, assembly_name));
    ICY_ERROR(key_inproc.set("Assembly"_s, assembly_info));
    ICY_ERROR(key_inproc.set("Class"_s, class_name));

    ICY_ERROR(key_inproc.set("CodeBase"_s, assembly_path));
    ICY_ERROR(key_inproc.set("RuntimeVersion"_s, "v4.0.30319"_s));
    ICY_ERROR(key_inproc.set("ThreadingModel"_s, "Both"_s));

    reg_key key_progid;
    ICY_ERROR(key_clsid.create("ProgId"_s, reg_key::access::set_value, key_progid));
    ICY_ERROR(key_progid.set(string_view(), class_name));
    return error_type();
}

 // library lib_ole32 = "ole32"_lib;
   // ICY_ERROR(lib_ole32.initialize());
    const auto dll_name = "icy_xamarin2"_s;
    library lib_xamarin("D:\\VS2015\\build\\icy_xamarin2\\bin\\Debug\\icy_xamarin2.dll");
    ICY_ERROR(lib_xamarin.initialize());

   // const auto func = static_cast<HRESULT(*)()>(lib_xamarin.find("DllRegisterServer"));
    //if (!func)
    //    return make_stdlib_error(std::errc::function_not_supported);

    string proc_name;
    ICY_ERROR(process_name(win32_instance(), proc_name));
    file_name proc_file_name(proc_name);

    string dll_path;
    ICY_ERROR(dll_path.appendf("%1%2.dll"_s, proc_file_name.directory, dll_name));
    //// {04766CA6-007F-4373-9102-EFBFDCE57DB6}
    const auto xamarin_app_class_guid = GUID{ 0x4766ca6, 0x7f, 0x4373, { 0x91, 0x2, 0xef, 0xbf, 0xdc, 0xe5, 0x7d, 0xb6 } };
    const auto xamarin_app_class_name = "Icy.IcyApplication3"_s;
    ICY_ERROR(register_com_class(reinterpret_cast<const guid&>(xamarin_app_class_guid),
        xamarin_app_class_name, dll_name,
        "D:\\VS2015\\build\\icy_xamarin2\\bin\\Debug\\icy_xamarin2.dll"_s
        //"D:/VS2015/bin/Debug/icy_xamarin.dll"_s
        ));

    reg_key key_classes;
    ICY_ERROR(reg_key::current_user.open("Software\\Classes"_s, reg_key::access::get_sub_key, key_classes));
    ICY_ERROR(reg_key::classes_root.override(key_classes));

   // ICY_COM_ERROR(func());

    void* ppv = nullptr;
    ICY_COM_ERROR(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    auto hr = CoGetClassObject(reinterpret_cast<const _GUID&>(xamarin_app_class_guid),
        CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, nullptr, IID_IClassFactory, (void**)&ppv);//IID_IUnknown
    ICY_COM_ERROR(hr);


*/