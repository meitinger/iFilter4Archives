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

#include <atomic>
#include <future>
#include <ios>
#include <new>
#include <stdexcept>

namespace com
{
    static std::atomic<size_t> _object_count = 0;

    static void* offset_ptr(void* ptr, ptrdiff_t offset) noexcept
    {
        return reinterpret_cast<void*>(reinterpret_cast<intptr_t>(ptr) + offset);
    }

    class unknown : public IUnknown
    {
        const std::unique_ptr<object> _object_ptr;
        const object_interface_map& _interface_map;
        std::atomic<ULONG> _ref_count = 1;

    public:

        unknown(std::unique_ptr<object> object_ptr, const object_interface_map& interface_map) noexcept :
            _object_ptr(std::move(object_ptr)),
            _interface_map(interface_map)
        {
            _object_count++;
        }

        ~unknown() noexcept
        {
            _object_count--;
        }

        unknown(const unknown&) = delete;
        unknown(unknown&&) = delete;
        unknown& operator= (const unknown&) = delete;
        unknown& operator= (unknown&&) = delete;

        STDMETHODIMP unknown::QueryInterface(REFIID riid, void** ppvObject) noexcept override
        {
            COM_CHECK_POINTER_AND_SET(ppvObject, nullptr);
            if (riid == IID_IUnknown)
            {
                *ppvObject = this;
            }
            else
            {
                const auto entry = _interface_map.find(riid); // not a noexcept, but operator== should never throw
                if (entry == _interface_map.end()) { return E_NOINTERFACE; }
                *ppvObject = offset_ptr(_object_ptr.get(), entry->second);
            }
            reinterpret_cast<IUnknown*>(*ppvObject)->AddRef(); // important, might get forwarded to outer unknown
            return S_OK;
        }

        STDMETHODIMP_(ULONG) unknown::AddRef(void) noexcept override
        {
            return ++_ref_count;
        }

        STDMETHODIMP_(ULONG) unknown::Release(void) noexcept override
        {
            const auto new_ref_count = --_ref_count;
            if (new_ref_count == 0)
            {
                delete this;
            }
            return new_ref_count;
        }
    };

    //----------------------------------------------------------------------------//

    IUnknown* object::unknown() noexcept
    {
        assert(_unknown);
        return _unknown;
    }

    object::object() noexcept = default;

    object::~object() noexcept = default;

    object::object(const object& obj) noexcept
    {
        // source may be COM
    }

    object::object(object&& obj) noexcept
    {
        assert(!obj._unknown); // source must not be COM
    }

    object& object::operator= (const object& obj) noexcept
    {
        if (this != std::addressof(obj))
        {
            assert(!_unknown); // target must not be COM
        }
        return *this;
    }

    object& object::operator= (object&& obj) noexcept
    {
        if (this != std::addressof(obj))
        {
            assert(!_unknown); // neither source ...
            assert(!obj._unknown); // ...nor target must be COM
        }
        return *this;
    }

    size_t object::count() noexcept
    {
        return _object_count;
    }

    void object::create(std::unique_ptr<com::object> object_ptr, const com::object_interface_map& interface_map, IUnknown* outer_unknown, REFIID interface_id, void** com_object)
    {
        if (!object_ptr || object_ptr->_unknown != nullptr) { throw std::invalid_argument("object_ptr"); }
        if (com_object == nullptr) { COM_THROW(E_POINTER); }
        *com_object = nullptr;

        const auto inner_object = object_ptr.get();
        if (outer_unknown != nullptr)
        {
            // aggregated, only IUnknown allowed
            if (interface_id != IID_IUnknown) { COM_THROW(E_NOINTERFACE); } // see https://docs.microsoft.com/en-us/windows/win32/com/aggregation
            inner_object->_unknown = outer_unknown; // all IUnknown calls are forwarded to the outer object
            *com_object = static_cast<IUnknown*>(new com::unknown(std::move(object_ptr), interface_map)); // the inner IUnknown is returned to the outer object
        }
        else
        {
            // non-aggregated, query the interface if it exists
            const auto entry = interface_map.find(interface_id);
            if (entry == interface_map.end()) { COM_THROW(E_NOINTERFACE); }
            inner_object->_unknown = static_cast<IUnknown*>(new com::unknown(std::move(object_ptr), interface_map)); // IUnknown is handled internally
            *com_object = offset_ptr(inner_object, entry->second); // return the pointer to the requested interface
        }
    }
}

/******************************************************************************/

namespace win32
{
    void cotaskmem_deleter::operator()(void* buffer) noexcept
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
            // custom errors (i.e. key_missing, value_missing) are just WIN32 errors with additional bits set so remove them
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
