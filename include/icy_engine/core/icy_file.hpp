#pragma once

#include "icy_string_view.hpp"

namespace icy
{
	class string;
	template<typename T> class array;

	enum class file_share : uint32_t
	{
		none	=	0x00,
		read	=	0x01,
		write	=	0x02,
		del		=	0x04,
	};
	enum class file_open : uint32_t
	{
		open_existing		=	0x03,
		open_always			=	0x04,
		create_always		=	0x02,
		create_new			=	0x01,
		truncate_existing	=	0x05,
	};
	enum class file_access : uint32_t
	{
		none	=	0x00,
		read	=	0x01,
        write   =   0x02,
		app		=	0x04,
		exec	=	0x20,
	};
	enum class file_type : uint32_t
	{
		none,
		file,
		dir,
	};
	enum class file_flag : uint32_t
	{
		none,
		delete_on_close,
	};

	class file_info
	{
	public:
		file_info() noexcept
		{

		}
		error_type initialize(const string_view path) noexcept;
    public:
		file_type type = file_type::none;
		file_access access = file_access::none;
		size_t size = 0;
		std::chrono::system_clock::time_point time_access;
		std::chrono::system_clock::time_point time_modify;
		std::chrono::system_clock::time_point time_status;		
	};

    class file_const_map;
    class file_map;
    class file_const_view;
    class file_view;

	class file
	{
        friend file_const_map;
	public:
        static error_type remove(const string_view path) noexcept;
        static error_type replace(const string_view src, const string_view dst) noexcept;
        static error_type copy(const string_view src, const string_view dst) noexcept;
        static error_type tmpname(string& str) noexcept;
		file() noexcept = default;
        file(const file&) = delete;
		~file() noexcept
		{
			close();
		}
		auto info() const noexcept
		{
			return m_info;
		}
		void close() noexcept;
		error_type open(const string_view path, const file_access access, const file_open open_mode, const file_share share, const file_flag flags = file_flag::none) noexcept;
		error_type read(void* data, size_t& size) noexcept;
		error_type append(const void* data, size_t size) noexcept;
		error_type text(array<string>& lines) noexcept
		{
			return text(SIZE_MAX, lines);
		}
		error_type text(const size_t max, array<string>& lines) noexcept;
        explicit operator bool() const noexcept
        {
            return m_handle != reinterpret_cast<void*>(-1);
        }
	private:
		file_info m_info;
		void* m_handle = reinterpret_cast<void*>(-1);
	};
    class file_const_map
    {
        friend file_const_view;
        friend file_view;
    public:
        file_const_map() noexcept = default;
        file_const_map(file_const_map& rhs) noexcept = default;
        ~file_const_map() noexcept;
        error_type initialize(const file& file) noexcept
        {
            return initialize(file, true);
        }
    protected:
        error_type initialize(const file& file, const bool readonly) noexcept;
    private:
        void* m_handle = nullptr;
    };
    class file_map : public file_const_map
    {
    public:
        error_type initialize(const file& file) noexcept 
        {
            return file_const_map::initialize(file, false);
        }
    };
    class file_const_view : public const_array_view<uint8_t>
    {
    public:
        file_const_view() noexcept = default;
        file_const_view(const file_const_view&) = delete;
        ~file_const_view() noexcept;
        error_type initialize(const file_const_map& map, const size_t offset, const size_t length) noexcept;
    };
    class file_view : public array_view<uint8_t>
    {
    public:
        file_view() noexcept = default;
        file_view(const file_view&) = delete;
        ~file_view() noexcept;
        error_type initialize(const file_map& map, const size_t offset, const size_t length) noexcept;
    };

	inline auto operator|(const file_share lhs, const file_share rhs) noexcept
	{
		return file_share(uint32_t(lhs) | uint32_t(rhs));
	}
	inline auto operator|(const file_access lhs, const file_access rhs) noexcept
	{
		return file_access(uint32_t(lhs) | uint32_t(rhs));
	}
	error_type enum_files(const string_view path, array<string>& files) noexcept;
	error_type enum_files_rec(const string_view path, array<string>& files) noexcept;

    struct dialog_filter 
    {
        dialog_filter() noexcept = default;
        dialog_filter(const string_view name, const string_view spec) noexcept : name(name), spec(spec)
        {

        }
        string_view name; 
        string_view spec; 
    };
    error_type dialog_open_file(string& file_name, const const_array_view<dialog_filter> filters = {}) noexcept;
    error_type dialog_save_file(string& file_name, const const_array_view<dialog_filter> filters = {}) noexcept;
    error_type dialog_select_dir(string& dir_path) noexcept;

    struct file_name
    {
		error_type initialize(const string_view file) noexcept;
        string_view directory;
        string_view name;
        string_view extension;
    };

	error_type process_directory(string& path) noexcept;
    error_type make_directory(const string_view path) noexcept;
}