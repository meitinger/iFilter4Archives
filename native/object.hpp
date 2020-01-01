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

#include "com.hpp"
#include "pimpl.hpp"

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

namespace com
{
    class object; // base class for all COM visible classes
    using object_interface_map = std::unordered_map<IID, ptrdiff_t>; // maps a COM interface ID to the offset of the vtable within a class

    /******************************************************************************/

    template <typename Type, typename Interface>
    ptrdiff_t offset_of_interface()
    {
        constexpr const auto not_null = intptr_t(8); // compiler could optimize null-pointer out
        return reinterpret_cast<intptr_t>(static_cast<Interface*>(static_cast<Type*>(reinterpret_cast<object*>(not_null)))) - not_null;
    }

    template<typename T>
    std::unique_ptr<com::object> make_copy(const T& other)
    {
        if constexpr (std::is_abstract_v<T>) { throw std::invalid_argument(typeid(T).name()); }
        else { return std::make_unique<T>(other); }
    }

    template<typename T, typename ...Interfaces>
    object_interface_map make_interface_map(const object_interface_map& base_interface_map)
    {
        auto result = object_interface_map({ {__uuidof(Interfaces), offset_of_interface<T, Interfaces>()}... }); // calculate all offsets
        result.merge(object_interface_map(base_interface_map)); // add all non-hidden base interfaces
        return result;
    }

    /******************************************************************************/

    class object : public IUnknown
    {
    private:
        IUnknown* _unknown_ptr = nullptr;
        static HRESULT make_com(std::unique_ptr<object> object_ptr, IUnknown* outer_unknown, REFIID interface_id, void** com_object); // also throws despite HRESULT

    protected:
        static inline const auto class_interface_map = object_interface_map({ {IID_IUnknown, offset_of_interface<object, IUnknown>()} });
        virtual const object_interface_map& interface_map() const noexcept { return object::class_interface_map; }
        virtual std::unique_ptr<com::object> make_copy() const { return std::make_unique<object>(*this); }

        template<class Type, class Interface, typename ...Args>
        static _com_ptr_t<_com_IIID<Interface, &__uuidof(Interface)>> create_com_instance(Args&&... args)
        {
            static_assert(std::is_base_of_v<Interface, Type>);
            auto ptr = _com_ptr_t<_com_IIID<Interface, &__uuidof(Interface)>>();
            COM_DO_OR_THROW(make_com(std::make_unique<Type>(std::forward<Args>(args)...), nullptr, __uuidof(Interface), reinterpret_cast<void**>(&ptr)));
            return ptr;
        }

    public:
        object() noexcept;
        virtual ~object() noexcept;
        object(object&&) noexcept;
        object& operator= (object&&) noexcept;
        object(const object&) noexcept;
        object& operator= (const object&) noexcept;

        static HRESULT CanUnloadNow() noexcept;

        template<class Type, typename ...Args>
        static HRESULT CreateComInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject, Args&&... args) noexcept
        {
            COM_NOTHROW_BEGIN; // make_com and make_unique may throw
            return make_com(std::make_unique<Type>(std::forward<Args>(args)...), pUnkOuter, riid, ppvObject);
            COM_NOTHROW_END;
        }

        template<class Interface>
        _com_ptr_t<_com_IIID<Interface, &__uuidof(Interface)>> GetComInterface() const
        {
            auto ptr = _com_ptr_t<_com_IIID<Interface, &__uuidof(Interface)>>();
            COM_DO_OR_THROW(make_com(make_copy(), nullptr, __uuidof(Interface), reinterpret_cast<void**>(&ptr)));
            return ptr;
        }

        STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) noexcept override;
        STDMETHOD_(ULONG, AddRef)(void) noexcept override;
        STDMETHOD_(ULONG, Release)(void) noexcept override;
    };
}

/******************************************************************************/

#define COM_CLASS_DECLARATION(className, inheritance, ...) \
    class className : public inheritance \
    { \
        private: using self = className; \
        private: using base = CLASS_BASE((inheritance)); \
        public: ~className() noexcept override; \
        public: className(className&&) noexcept; \
        public: className& operator= (className&&) noexcept; \
        public: className(const className&) noexcept; \
        public: className& operator= (const className&) noexcept; \
        protected: std::unique_ptr<com::object> make_copy() const override { return com::make_copy<className>(*this); } \
        public: template<class Interface, typename ...Args> static _com_ptr_t<_com_IIID<Interface, &__uuidof(Interface)>> CreateComInstance(Args&&... args) { return com::object::create_com_instance<className, Interface, Args...>(std::forward<Args>(args)...); } \
        private: PIMPL; \
        __VA_ARGS__ \
    }

#define COM_CLASS_IMPLEMENTATION(className, ...) \
    className::~className() noexcept = default; \
    className::className(const className& obj) noexcept : base(obj) { PIMPL_COPY(obj); } \
    className::className(className&& obj) noexcept : base(std::move(obj)) { PIMPL_MOVE(obj); } \
    className& className::operator= (const className& obj) noexcept { if (this != std::addressof(obj)) { PIMPL_COPY(obj); } return *this; } \
    className& className::operator= (className&& obj) noexcept { if (this != std::addressof(obj)) { PIMPL_MOVE(obj); } return *this; } \
    PIMPL_IMPL(className, __VA_ARGS__)

#define _CLASS_BASE(baseClass, ...) baseClass
#define CLASS_BASE(inheritance) _CLASS_BASE inheritance

#define IMPLEMENTS(interfaceClass) \
    , public interfaceClass

/******************************************************************************/

#define COM_VISIBLE(...) \
    protected: static inline const auto class_interface_map = com::make_interface_map<self, __VA_ARGS__>(base::class_interface_map); \
    protected: const com::object_interface_map& interface_map() const noexcept override { return self::class_interface_map; } \
    public: STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) noexcept override { return com::object::QueryInterface(riid, ppvObject); } \
    public: STDMETHOD_(ULONG, AddRef)(void) noexcept override { return com::object::AddRef(); } \
    public: STDMETHOD_(ULONG, Release)(void) noexcept override { return com::object::Release(); }
