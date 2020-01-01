/*
 * iFilter4Archives
 * Copyright (C) 2019  Manuel Meitinger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <Windows.h>

#include <cassert>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

namespace win32
{
    // forward declarations for string literals
    template<class CharT, class Traits = std::char_traits<CharT>>
    class basic_zstring;
    namespace literals
    {
        constexpr win32::basic_zstring<char> operator"" _sz(const char* const str, const size_t len) noexcept;
        constexpr win32::basic_zstring<wchar_t> operator"" _sz(const wchar_t* const str, const size_t len) noexcept;
    }

    // wrapper for string, wstring and literals, anything that ensures zero termination
    template<class CharT, class Traits>
    class basic_zstring
    {
    public:
        using traits_type = Traits;
        using value_type = CharT;
        using pointer = const CharT*;
        using size_type = std::size_t;

        static_assert(std::is_same_v<value_type, typename traits_type::char_type>);

    private:
        const pointer _str;
        const size_type _len;

        constexpr basic_zstring(const pointer str, const size_type len) noexcept : _str(str), _len(len) {}
        constexpr size_type length() noexcept { return _len; }

        using string = std::basic_string<value_type, traits_type>;
        using string_view = std::basic_string_view<value_type, traits_type>;

        friend constexpr basic_zstring<char> literals::operator"" _sz(const char* const, const size_t) noexcept;
        friend constexpr basic_zstring<wchar_t> literals::operator"" _sz(const wchar_t* const, const size_t) noexcept;

    public:
        constexpr basic_zstring(const basic_zstring&) noexcept = default; // for nested calls, be careful
        constexpr basic_zstring& operator=(const basic_zstring&) noexcept = delete; // too dangerous
        constexpr basic_zstring(basic_zstring&&) noexcept = default; // if passed as rvalue
        constexpr basic_zstring& operator=(basic_zstring&&) noexcept = delete; // don't use it like that

        constexpr basic_zstring(std::nullptr_t) noexcept : _str(nullptr), _len(0) {} // nullptr
        constexpr basic_zstring(const pointer str) noexcept : _str(str), _len(Traits::length(str)) {} // another C string
        basic_zstring(const string& str) noexcept : _str(str.c_str()), _len(str.length()) {} // std::string, std::wstring
        template<typename T = std::enable_if_t<std::is_same_v<CharT, wchar_t>, std::filesystem::path>>
        basic_zstring(const T& path) noexcept : _str(path.native().c_str()), _len(path.native().length()) {} // std::filesystem::path

        constexpr operator string_view() const noexcept { return string_view(_str, _len); } // to support std::string.append(czstring)
        constexpr pointer c_str() const noexcept { return _str; }
        constexpr size_type length_excluding_null_terminator() const noexcept { return _len; }
        constexpr size_type length_including_null_terminator() const noexcept { return _len + 1; }
        constexpr size_type size_excluding_null_terminator() const noexcept { return _len * sizeof(value_type); }
        constexpr size_type size_including_null_terminator() const noexcept { return (_len + 1) * sizeof(value_type); }
    };

    using czstring = basic_zstring<char>; // LPCSTR
    using czwstring = basic_zstring<wchar_t>; //LPCWSTR

    namespace literals
    {
        constexpr win32::czstring operator"" _sz(const char* const str, const size_t len) noexcept
        {
            return win32::czstring(str, len);
        }

        constexpr win32::czwstring operator"" _sz(const wchar_t* const str, const size_t len) noexcept
        {
            return win32::czwstring(str, len);
        }
    }

    /******************************************************************************/

    struct HandleDeleter
    {
        void operator()(HANDLE) noexcept;
    };
    using unique_handle_ptr = std::unique_ptr<std::remove_pointer<HANDLE>::type, HandleDeleter>;

    struct LocalMemDeleter
    {
        void operator()(void*) noexcept;
    };
    template <typename T> using unique_localmem_ptr = std::unique_ptr<T, LocalMemDeleter>;

    struct RegistryDeleter
    {
        void operator()(HKEY) noexcept;
    };
    using unique_registry_ptr = std::unique_ptr<std::remove_pointer<HKEY>::type, RegistryDeleter>;

    struct LibraryDeleter
    {
        void operator()(HMODULE) noexcept;
    };
    using unique_library_ptr = std::unique_ptr<std::remove_pointer<HMODULE>::type, LibraryDeleter>;

    /******************************************************************************/

    // guid with string representation of {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
    struct guid : public GUID
    {
        guid() noexcept;
        guid(const GUID& guid) noexcept;

        std::wstring to_wstring() const;

        static guid create();
        static guid create_sequential();
        static guid parse(std::wstring_view s);
        static bool try_parse(std::wstring_view s, guid& guid);
    };
}

/******************************************************************************/

namespace std
{
    template<>
    struct hash<GUID>
    {
        size_t operator()(const GUID& guid) const noexcept
        {
            // taken from C#
            return guid.Data1 ^ (guid.Data2 << 16 | guid.Data3) ^ (guid.Data4[2] << 24 | guid.Data4[7]);
        }
    };
}

/******************************************************************************/

namespace utils
{
    win32::unique_library_ptr get_current_module();
    std::filesystem::path get_module_file_path(HMODULE module_handle);
    std::wstring get_temp_file_name();
    std::filesystem::path get_temp_path();
    win32::unique_library_ptr load_module(win32::czwstring path);
}

/******************************************************************************/

using namespace win32::literals; // must be declared here in global scope

#define STR(s) L ## s ## _sz

#define CHR(c) L ## c

#define WIN32_DO_OR_THROW(op) \
    do { \
        if (!(op)) { const auto last_error = ::GetLastError(); WIN32_THROW(last_error); } \
    } while (0)

#define WIN32_THROW(err) \
    do { \
        assert((err) != ERROR_SUCCESS); \
        throw std::system_error((err), std::system_category()); \
    } while (0)
