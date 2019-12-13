#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <Windows.h>
#include <Shobjidl.h>
#include "icy_com.hpp"

using namespace icy;

static void file_info_create(const struct _stat64& stat, file_info& info) noexcept
{	
	info.size = size_t(stat.st_size);
	if (stat.st_mode & S_IFDIR)
		info.type = file_type::dir;
	else if (stat.st_mode & S_IFREG)
		info.type = file_type::file;
	else
		info.type = file_type::none;

	info.access = file_access::none;
	if (stat.st_mode & S_IREAD)
		info.access = info.access | file_access::read;
	if (stat.st_mode & S_IWRITE)
		info.access = info.access | file_access::app;
	if (stat.st_mode & S_IEXEC)
		info.access = info.access | file_access::exec;

	info.time_access = std::chrono::system_clock::from_time_t(stat.st_atime);
	info.time_modify = std::chrono::system_clock::from_time_t(stat.st_mtime);
	info.time_status = std::chrono::system_clock::from_time_t(stat.st_ctime);
}

error_type file_info::initialize(const string_view path) noexcept
{
	array<wchar_t> wpath;
	auto wsize = 0_z;
	if (const auto error = path.to_utf16(nullptr, &wsize))
		return error;
	if (const auto error = wpath.resize(wsize + 1))
		return error;
	if (const auto error = path.to_utf16(wpath.data(), &wsize))
		return error;

	struct _stat64 stat = {};
	if (_wstat64(wpath.data(), &stat) != 0)
		return last_system_error();

	file_info_create(stat, *this);
	return {};
}

error_type file::open(const string_view path, const file_access access, const file_open open_mode, const file_share share) noexcept
{
	array<wchar_t> wpath;
	auto size = 0_z;
	ICY_ERROR(path.to_utf16(nullptr, &size));
	ICY_ERROR(wpath.resize(size + 1));
	ICY_ERROR(path.to_utf16(wpath.data(), &size));
	
	auto handle = CreateFileW(wpath.data(), uint32_t(access), uint32_t(share), nullptr, uint32_t(open_mode),
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (handle == INVALID_HANDLE_VALUE)
		return last_system_error();
	ICY_SCOPE_EXIT{ if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); };

	struct _stat64 stat = {};
	if (_wstat64(wpath.data(), &stat) != 0)
		return last_system_error();

	close();
	file_info_create(stat, m_info);
	std::swap(m_handle, handle);
	return {};
}
void file::close() noexcept
{
	if (m_handle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_handle);
		m_handle = INVALID_HANDLE_VALUE;
	}
	m_info = {};
}
error_type file::read(void* data, size_t& size) noexcept
{
	auto count = 0ul;
	if (!ReadFile(m_handle, data, uint32_t(size), &count, nullptr))
		return last_system_error();
	size = count;
	return {};
}
error_type file::append(const void* data, size_t size) noexcept
{
	auto count = 0ul;
	while (size && WriteFile(m_handle, data, uint32_t(size), &count, nullptr))
	{
		reinterpret_cast<const char*&>(data) += count;
		size -= count;
	}
	if (size)
		return last_system_error();
	return {};
}
error_type file::text(const size_t max, array<string>& lines) noexcept
{
	lines.clear();
	if (!m_info.size)
		return {};

	const auto buffer_size = 4096;
	struct buffer_type { char bytes[buffer_size]; };
	array<buffer_type> buffers;
	//	line begins in buffer[cursor / buffer_size] at position [cursor % buffer_size]
	auto beg = 0_z;
	auto cur = 0_z;

	const auto next_line = [this, &lines, max, &buffers, &beg, &cur, buffer_size]() -> error_type
	{
		if (beg >= m_info.size || lines.size() >= max)
			return {};

		string line;
		const auto beg_index = beg / buffer_size;
		const auto cur_index = cur / buffer_size;
		if (beg_index == cur_index)
		{
			ICY_ERROR(line.append(string_view(
				buffers[beg_index].bytes + beg % buffer_size, 
				buffers[beg_index].bytes + cur % buffer_size)));
		}
		else
		{
			ICY_ERROR(line.append(string_view(
				buffers[beg_index].bytes + beg % buffer_size,
				buffers[beg_index].bytes + buffer_size)));
			
			for (auto n = beg_index + 1; n < cur_index; ++n)
				ICY_ERROR(line.append(buffers[n].bytes));
			
			ICY_ERROR(line.append(string_view(
				buffers[cur_index].bytes,
				buffers[cur_index].bytes + cur % buffer_size)));
		}
		ICY_ERROR(lines.push_back(std::move(line)));
		if (cur + 1 < buffers.size() * buffer_size && (
			buffers[(cur + 0) / buffer_size].bytes[(cur + 0) % buffer_size] == '\r' &&
			buffers[(cur + 1) / buffer_size].bytes[(cur + 1) % buffer_size] == '\n'))
		{
			cur += 2;
		}
		else
		{
			cur += 1;
		}
		beg = cur;
		return {};
	};

	while (cur < m_info.size && lines.size() < max)
	{
		if (cur == buffers.size() * buffer_size)
		{
			buffer_type buffer = {};
			auto size = sizeof(buffer.bytes);
			ICY_ERROR(read(buffer.bytes, size));
			ICY_ERROR(buffers.push_back(std::move(buffer)));
		}

		while (cur < buffers.size() * buffer_size)
		{
			const auto chr = buffers[cur / buffer_size].bytes[cur % buffer_size];
			if (chr == '\0')
				return next_line();
			if (chr == '\r' || chr == '\n')
			{
				ICY_ERROR(next_line());
				break;
			}
			++cur;
		}
	}
	ICY_ERROR(next_line());
	if (cur == m_info.size && lines.size() < max)
	{
		const auto chr = buffers[(cur - 1) / buffer_size].bytes[(cur - 1) % buffer_size];
		if (chr == '\r' || chr == '\n')
			lines.push_back(string{});
	}
	return {};
}

error_type file_const_map::initialize(const file& file, const bool readonly) noexcept
{
    ULARGE_INTEGER integer;
    integer.QuadPart = file.m_info.size;
    auto handle = CreateFileMappingW(file.m_handle, nullptr, readonly ? PAGE_READONLY : PAGE_READWRITE, integer.HighPart, integer.LowPart, nullptr);
    if (!handle)
        return last_system_error();
    ICY_SCOPE_EXIT{ CloseHandle(handle); };
    std::swap(m_handle , handle);
    return {};
}
file_const_map::~file_const_map() noexcept
{
    if (m_handle)
        CloseHandle(m_handle);
}
error_type file_const_view::initialize(const file_const_map& map, const size_t offset, const size_t length) noexcept
{
    ULARGE_INTEGER integer;
    integer.QuadPart = offset;
    auto ptr = static_cast<uint8_t*>(MapViewOfFile(map.m_handle, FILE_MAP_READ, integer.HighPart, integer.LowPart, length));
    if (!ptr)
        return last_system_error();

    ICY_SCOPE_EXIT{ UnmapViewOfFile(ptr); };
    std::swap(m_ptr, ptr);
    return {};
}
file_const_view::~file_const_view() noexcept
{
    if (m_ptr)
        UnmapViewOfFile(m_ptr);
}
error_type file_view::initialize(const file_map& map, const size_t offset, const size_t length) noexcept
{
    ULARGE_INTEGER integer;
    integer.QuadPart = offset;
    auto ptr = static_cast<uint8_t*>(MapViewOfFile(map.m_handle, FILE_MAP_READ, integer.HighPart, integer.LowPart, length));
    if (!ptr)
        return last_system_error();

    ICY_SCOPE_EXIT{ UnmapViewOfFile(ptr); };
    std::swap(m_ptr, ptr);
    return {};
}
file_view::~file_view() noexcept
{
    if (m_ptr)
        UnmapViewOfFile(m_ptr);
}

static error_type enum_files(string_view path, const string_view name_prefix, const bool recursive, array<string>& files) noexcept
{
	while (!path.empty() && path.bytes().back() == '/' || path.bytes().back() == '\\')
		path = { path.bytes().data(), path.bytes().size() - 1 };

	array<wchar_t> wpath;
	auto size = 0_z;
	
	ICY_ERROR(path.to_utf16(nullptr, &size));
	ICY_ERROR(wpath.resize(size + 1));
	ICY_ERROR(path.to_utf16(wpath.data(), &size));
	wpath.pop_back();
	ICY_ERROR(wpath.append({ L'/', L'*', L'.', L'*', L'\0' }));
	
	WIN32_FIND_DATA data = {};
	auto handle = FindFirstFileExW(wpath.data(), FindExInfoBasic, &data, 
		FINDEX_SEARCH_OPS::FindExSearchNameMatch, nullptr, FIND_FIRST_EX_CASE_SENSITIVE | FIND_FIRST_EX_LARGE_FETCH);
	if (handle == INVALID_HANDLE_VALUE)
		return last_system_error();
	ICY_SCOPE_EXIT{ FindClose(handle); };
	while (true)
	{
		if (wmemcmp(data.cFileName, L".", 1) == 0 || wmemcmp(data.cFileName, L"..", 2) == 0)
		{
			;
		}
		else
		{
			string file_end;
			string file_name;
			const auto file_bytes = array_view<wchar_t>(data.cFileName, wcsnlen(data.cFileName, MAX_PATH));
			ICY_ERROR(to_string(file_bytes, file_end));
			if (recursive)
			{
				ICY_ERROR(file_name.append(name_prefix));
				ICY_ERROR(file_name.append(file_end));
			}
			else
			{
				file_name = std::move(file_end);
			}

			string new_path;
			string new_prefix;
			if (recursive && (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				ICY_ERROR(new_path.append(path));
				ICY_ERROR(new_path.append("/"_s));
				ICY_ERROR(new_path.append(file_end));
				ICY_ERROR(new_prefix.append(file_name));
				ICY_ERROR(new_prefix.append("/"_s));
			}
			ICY_ERROR(files.push_back(std::move(file_name)));
			if (!new_path.empty())
				ICY_ERROR(enum_files(new_path, new_prefix, true, files));
		}
		if (!FindNextFileW(handle, &data))
		{
			const auto error = last_system_error();
			if (error == make_system_error(make_system_error_code(ERROR_NO_MORE_FILES)))
				break;
			return error;
		}
	}
	return {};
}
error_type icy::enum_files(const string_view path, array<string>& files) noexcept
{
	return ::enum_files(path, {}, false, files);
}
error_type icy::enum_files_rec(const string_view path, array<string>& files) noexcept
{
	return ::enum_files(path, {}, true, files);
}

static error_type file_dialog(string& str, const const_array_view<dialog_filter> filters, const FILEOPENDIALOGOPTIONS options)
{
    library lib("ole32.dll");
    ICY_ERROR(lib.initialize());
    const auto init = ICY_FIND_FUNC(lib, CoInitializeEx);
    const auto uninit = ICY_FIND_FUNC(lib, CoUninitialize);
    const auto create = ICY_FIND_FUNC(lib, CoCreateInstance);

    if (init && uninit && create)
    {
        ICY_COM_ERROR(init(nullptr, COINIT_MULTITHREADED));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    ICY_SCOPE_EXIT{ uninit(); };

    com_ptr<IFileDialog> dialog;
    ICY_COM_ERROR(create(options & FOS_FILEMUSTEXIST ? CLSID_FileOpenDialog : CLSID_FileSaveDialog,
        nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)));

    FILEOPENDIALOGOPTIONS dlg_options = 0;
    ICY_COM_ERROR(dialog->GetOptions(&dlg_options));
    ICY_COM_ERROR(dialog->SetOptions(dlg_options | options));
    
    array<wchar_t> wstr;
    array<size_t> offsets;
    for (auto&& filter : filters)
    {
        const auto append = [&offsets, &wstr](const string_view str)
        {
            array<wchar_t> wide;
            ICY_ERROR(to_utf16(str, wide));
            ICY_ERROR(offsets.push_back(wstr.size()));
            ICY_ERROR(wstr.append(wide.data(), wide.data() + wide.size()));
            return error_type{};
        };
        ICY_ERROR(append(filter.name));
        ICY_ERROR(append(filter.spec));
    }

    array<COMDLG_FILTERSPEC> dlg_filters;
    ICY_ERROR(dlg_filters.reserve(filters.size()));
    for (auto k = 0u; k < offsets.size() / 2; ++k)
    {
        COMDLG_FILTERSPEC new_filter = {};
        new_filter.pszName = wstr.data() + offsets[2 * k + 0];
        new_filter.pszSpec = wstr.data() + offsets[2 * k + 1];
        ICY_ERROR(dlg_filters.push_back(new_filter));
    }
    if (!dlg_filters.empty())
        ICY_COM_ERROR(dialog->SetFileTypes(unsigned(dlg_filters.size()), dlg_filters.data()));
    
    const auto hr = dialog->Show(nullptr);
    if (uint32_t(hr) == make_system_error_code(ERROR_CANCELLED))
        return {};
    ICY_COM_ERROR(hr);

    com_ptr<IShellItem> item;
    ICY_COM_ERROR(dialog->GetResult(&item));
    
    wchar_t* ptr = nullptr;
    ICY_COM_ERROR(item->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &ptr));
    ICY_ERROR(to_string(const_array_view<wchar_t>(ptr, ptr ? wcslen(ptr) : 0), str));
    return {};
}

error_type icy::dialog_open_file(string& file_name,const const_array_view<dialog_filter> filters) noexcept
{
    return file_dialog(file_name, filters, FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);
}
error_type icy::dialog_save_file(string& file_name, const const_array_view<dialog_filter> filters) noexcept
{
    return file_dialog(file_name, filters, FOS_PATHMUSTEXIST);
}
error_type icy::dialog_select_dir(string& dir_path) noexcept
{
    return file_dialog(dir_path, {}, FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST | FOS_PICKFOLDERS);
}

file_name::file_name(const string_view file) noexcept
{
    const auto bytes = file.bytes();
    const auto chr_dot = '.';
    const auto chr_delim_0 = '/';
    const auto chr_delim_1 = '\\';
    const auto ptr = bytes.data();
    const auto len = bytes.size();

    enum
    {
        find_dot,
        find_delim,
    } state = find_dot;
    
    for (auto k = len; k--;)
    {
        const auto chr = bytes[k];
        if (state == find_dot)
        {
            if (chr == chr_dot)
            {
                ext = string_view(ptr + k, ptr + len);
                state = find_delim;
            }
            else if (chr == chr_delim_0 || chr == chr_delim_1)
            {
                nam = string_view(ptr + k + 1, ptr + len);
                dir = string_view(ptr, ptr + k + 1);
                return;
            }
        }
        else if (state == find_delim)
        {
            if (chr == chr_delim_0 || chr == chr_delim_1)
            {
                nam = string_view(ptr + k + 1, ext.bytes().data());
                dir = string_view(ptr, ptr + k + 1);
                return;
            }
        }
    }
    if (ext.empty())
        nam = file;
    else
        nam = string_view(ptr, ext.bytes().data());
}

error_type icy::make_directory(const string_view path) noexcept
{
    const auto chr_drive = L':';
    const auto chr_delim_0 = L'/';
    const auto chr_delim_1 = L'\\';

    array<wchar_t> wide;
    ICY_ERROR(to_utf16(path, wide));
    
    for (auto k = 0u; k < wide.size(); ++k)
    {
        if (wide[k] == chr_drive)
        {
            ++k;
            for (; k < wide.size(); ++k)
            {
                if (wide[k] == chr_delim_0 || wide[k] == chr_delim_1)
                    ;
                else
                    break;
            }
        }
        else if (wide[k] == chr_delim_0 || wide[k] == chr_delim_1)
        {
            const auto saved = wide[k];
            wide[k] = L'\0';
            if (_wmkdir(wide.data()) != 0)
            {
                if (errno == EEXIST)
                    ;
                else
                    return make_stdlib_error(std::errc(errno));
            }
            wide[k] = saved;
        }
    }
    if (_wmkdir(wide.data()) != 0)
    {
        if (errno == EEXIST)
            ;
        else
            return make_stdlib_error(std::errc(errno));
    }
    return {};
}