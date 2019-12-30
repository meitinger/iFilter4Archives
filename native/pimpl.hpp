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

#include <cassert>
#include <memory>
#include <type_traits>

#ifdef NDEBUG
#define PIMPL struct impl; std::shared_ptr<impl> pImpl
#define PIMPL_(member) pImpl->member
#else
#define PIMPL \
    struct impl; std::shared_ptr<impl> pImpl; \
    impl* assert_pimpl() const \
    { \
        assert(pImpl); \
        return pImpl.get(); \
    }
#define PIMPL_(member) assert_pimpl()->member
#endif
#define PIMPL_IMPL(type, ...) \
    struct type::impl \
    { \
        public: impl() = default; \
        public: impl(const impl&) = delete; \
        public: impl(impl&&) = delete; \
        public: impl& operator= (const impl&) = delete; \
        public: impl& operator= (impl&&) = delete; \
        __VA_ARGS__ \
    }
#define PIMPL_CONSTRUCTOR public: impl
#define PIMPL_DECONSTRUCTOR() public: ~impl() noexcept
#ifdef NDEBUG
#define PIMPL_CAPTURE pImpl = pImpl.get()
#else
#define PIMPL_CAPTURE assert_pimpl = ([pImpl = pImpl.get()](){assert(pImpl); return pImpl;})
#endif
#define PIMPL_COPY(src) \
    do { \
        assert(src.pImpl); \
        pImpl = src.pImpl; \
    } while (0)
#define PIMPL_MOVE(src) \
    do { \
        assert(src.pImpl); \
        pImpl = std::move(src.pImpl); \
    } while (0)
#define PIMPL_INIT(...) pImpl(std::make_shared<impl>(__VA_ARGS__))
#define PIMPL_GETTER_ATTRIB const noexcept
#define PIMPL_GETTER(className, propertyType, propertyName) \
    propertyType className::Get##propertyName() PIMPL_GETTER_ATTRIB \
    { return PIMPL_(propertyName); }
#define PIMPL_SETTER_ATTRIB noexcept
#define PIMPL_SETTER(className, propertyType, propertyName) \
    void className::Set##propertyName(propertyType value) PIMPL_SETTER_ATTRIB \
    { PIMPL_(propertyName) = value; }
#define PIMPL_LOCK_BEGIN(mutexMember) \
    { \
        std::unique_lock<std::mutex> _lock_##mutexMember(PIMPL_(mutexMember))
#define PIMPL_LOCK_END \
    }
#define PIMPL_WAIT(mutexMember, cvMember, condition) PIMPL_(cvMember).wait(_lock_##mutexMember, [&] { return (condition); })

 /******************************************************************************/

#define PROPERTY_READONLY(propertyType, propertyName, getAttributes) \
    propertyType Get##propertyName() getAttributes; \
    __declspec(property(get=Get##propertyName)) propertyType propertyName

#define PROPERTY_READWRITE(propertyType, propertyName, getAttributes, setAttributes) \
    propertyType Get##propertyName() getAttributes; \
    void Set##propertyName(propertyType value) setAttributes; \
    __declspec(property(get=Get##propertyName, put=Set##propertyName)) propertyType propertyName

#define PROPERTY_WRITEONLY(propertyType, propertyName, setAttributes) \
    void Set##propertyName(propertyType value) setAttributes; \
    __declspec(property(put=Set##propertyName)) propertyType propertyName

/******************************************************************************/

#define SIMPLE_CLASS_DECLARATION(className, ...) \
    class className \
    { \
        public: ~className() noexcept; \
        public: className(className&&) noexcept; \
        public: className& operator= (className&&) noexcept; \
        public: className(const className&) noexcept; \
        public: className& operator= (const className&) noexcept; \
        private: PIMPL; \
        __VA_ARGS__ \
    }

#define SIMPLE_CLASS_IMPLEMENTATION(className, ...) \
    className::~className() noexcept = default; \
    className::className(const className& obj) noexcept { PIMPL_COPY(obj); } \
    className::className(className&& obj) noexcept { PIMPL_MOVE(obj); } \
    className& className::operator= (const className& obj) noexcept { if (this != std::addressof(obj)) { PIMPL_COPY(obj); } return *this; } \
    className& className::operator= (className&& obj) noexcept { if (this != std::addressof(obj)) { PIMPL_MOVE(obj); } return *this; } \
    PIMPL_IMPL(className, __VA_ARGS__)
