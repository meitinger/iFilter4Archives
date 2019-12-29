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

#include <exception>
#include <memory>
#include <system_error>

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

namespace win32
{
	struct CoTaskMemDeleter
	{
		void operator()(void* buffer) noexcept;
	};
	template <typename T> using unique_cotaskmem_ptr = std::unique_ptr<T, CoTaskMemDeleter>;

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

namespace utils
{
	HRESULT hresult_from_system_error(const std::system_error&) noexcept;
}

/******************************************************************************/

#define COM_NOTHROW_BEGIN \
	try \
	{

#define COM_NOTHROW_END \
	} \
	catch (const std::bad_alloc&) { return E_OUTOFMEMORY; } \
	catch (const std::system_error& e) { return utils::hresult_from_system_error(e); } \
	catch (...) { return E_UNEXPECTED; }

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
