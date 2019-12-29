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

#include "Registrar.hpp"

#include "com.hpp"
#include "registry.hpp"
#include "settings.hpp"

#include "Factory.hpp"
#include "Filter.hpp"

#include <functional>
#include <mutex>
#include <unordered_map>

namespace com
{
	SIMPLE_CLASS_IMPLEMENTATION(Registrar,
public:
	std::mutex cacheMutex;
	std::unordered_map<std::wstring, std::optional<CLSID>> cache;
	);

	static const auto PersistentHandlerGuid = GUID{ 0x8cc8186e, 0x4618, 0x426d, { 0xb7, 0x45, 0x44, 0x42, 0xf7, 0xe7, 0xa5, 0x6a } };
	static const auto NullPersistentHandlerGuid = GUID{ 0x098f2470, 0xbae0, 0x11cd, { 0xb5, 0x79, 0x08, 0x00, 0x2b, 0x20, 0xbf, 0xeb } };

	namespace str
	{
		namespace guid
		{
			static const auto CLSID_Filter = STR("{DD88FF21-CD20-449E-B0B1-E84B1911F381}");
			static const auto IID_IFilter = STR("{89BCB740-6119-101A-BCB7-00DD010655AF}");
			static const auto PersistentHandler = STR("{8CC8186E-4618-426D-B745-4442F7E7A56A}");
		}
		static const auto Sep = STR("\\");
		static const auto CLSID = STR("CLSID");
		static const auto Software_Classes = STR("SOFTWARE\\Classes");
		static const auto Software_Classes_CLSID = STR("SOFTWARE\\Classes\\CLSID");
		static const auto PersistentHandler = STR("PersistentHandler");
		static const auto InprocServer32 = STR("InprocServer32");
		static const auto ThreadingModel = STR("ThreadingModel");
		static const auto Both = STR("Both");
		static const auto PersistentAddinsRegistered = STR("PersistentAddinsRegistered");
		static const auto iFilter4Archives = STR("iFilter4Archives");
		static const auto iFilter4Archives_persistent_handler = STR("iFilter4Archives persistent handler");
	}

	static std::optional<win32::guid> GetDefaultAsGuid(const win32::registry_key& key)
	{
		const auto string = key.get_string_value(nullptr);
		auto guid = win32::guid();
		if (string && win32::guid::try_parse(*string, guid))
		{
			return guid;
		}
		return std::nullopt;
	}

	static std::optional<win32::guid> GetPersistentHandlerGuid(win32::czwstring extension)
	{
		// open HKEY_LOCAL_MACHINE\SOFTWARE\Classes\.<ext>
		const auto classesKey = win32::registry_key::local_machine().open_sub_key_readonly(str::Software_Classes);
		if (!classesKey) { return std::nullopt; }
		const auto extKey = classesKey->open_sub_key_readonly(extension);
		if (!extKey) { return std::nullopt; }

		// check for HKEY_LOCAL_MACHINE\SOFTWARE\Classes\.<ext>\PersistentHandler
		const auto persistentHandlerKey = extKey->open_sub_key_readonly(str::PersistentHandler);
		if (persistentHandlerKey)
		{
			return GetDefaultAsGuid(*persistentHandlerKey);
		}

		// not found, try obsolete registration, get HKEY_LOCAL_MACHINE\SOFTWARE\Classes\.<ext>\(default)
		const auto extDefault = extKey->get_string_value(nullptr);
		if (!extDefault) { return std::nullopt; }

		// open HKEY_LOCAL_MACHINE\SOFTWARE\Classes\<(default)>\CLSID and get default value as application GUID
		const auto extDefaultKey = classesKey->open_sub_key_readonly
		(
			std::wstring().append(*extDefault).append(str::Sep).append(str::CLSID)
		);
		if (!extDefaultKey) { return std::nullopt; }
		const auto applicationGuid = GetDefaultAsGuid(*extDefaultKey);
		if (!applicationGuid) { return std::nullopt; }

		// open HKEY_LOCAL_MACHINE\SOFTWARE\Classes\CLSID\<ApplicationGUID>\PersistentHandler
		const auto applicationPersistentHandlerKey = classesKey->open_sub_key_readonly
		(
			std::wstring().append(str::CLSID).append(str::Sep).append(applicationGuid->to_wstring()).append(str::Sep).append(str::PersistentHandler)
		);
		if (applicationPersistentHandlerKey)
		{
			return GetDefaultAsGuid(*applicationPersistentHandlerKey);
		}

		// nothing found
		return std::nullopt;
	}

	static bool DeleteKey(const win32::registry_key& parent, win32::czwstring name, std::function<bool(win32::registry_key&)> cleanup)
	{
		{
			auto key = parent.open_sub_key_writeable(name);
			if (!key) { return true; } // already deleted
			if (!cleanup(*key) || !key->empty()) { return false; } // not everything got removed
		}
		// make sure key is out of scope and closed before deleting it
		parent.delete_sub_key(name);
		return true;
	}

	Registrar::Registrar() : PIMPL_INIT() {}

	static bool UseKnownExtension()
	{}

	static bool IsKnownExtension(const std::wstring& extension)
	{
		const auto& formats = archive::Factory::GetInstance().Formats;
		return formats.find(extension) != formats.end();
	}

	std::optional<CLSID> Registrar::FindClsid(const std::wstring& extension) const
	{
		// check the cache first
		PIMPL_LOCK_BEGIN(cacheMutex);
		const auto cachedResult = PIMPL_(cache).find(extension);
		if (cachedResult != PIMPL_(cache).end())
		{
			return cachedResult->second;
		}
		PIMPL_LOCK_END;

		// always use recursion if the extension is known and the behavior is wanted
		if (settings::ignore_registered_persistent_handler_if_archive())
		{
			if (IsKnownExtension(extension))
			{
				PIMPL_LOCK_BEGIN(cacheMutex);
				PIMPL_(cache)[extension] = __uuidof(Filter);
				PIMPL_LOCK_END;
				return __uuidof(Filter);
			}
		}

		// get the GUID of the persistant handler for the extension (ignore the null handler unless requested)
		const auto persistentHandlerGuid = GetPersistentHandlerGuid(extension);
		if (persistentHandlerGuid && (*persistentHandlerGuid != NullPersistentHandlerGuid || !settings::ignore_null_persistent_handler()))
		{
			// open HKEY_LOCAL_MACHINE\SOFTWARE\Classes\CLSID\<PersistentHandlerGUID>\PersistentAddinsRegistered\{89BCB740-6119-101A-BCB7-00DD010655AF}
			const auto key = win32::registry_key::local_machine().open_sub_key_readonly
			(
				std::wstring()
				.append(str::Software_Classes_CLSID).append(str::Sep)
				.append(persistentHandlerGuid->to_wstring()).append(str::Sep)
				.append(str::PersistentAddinsRegistered).append(str::Sep)
				.append(str::guid::IID_IFilter)
			);
			if (key)
			{
				// parse, store and return the CLSID
				const auto result = GetDefaultAsGuid(*key);
				PIMPL_LOCK_BEGIN(cacheMutex);
				PIMPL_(cache)[extension] = result;
				PIMPL_LOCK_END;
				return result;
			}
		}

		// fallback to the internal handler if requested
		if (settings::use_internal_persistent_handler_if_none_registered())
		{
			if (IsKnownExtension(extension))
			{
				PIMPL_LOCK_BEGIN(cacheMutex);
				PIMPL_(cache)[extension] = __uuidof(Filter);
				PIMPL_LOCK_END;
				return __uuidof(Filter);
			}
		}

		// cache the negative result
		PIMPL_LOCK_BEGIN(cacheMutex);
		PIMPL_(cache)[extension] = std::nullopt;
		PIMPL_LOCK_END;
		return std::nullopt;
	}

	HRESULT Registrar::RegisterServer() noexcept
	{
		COM_NOTHROW_BEGIN;

		// register filter handler
		auto filterHandlerKey = win32::registry_key::local_machine().create_sub_key_writeable
		(
			std::wstring().append(str::Software_Classes_CLSID).append(str::Sep).append(str::guid::CLSID_Filter)
		);
		filterHandlerKey.set_string_value(nullptr, str::iFilter4Archives);
		auto inprocServerKey = filterHandlerKey.create_sub_key_writeable(str::InprocServer32);
		inprocServerKey.set_string_value(nullptr, utils::get_module_file_path(utils::get_current_module().get()));
		inprocServerKey.set_string_value(str::ThreadingModel, str::Both);

		// register persistent handler
		auto persistentHandlerKey = win32::registry_key::local_machine().create_sub_key_writeable
		(
			std::wstring().append(str::Software_Classes_CLSID).append(str::Sep).append(str::guid::PersistentHandler)
		);
		persistentHandlerKey.set_string_value(nullptr, str::iFilter4Archives_persistent_handler);
		auto filterInterfaceKey = persistentHandlerKey.create_sub_key_writeable(
			std::wstring().append(str::PersistentAddinsRegistered).append(str::Sep).append(str::guid::IID_IFilter)
		);
		filterInterfaceKey.set_string_value(nullptr, str::guid::CLSID_Filter);

		// register all known extensions
		for (const auto& format : archive::Factory::GetInstance().Formats)
		{
			auto extPersistentHandlerKey = win32::registry_key::local_machine().create_sub_key_writeable
			(
				std::wstring()
				.append(str::Software_Classes).append(str::Sep)
				.append(format.first).append(str::Sep)
				.append(str::PersistentHandler)
			);
			if (!GetDefaultAsGuid(extPersistentHandlerKey)) // TODO maybe NULL filter {098f2470-bae0-11cd-b579-08002b30bfeb}
			{
				// do not override existing values
				extPersistentHandlerKey.set_string_value(nullptr, str::guid::PersistentHandler);
			}
		}

		return S_OK;
		COM_NOTHROW_END;
	}

	HRESULT Registrar::UnregisterServer() noexcept
	{
		COM_NOTHROW_BEGIN;

		// if there is no classes key everything is... fine?
		const auto classesKey = win32::registry_key::local_machine().open_sub_key_readonly(str::Software_Classes);
		if (!classesKey) { return S_OK; }

		auto everythingDeleted = true;

		// unregister all extensions
		for (const auto& format : archive::Factory::GetInstance().Formats)
		{
			{
				const auto extensionKey = classesKey->open_sub_key_readonly(format.first);
				if (!extensionKey) { continue; } // nothing to do
				// enter new scope for persistent handler key
				{
					auto persistentHandlerKey = extensionKey->open_sub_key_writeable(str::PersistentHandler);
					if (!persistentHandlerKey) { goto persistent_handler_does_not_exist; }
					const auto persistentHandler = GetDefaultAsGuid(*persistentHandlerKey);
					if (persistentHandler)
					{
						if (*persistentHandler != PersistentHandlerGuid) { continue; } // only delete our handler
						persistentHandlerKey->delete_value(nullptr);
					}
					if (!persistentHandlerKey->empty())
					{
						everythingDeleted = false; // the PersistentHandler key should have be removed
						continue;
					}
				}
				extensionKey->delete_sub_key(str::PersistentHandler);
			persistent_handler_does_not_exist:
				if (!extensionKey->empty()) { continue; } // this is actually quite likely
			}
			classesKey->delete_sub_key(format.first);
		}

		const auto clsidKey = classesKey->open_sub_key_readonly(str::CLSID);
		if (clsidKey)
		{
			// unregister persistent handler
			everythingDeleted &= DeleteKey(*clsidKey, str::guid::PersistentHandler, [](auto& persistentHandlerKey)
			{
				return DeleteKey(persistentHandlerKey, str::PersistentAddinsRegistered, [](auto& persistentAddinsKey)
				{
					return DeleteKey(persistentAddinsKey, str::guid::IID_IFilter, [](auto& filterInterfaceKey)
					{
						filterInterfaceKey.delete_value(nullptr);
						return true;
					});
				});
			});

			// unregister filter handler
			everythingDeleted &= DeleteKey(*clsidKey, str::guid::CLSID_Filter, [](auto& filterHandlerKey)
			{
				filterHandlerKey.delete_value(nullptr);
				return DeleteKey(filterHandlerKey, str::InprocServer32, [](auto& inprocServerKey)
				{
					inprocServerKey.delete_value(nullptr);
					inprocServerKey.delete_value(str::ThreadingModel);
					return true;
				});
			});
		}

		return everythingDeleted ? S_OK : S_FALSE;
		COM_NOTHROW_END;
	}
}
