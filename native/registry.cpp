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

#include "registry.hpp"

static std::wstring build_path(std::wstring_view parent, std::wstring_view child)
{
    auto path = std::wstring();
    path.append(parent);
    if (!parent.empty() && parent.back() != CHR('\\') && !child.empty() && child.front() != CHR('\\'))
    {
        path.append(STR("\\"));
    }
    path.append(child);
    return path;
}

namespace errors
{
    class registry_error_category : public std::error_category
    {
        const char* name() const noexcept override
        {
            return "registry";
        }

        std::string message(int condition) const override
        {
            // custom errors also map to standard system error codes
            if (condition & static_cast<int>(registry_errc::custom))
            {
                condition = condition & 0xFFFF;
            }
            return std::system_category().message(condition);
        }
    };

    static const auto _registry_category = registry_error_category();

    const std::error_category& registry_category() noexcept { return _registry_category; }

    registry_error::registry_error(std::wstring_view path, LSTATUS ec) :
        _path(path),
        std::system_error(std::error_code(ec, registry_category()))
    {}

    registry_error::registry_error(std::wstring_view key, std::wstring_view name, LSTATUS ec) :
        registry_error(build_path(key, name), ec)
    {}

    const std::wstring& registry_error::path() const noexcept
    {
        return _path;
    }
}

/******************************************************************************/

namespace win32
{
#define REGISTRY_OPEN_SUB_KEY(desiredAcccess) \
    do { \
        auto key = registry_key(build_path(_path, name)); \
        auto nativeKey = HKEY(); \
        const auto error_code = ::RegOpenKeyExW(get(), name.c_str(), 0, desiredAcccess, &nativeKey); \
        key.reset(nativeKey); \
        switch (error_code) \
        { \
            case ERROR_SUCCESS: return key; \
            case ERROR_FILE_NOT_FOUND: return std::nullopt; \
            default: throw errors::registry_error(_path, name, error_code); \
        } \
    } while (0)

#define REGISTRY_CREATE_SUB_KEY(desiredAccess) \
    do { \
        auto key = registry_key(build_path(_path, name)); \
        auto nativeKey = HKEY(); \
        const auto error_code = ::RegCreateKeyExW(get(), name.c_str(), 0, nullptr, 0, desiredAccess, nullptr, &nativeKey, nullptr); \
        key.reset(nativeKey); \
        if (error_code != ERROR_SUCCESS) { throw errors::registry_error(_path, name, error_code); } \
        return key; \
    } while (0)

#define REGISTRY_DELETE(function, ec) \
    do { \
        const auto error_code = function(get(), name.c_str()); \
        switch (error_code) \
        { \
            case ERROR_SUCCESS: break; \
            case ERROR_FILE_NOT_FOUND: \
                if (throw_if_missing) { throw errors::registry_error(_path, name, static_cast<int>(ec)); } \
                break; \
            default: throw errors::registry_error(_path, name, error_code); \
        } \
    } while (0)

#define REGISTRY_SET_VALUE(type, ptr, len) \
    do { \
        const auto error_code = ::RegSetValueExW(get(), name.c_str(), 0, type, reinterpret_cast<const BYTE*>(ptr), static_cast<DWORD>(len)); \
        if (error_code != ERROR_SUCCESS) { throw errors::registry_error(_path, name, error_code); } \
    } while (0)

#define REGISTRY_ROOT(name, handle) \
    registry_key &registry_key::name() \
    { \
        static auto key = registry_key(STR(#handle), handle); \
        return key; \
    }

    registry_key::registry_key(std::wstring_view path) : _path(path) {}

    registry_key::registry_key(std::wstring_view path, HKEY handle) : _path(path), win32::unique_registry_ptr(handle) {}

    std::optional<const registry_key> registry_key::open_sub_key_readonly(czwstring name) const
    {
        REGISTRY_OPEN_SUB_KEY(KEY_READ);
    }

    std::optional<registry_key> registry_key::open_sub_key_writeable(czwstring name) const
    {
        REGISTRY_OPEN_SUB_KEY(KEY_READ | KEY_WRITE);
    }

    const registry_key registry_key::create_sub_key_readonly(czwstring name)
    {
        REGISTRY_CREATE_SUB_KEY(KEY_READ);
    }

    registry_key registry_key::create_sub_key_writeable(czwstring name)
    {
        REGISTRY_CREATE_SUB_KEY(KEY_READ | KEY_WRITE);
    }

    void registry_key::delete_sub_key(czwstring name, bool throw_if_missing) const
    {
        REGISTRY_DELETE(::RegDeleteKeyW, errors::registry_errc::key_missing);
    }

    bool registry_key::empty() const
    {
        auto sub_keys = DWORD(0);
        auto values = DWORD(0);
        const auto error_code = ::RegQueryInfoKeyW(get(), nullptr, nullptr, nullptr, &sub_keys, nullptr, nullptr, &values, nullptr, nullptr, nullptr, nullptr);
        if (error_code != ERROR_SUCCESS) { throw errors::registry_error(_path, error_code); }
        return !sub_keys && !values;
    }

    std::optional<DWORD> registry_key::get_dword_value(czwstring name) const
    {
        auto dword_value = DWORD();
        auto size_of_dword = static_cast<DWORD>(sizeof(DWORD));
        const auto error_code = ::RegGetValueW
        (
            get(),
            nullptr,
            name.c_str(),
            RRF_RT_DWORD,
            nullptr,
            reinterpret_cast<PVOID>(&dword_value),
            &size_of_dword
        );
        switch (error_code)
        {
        case ERROR_SUCCESS:
            return dword_value;
        case ERROR_FILE_NOT_FOUND:
            return std::nullopt;
        default:
            throw errors::registry_error(_path, name, error_code);
        }
    }

    std::optional<std::wstring> registry_key::get_string_value(czwstring name, bool allow_expand) const
    {
        auto string_buffer = std::vector<WCHAR>(MAX_PATH);
        while (true)
        {
            auto size_including_null_terminator = static_cast<DWORD>(string_buffer.size() * sizeof(WCHAR));
            const auto error_code = ::RegGetValueW
            (
                get(),
                nullptr,
                name.c_str(),
                RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | (!allow_expand ? RRF_NOEXPAND : 0),
                nullptr,
                reinterpret_cast<PVOID>(string_buffer.data()),
                &size_including_null_terminator
            );
            switch (error_code)
            {
            case ERROR_SUCCESS:
                return std::wstring(string_buffer.data(), (size_including_null_terminator / sizeof(WCHAR)) - 1);
            case ERROR_MORE_DATA:
                string_buffer.resize(size_including_null_terminator / sizeof(WCHAR));
                continue;
            case ERROR_FILE_NOT_FOUND:
                return std::nullopt;
            default:
                throw errors::registry_error(_path, name, error_code);
            }
        }
    }

    void registry_key::set_dword_value(czwstring name, DWORD value)
    {
        REGISTRY_SET_VALUE(REG_DWORD, &value, sizeof(DWORD));
    }

    void registry_key::set_string_value(czwstring name, czwstring value, bool is_expandable)
    {
        REGISTRY_SET_VALUE(is_expandable ? REG_EXPAND_SZ : REG_SZ, value.c_str(), value.size_including_null_terminator());
    }

    void registry_key::delete_value(czwstring name, bool throw_if_missing)
    {
        REGISTRY_DELETE(::RegDeleteValueW, errors::registry_errc::value_missing);
    }

    REGISTRY_ROOT(classes_root, HKEY_CLASSES_ROOT);
    REGISTRY_ROOT(current_user, HKEY_CURRENT_USER);
    REGISTRY_ROOT(local_machine, HKEY_LOCAL_MACHINE);
    REGISTRY_ROOT(users, HKEY_USERS);
    REGISTRY_ROOT(performance_data, HKEY_PERFORMANCE_DATA);
    REGISTRY_ROOT(current_config, HKEY_CURRENT_CONFIG);
    REGISTRY_ROOT(dyn_data, HKEY_DYN_DATA);

#undef REGISTRY_OPEN_SUB_KEY
#undef REGISTRY_CREATE_SUB_KEY
#undef REGISTRY_DELETE
#undef REGISTRY_SET_VALUE
#undef REGISTRY_ROOT

}
