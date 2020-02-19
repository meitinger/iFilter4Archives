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

#include "win32.hpp"

#include <cassert>
#include <memory>
#include <new>
#include <system_error>
#include <type_traits>
#include <unordered_map>

#include <comip.h>
#include <comdef.h>
#include <comdefsp.h>
#include <objbase.h>
#include <PropIdl.h>

 // included here for others
#pragma comment(lib, "Propsys")
#include <propsys.h>
#include <propvarutil.h>
#include <Filter.h>
#include <Filterr.h>
_COM_SMARTPTR_TYPEDEF(IFilter, IID_IFilter);
_COM_SMARTPTR_TYPEDEF(IInitializeWithStream, IID_IInitializeWithStream);

namespace com
{
    using object_interface_map = std::unordered_map<IID, ptrdiff_t>;

    class object : public IUnknown
    {
    private:
        IUnknown* _unknown = nullptr;

    protected:
        IUnknown* unknown() noexcept;

    public:
        object() noexcept;
        virtual ~object() noexcept = 0;
        object(const object&) noexcept;
        object(object&&) noexcept;
        object& operator= (const object&) noexcept;
        object& operator= (object&&) noexcept;

        static size_t count() noexcept;
        static void create(std::unique_ptr<com::object> object_ptr, const com::object_interface_map& interface_map, IUnknown* outer_unknown, REFIID interface_id, void** com_object);
    };

    template<typename...Interfaces>
    struct interfaces : public Interfaces... { };
}

/******************************************************************************/

namespace win32
{
    struct cotaskmem_deleter
    {
        void operator()(void* buffer) noexcept;
    };
    template <typename T> using unique_cotaskmem_ptr = std::unique_ptr<T, cotaskmem_deleter>;

    struct propvariant : public PROPVARIANT
    {
        propvariant() noexcept;
        ~propvariant() noexcept;
        propvariant(const propvariant&);
        propvariant(propvariant&&) noexcept;
        propvariant& operator=(const propvariant&);
        propvariant& operator=(propvariant&&) noexcept;
        void clear();
    };
}

/******************************************************************************/

namespace errors
{
    const std::error_category& com_category() noexcept;
}

/******************************************************************************/

#define COM_CLASS \
    public: STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) noexcept override { return unknown()->QueryInterface(riid, ppvObject); } \
    public: STDMETHODIMP_(ULONG) AddRef(void) noexcept override { return unknown()->AddRef(); } \
    public: STDMETHODIMP_(ULONG) Release(void) noexcept override { return unknown()->Release(); } \
    public: template<class Interface, typename ...Args> static _com_ptr_t<_com_IIID<Interface, &__uuidof(Interface)>> CreateComInstance(Args&&... args) \
    { \
        static_assert(std::is_base_of_v<Interface, self>); \
        auto ptr = _com_ptr_t<_com_IIID<Interface, &__uuidof(Interface)>>(); \
        com::object::create(std::make_unique<self>(std::forward<Args>(args)...), self::interface_map, nullptr, __uuidof(Interface), reinterpret_cast<void**>(&ptr)); \
        return ptr; \
    }

#define COM_VISIBLE(...) \
    public: static inline const auto interface_map = utils::make_interface_map<self, __VA_ARGS__>();

#define COM_CLASS_DECLARATION(className, comInterfaces, ...) \
    CLASS_DECLARATION_EXTENDS(className, (public com::object, public com::interfaces<_CLASS_INLINE comInterfaces>), COM_VISIBLE comInterfaces COM_CLASS private: __VA_ARGS__)

#define COM_CLASS_DECLARATION_EXTENDS(className, baseClasses, comInterfaces, ...) \
    CLASS_DECLARATION_EXTENDS(className, (_CLASS_INLINE baseClasses, public com::object, public com::interfaces<_CLASS_INLINE comInterfaces>), COM_VISIBLE comInterfaces COM_CLASS private: __VA_ARGS__)

#define COM_NOTHROW_BEGIN \
    try \
    {

#define COM_NOTHROW_END \
    } \
    catch (const std::bad_alloc&) { return E_OUTOFMEMORY; } \
    catch (const std::system_error& e) { return utils::hresult_from_system_error(e); } \
    catch (...) { return E_UNEXPECTED; }

#define COM_THREAD_BEGIN(coinit) \
    try \
    { \
        COM_DO_OR_THROW(::CoInitializeEx(nullptr, (coinit))); \
        {

#define COM_THREAD_END(hr) \
        } \
        ::CoUninitialize(); \
    } \
    catch (const std::bad_alloc&) { (hr) = E_OUTOFMEMORY; } \
    catch (const std::system_error& e) { (hr) = utils::hresult_from_system_error(e); } \
    catch (...) { (hr) = E_UNEXPECTED; } \

#define COM_CHECK_ARG(check) \
    do { \
        if (!(check)) { return E_INVALIDARG; } \
    } while (0)

#define COM_CHECK_POINTER(ptr) \
    do { \
        if ((ptr) == nullptr) { return E_POINTER; } \
    } while (0)

#define COM_CHECK_POINTER_AND_SET(ptr, val) \
    do { \
        if ((ptr) == nullptr) { return E_POINTER; } \
        *(ptr) = (val); \
    } while (0)

#define COM_CHECK_STATE(check) \
    do { \
        if (!(check)) { return E_NOT_VALID_STATE; } \
    } while (0)

#define COM_LAST_WIN32_ERROR \
    (HRESULT_FROM_WIN32(::GetLastError()) | 0x80000000) // always ensure that it is an error

#define COM_DO_OR_THROW(op) \
    do { \
        const auto hr = (op); \
        if (FAILED(hr)) { throw std::system_error(hr, errors::com_category()); } \
    } while (0)

#define COM_THROW(hr) \
    do { \
        assert(FAILED(hr)); \
        throw std::system_error(hr, errors::com_category()); \
    } while (0)

#define COM_DO_OR_RETURN(op) \
    do { \
        const auto hr = (op); \
        if (FAILED(hr)) { return hr; } \
    } while (0)

#define WIN32_DO_OR_RETURN(op) \
    do { \
        if (!(op)) { return COM_LAST_WIN32_ERROR; } \
    } while (0)

/******************************************************************************/

namespace utils
{
    HRESULT hresult_from_system_error(const std::system_error&) noexcept;

    template<class Type, typename ...Args>
    static HRESULT make_com(IUnknown* pUnkOuter, REFIID riid, void** ppvObject, Args&&... args) noexcept
    {
        COM_NOTHROW_BEGIN;
        com::object::create(std::make_unique<Type>(std::forward<Args>(args)...), Type::interface_map, pUnkOuter, riid, ppvObject);
        return S_OK;
        COM_NOTHROW_END;
    }

    template <typename Type, typename... Interfaces>
    com::object_interface_map make_interface_map()
    {
        return com::object_interface_map({ {__uuidof(Interfaces), offset_of_interface<Type, Interfaces>()}... });
    }

    template <typename Type, typename Interface>
    constexpr ptrdiff_t offset_of_interface()
    {
        static_assert(std::is_base_of_v<Interface, Type>);
        constexpr const auto not_null = intptr_t(0x00400000); // compiler could optimize null-pointer out
        return reinterpret_cast<intptr_t>(static_cast<Interface*>(static_cast<Type*>(reinterpret_cast<com::object*>(not_null)))) - not_null;
    }
}
