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

#include "com.hpp"

#include "registry.hpp"

#include <future>
#include <ios>

namespace win32
{
    void CoTaskMemDeleter::operator()(void* buffer) noexcept
    {
        ::CoTaskMemFree(reinterpret_cast<LPVOID>(buffer));
    }

    static void propvariant_move(PROPVARIANT* destination, PROPVARIANT* source) noexcept
    {
        std::memcpy(destination, source, sizeof(PROPVARIANT));
        std::memset(source, 0, sizeof(PROPVARIANT));
    }

    static void propvariant_checked_clear(PROPVARIANT* pvar) noexcept
    {
        const auto hr = ::PropVariantClear(pvar);
        assert(SUCCEEDED(hr)); // checked that we don't leak, but ignore in NDEBUG
        std::memset(pvar, 0, sizeof(PROPVARIANT));
    }

    propvariant::propvariant() noexcept
    {
        ::PropVariantInit(this);
    }

    propvariant::~propvariant() noexcept
    {
        propvariant_checked_clear(this);
    }

    propvariant::propvariant(const propvariant& other)
    {
        COM_DO_OR_THROW(::PropVariantCopy(this, &other));
    }

    propvariant::propvariant(propvariant&& other) noexcept
    {
        propvariant_move(this, &other);
    }

    propvariant& propvariant::operator=(const propvariant& rhs)
    {
        if (this != std::addressof(rhs))
        {
            clear();
            COM_DO_OR_THROW(::PropVariantCopy(this, &rhs));
        }
        return *this;
    }

    propvariant& propvariant::operator=(propvariant&& rhs) noexcept
    {
        if (this != std::addressof(rhs))
        {
            propvariant_checked_clear(this);
            propvariant_move(this, &rhs);
        }
        return *this;
    }

    void propvariant::clear()
    {
        COM_DO_OR_THROW(::PropVariantClear(this));
    }
}

/******************************************************************************/

namespace errors
{
    class com_error_category : public std::error_category
    {
        const char* name() const noexcept override
        {
            return "com";
        }

        std::string message(int condition) const override
        {
            return std::system_category().message(condition);
        }
    };

    static const auto _com_category = com_error_category();

    const std::error_category& com_category() noexcept { return _com_category; }
}

/******************************************************************************/

namespace utils
{
    HRESULT hresult_from_system_error(const std::system_error& e) noexcept
    {
        const auto& code = e.code();
        const auto& category = code.category();
        const auto value = code.value();
        if (category == errors::com_category()) { return value; } // already HRESULT
        if (category == errors::registry_category())
        {
            // custom errors (ie. key_missing, value_missing) are just WIN32 errors with additional bits set so remove them
            return HRESULT_FROM_WIN32(value & static_cast<int>(errors::registry_errc::custom) ? value & 0xFFFF : value);
        }
        if (category == std::system_category()) { return HRESULT_FROM_WIN32(value); }
        if (category == std::generic_category())
        {
            // translate generic errors to COM (but don't map too many to one HRESULT, instead use custom facility)
            switch (static_cast<std::errc>(value))
            {
            case std::errc::bad_address:             return E_POINTER;
            case std::errc::bad_file_descriptor:     return E_HANDLE;
            case std::errc::function_not_supported:  return E_NOTIMPL;
            case std::errc::interrupted:             return E_ABORT;
            case std::errc::invalid_argument:        return E_INVALIDARG;
            case std::errc::not_enough_memory:       return E_OUTOFMEMORY;
            case std::errc::not_supported:           return E_NOTIMPL;
            case std::errc::operation_canceled:      return E_ABORT;
            case std::errc::operation_in_progress:   return E_PENDING;
            case std::errc::operation_not_permitted: return E_ACCESSDENIED;
            case std::errc::operation_not_supported: return E_NOTIMPL;
            case std::errc::permission_denied:       return E_ACCESSDENIED;

            default: return 0xA0010000 | (value & 0xFFFF); // custom facility 1
            }
        }
        if (category == std::iostream_category()) { return 0xA0020000 | (value & 0xFFFF); } // custom facility 2
        if (category == std::future_category()) { return 0xA0030000 | (value & 0xFFFF); } // custom facility 3
        // TODO add upcoming error categories
        return E_FAIL;
    }
}
