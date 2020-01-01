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

#include "object.hpp"

#include <atomic>
#include <cassert>

namespace com
{
    static std::atomic<ULONG> _com_object_count = 0;

    static void* offset_ptr(void* ptr, ptrdiff_t offset) noexcept
    {
        return reinterpret_cast<void*>(reinterpret_cast<intptr_t>(ptr) + offset);
    }

    /******************************************************************************/

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
            _com_object_count++;
        }

        ~unknown() noexcept
        {
            _com_object_count--;
        }

        unknown(const unknown&) = delete;
        unknown(unknown&&) = delete;
        unknown& operator= (const unknown&) = delete;
        unknown& operator= (unknown&&) = delete;

        STDMETHODIMP unknown::QueryInterface(REFIID riid, void** ppvObject) noexcept override
        {
            COM_CHECK_POINTER_AND_SET(ppvObject, nullptr);
            const auto entry = _interface_map.find(riid); // not a noexcept, but operator== should never throw
            if (entry == _interface_map.end()) { return E_NOINTERFACE; }
            *ppvObject = offset_ptr(_object_ptr.get(), entry->second);
            _object_ptr->AddRef(); // important, might get forwarded to outer unknown
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

    /******************************************************************************/

    HRESULT object::make_com(std::unique_ptr<object> object_ptr, IUnknown* outer_unknown, REFIID interface_id, void** com_object)
    {
        assert(object_ptr); // it's a private function, but do an assert anyway
        COM_CHECK_POINTER_AND_SET(com_object, nullptr);

        const auto& map = object_ptr->interface_map();
        auto inner_object = object_ptr.get();
        if (outer_unknown != nullptr)
        {
            // aggregated, only IUnknown allowed
            if (interface_id != IID_IUnknown) { return E_NOINTERFACE; } // see https://docs.microsoft.com/en-us/windows/win32/com/aggregation
            inner_object->_unknown_ptr = outer_unknown; // all IUnknown calls are forwarded to the outer object
            *com_object = static_cast<IUnknown*>(new unknown(std::move(object_ptr), map)); // the inner IUnknown is returned to the outer object
        }
        else
        {
            // non-aggregated, query the interface if it exists
            const auto entry = map.find(interface_id);
            if (entry == map.end()) { return E_NOINTERFACE; }
            inner_object->_unknown_ptr = static_cast<IUnknown*>(new unknown(std::move(object_ptr), map)); // IUnknown is handled internally
            *com_object = offset_ptr(inner_object, entry->second); // return the pointer to the requested interface
        }
        return S_OK;
    }

    object::object() noexcept = default;

    object::~object() noexcept = default;

    object::object(const object& obj) noexcept
    {
        // source may be COM
    }

    object::object(object&& obj) noexcept
    {
        assert(!obj._unknown_ptr); // source must not be COM
    }

    object& object::operator= (const object& obj) noexcept
    {
        if (this != std::addressof(obj))
        {
            assert(!_unknown_ptr); // target must not be COM
        }
        return *this;
    }

    object& object::operator= (object&& obj) noexcept
    {
        if (this != std::addressof(obj))
        {
            assert(!_unknown_ptr); // neither source ...
            assert(!obj._unknown_ptr); // ...nor target must be COM
        }
        return *this;
    }

    HRESULT object::CanUnloadNow() noexcept
    {
        return _com_object_count == 0 ? S_OK : S_FALSE;
    }

    STDMETHODIMP object::QueryInterface(REFIID riid, void** ppvObject) noexcept
    {
        assert(_unknown_ptr);
        return _unknown_ptr->QueryInterface(riid, ppvObject);
    }

    STDMETHODIMP_(ULONG) object::AddRef(void) noexcept
    {
        assert(_unknown_ptr);
        return _unknown_ptr->AddRef();
    }

    STDMETHODIMP_(ULONG) object::Release(void) noexcept
    {
        assert(_unknown_ptr);
        return _unknown_ptr->Release();
    }
}
