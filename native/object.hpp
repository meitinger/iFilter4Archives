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

#include <stdexcept>
#include <type_traits>
#include <unordered_map>

namespace com
{
    class object; // base class for all COM visible classes
    using object_interface_map = std::unordered_map<IID, ptrdiff_t>; // maps a COM interface ID to the offset of the vtable within a class

    /******************************************************************************/

    template <typename T, typename U>
    inline ptrdiff_t offset_of()
    {
        constexpr const auto not_null = intptr_t(8); // compiler could optimize null-pointer out
        return reinterpret_cast<intptr_t>(static_cast<U*>(reinterpret_cast<T*>(not_null))) - not_null;
    }

    template<typename T>
    inline std::unique_ptr<com::object> make_copy(const T& other)
    {
        if constexpr (std::is_abstract_v<T>)
        {
            throw std::invalid_argument(typeid(T).name());
        }
        else
        {
            return std::make_unique<T>(other);
        }
    }

    template<typename T, typename U, typename ...Interfaces>
    inline object_interface_map make_interface_map(const object_interface_map& base_interface_map)
    {
        auto result = object_interface_map({ {__uuidof(Interfaces), offset_of<T, Interfaces>()}... });
        for (const auto& entry : base_interface_map)
        {
            if (result.find(entry.first) == result.end()) // do not override re-implemented interfaces
            {
                result[entry.first] = offset_of<T, U>() + entry.second; // adjust the offset
            }
        }
        return result;
    }

    /******************************************************************************/

    class object : public IUnknown
    {
    private:
        IUnknown* _unknown_ptr = nullptr;
        static object& make_com(std::unique_ptr<object> object_ptr, IUnknown* outer_unknown, REFIID interface_id, void*& com_object);

    protected:
        static inline const auto class_interface_map = object_interface_map({ {IID_IUnknown, offset_of<object, IUnknown>()} });
        virtual inline const object_interface_map& interface_map() const noexcept { return object::class_interface_map; }
        virtual inline std::unique_ptr<com::object> make_copy() const { return std::make_unique<object>(*this); }

    public:
        object() noexcept;
        virtual ~object() noexcept;
        object(object&&) noexcept;
        object& operator= (object&&) noexcept;
        object(const object&) noexcept;
        object& operator= (const object&) noexcept;

        static HRESULT CanUnloadNow() noexcept;

        template<class Q, typename ...Args> static Q& CreateComInstance(IUnknown* pUnkOuter, REFIID riid, void*& ppvObject, Args&&... args)
        {
            return static_cast<Q&>(make_com(std::make_unique<Q>(std::forward<Args>(args)...), pUnkOuter, riid, ppvObject));
        }

        template<class Q> inline _com_ptr_t<_com_IIID<Q, &__uuidof(Q)>> GetComInterface() const
        {
            auto ptr = _com_ptr_t<_com_IIID<Q, &__uuidof(Q)>>();
            make_com(make_copy(), nullptr, __uuidof(Q), *reinterpret_cast<void**>(&ptr));
            return ptr;
        }

        STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject);
        STDMETHOD_(ULONG, AddRef)(void);
        STDMETHOD_(ULONG, Release)(void);
    };
}

/******************************************************************************/

#define COM_CLASS_DECLARATION(className, inheritance, ...) \
    class className : public inheritance \
    { \
        private: using type = className; \
        private: using base = CLASS_BASE((inheritance)); \
        public: ~className() noexcept override; \
        public: className(className&&) noexcept; \
        public: className& operator= (className&&) noexcept; \
        public: className(const className&) noexcept; \
        public: className& operator= (const className&) noexcept; \
        protected: inline std::unique_ptr<com::object> make_copy() const override { return com::make_copy<className>(*this); } \
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
    protected: static inline const auto class_interface_map = com::make_interface_map<type, base, __VA_ARGS__>(base::class_interface_map); \
    protected: inline const com::object_interface_map& interface_map() const noexcept override { return type::class_interface_map; } \
    public: inline STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) override { return com::object::QueryInterface(riid, ppvObject); } \
    public: inline STDMETHOD_(ULONG, AddRef)(void) override { return com::object::AddRef(); } \
    public: inline STDMETHOD_(ULONG, Release)(void) override { return com::object::Release(); }
