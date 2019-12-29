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

#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace errors
{
	enum class registry_errc
	{
		custom = 0x20000000,
		key_missing = 0x20000002,
		value_missing = 0x20010002,
	};

	const std::error_category& registry_category() noexcept;

	class registry_error : public std::system_error
	{
	private:
		const std::wstring _path;

	public:
		registry_error(std::wstring_view path, LSTATUS ec);
		registry_error(std::wstring_view key, std::wstring_view name, LSTATUS ec);
		const std::wstring& path() const noexcept;
	};
}

/******************************************************************************/

namespace win32
{
	class registry_key : private win32::unique_registry_ptr
	{
	private:
		const std::wstring _path;
		registry_key(std::wstring_view path);
		registry_key(std::wstring_view path, HKEY handle);

	public:
		std::optional<const registry_key> open_sub_key_readonly(czwstring name) const;
		std::optional<registry_key> open_sub_key_writeable(czwstring name) const;
		const registry_key create_sub_key_readonly(czwstring name);
		registry_key create_sub_key_writeable(czwstring name);
		void delete_sub_key(czwstring name, bool throw_if_missing = true) const; // const because ::RegDeleteKey is not affected by the rights of the current key
		bool empty() const;
		std::optional<DWORD> get_dword_value(czwstring name) const;
		std::optional<std::wstring> get_string_value(czwstring name, bool allow_expand = true) const;
		void set_dword_value(czwstring name, DWORD value);
		void set_string_value(czwstring name, czwstring value, bool is_expandable = false);
		void delete_value(czwstring name, bool throw_if_missing = true);

		static registry_key& classes_root();
		static registry_key& current_user();
		static registry_key& local_machine();
		static registry_key& users();
		static registry_key& performance_data();
		static registry_key& current_config();
		static registry_key& dyn_data();
	};
}
