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

#include "registry.hpp"
#include "settings.hpp"

#include "Factory.hpp"
#include "Filter.hpp"

#include <functional>
#include <mutex>
#include <unordered_map>

namespace com
{
    CLASS_IMPLEMENTATION(Registrar,
public:
    std::mutex cacheMutex;
    std::unordered_map<std::wstring, std::optional<CLSID>> cache;
    );

    constexpr static const auto PersistentHandlerGuid = GUID{ 0x8cc8186e, 0x4618, 0x426d, { 0xb7, 0x45, 0x44, 0x42, 0xf7, 0xe7, 0xa5, 0x6a } };
    constexpr static const auto NullPersistentHandlerGuid = GUID{ 0x098f2470, 0xbae0, 0x11cd, { 0xb5, 0x79, 0x08, 0x00, 0x2b, 0x30, 0xbf, 0xeb } };

    namespace str
    {
        namespace guid
        {
            constexpr static const auto CLSID_Filter = STR("{DD88FF21-CD20-449E-B0B1-E84B1911F381}");
            constexpr static const auto IID_IFilter = STR("{89BCB740-6119-101A-BCB7-00DD010655AF}");
            constexpr static const auto PersistentHandler = STR("{8CC8186E-4618-426D-B745-4442F7E7A56A}");
            constexpr static const auto NullPersistentHandler = STR("{098f2470-bae0-11cd-b579-08002b30bfeb}"); // not capitalized on purpose
        }
        constexpr static const auto Sep = STR("\\");
        constexpr static const auto Empty = STR("");
        constexpr static const auto CLSID = STR("CLSID");
        constexpr static const auto Software_Classes = STR("SOFTWARE\\Classes");
        constexpr static const auto Software_Classes_CLSID = STR("SOFTWARE\\Classes\\CLSID");
        constexpr static const auto PersistentHandler = STR("PersistentHandler");
        constexpr static const auto HasNullFilter = STR("HasNullFilter");
        constexpr static const auto InprocServer32 = STR("InprocServer32");
        constexpr static const auto ThreadingModel = STR("ThreadingModel");
        constexpr static const auto Both = STR("Both");
        constexpr static const auto PersistentAddinsRegistered = STR("PersistentAddinsRegistered");
        constexpr static const auto iFilter4Archives = STR("iFilter4Archives");
        constexpr static const auto iFilter4Archives_persistent_handler = STR("iFilter4Archives persistent handler");
    }

    static std::optional<win32::guid> GetDefaultAsGuid(const win32::registry_key& key)
    {
        const auto string = key.get_string_value(nullptr);
        auto guid = win32::guid();
        if (string && win32::guid::try_parse(*string, guid)) { return guid; }
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
        if (persistentHandlerKey) { return GetDefaultAsGuid(*persistentHandlerKey); }

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
        if (applicationPersistentHandlerKey) { return GetDefaultAsGuid(*applicationPersistentHandlerKey); }

        // nothing found
        return std::nullopt;
    }

    static bool CleanupAndDeleteIfEmpty(const win32::registry_key& parent, win32::czwstring name, const win32::transaction& transaction, std::function<bool(win32::registry_key&, const win32::transaction&)> cleanup)
    {
        {
            auto key = parent.open_sub_key_writeable_transacted(name, transaction);
            if (!key) { return true; } // already deleted
            if (!cleanup(*key, transaction) || !key->empty()) { return false; } // not everything got removed
        }
        // make sure key is out of scope and closed before deleting it
        parent.delete_sub_key_transacted(name, transaction);
        return true;
    }

    Registrar::Registrar() : PIMPL_INIT() {}

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
        if (cachedResult != PIMPL_(cache).end()) { return cachedResult->second; }
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

        // get the GUID of the persistent handler for the extension (ignore the null handler unless requested)
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

        // fall-back to the internal handler if requested
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

        auto transaction = win32::transaction(STR("register iFilter4Archives"));

        // register filter handler
        auto filterHandlerKey = win32::registry_key::local_machine().create_sub_key_writeable_transacted
        (
            std::wstring().append(str::Software_Classes_CLSID).append(str::Sep).append(str::guid::CLSID_Filter),
            transaction
        );
        filterHandlerKey.set_string_value(nullptr, str::iFilter4Archives);
        auto inprocServerKey = filterHandlerKey.create_sub_key_writeable_transacted(str::InprocServer32, transaction);
        inprocServerKey.set_string_value(nullptr, utils::get_module_file_path(utils::get_current_module().get()));
        inprocServerKey.set_string_value(str::ThreadingModel, str::Both);

        // register persistent handler
        auto persistentHandlerKey = win32::registry_key::local_machine().create_sub_key_writeable_transacted
        (
            std::wstring().append(str::Software_Classes_CLSID).append(str::Sep).append(str::guid::PersistentHandler),
            transaction
        );
        persistentHandlerKey.set_string_value(nullptr, str::iFilter4Archives_persistent_handler);
        auto filterInterfaceKey = persistentHandlerKey.create_sub_key_writeable_transacted(
            std::wstring().append(str::PersistentAddinsRegistered).append(str::Sep).append(str::guid::IID_IFilter),
            transaction
        );
        filterInterfaceKey.set_string_value(nullptr, str::guid::CLSID_Filter);

        // register all known extensions
        for (const auto& format : archive::Factory::GetInstance().Formats)
        {
            auto extPersistentHandlerKey = win32::registry_key::local_machine().create_sub_key_writeable_transacted
            (
                std::wstring()
                .append(str::Software_Classes).append(str::Sep)
                .append(format.first).append(str::Sep)
                .append(str::PersistentHandler),
                transaction
            );
            const auto existingHandler = GetDefaultAsGuid(extPersistentHandlerKey);
            if (existingHandler)
            {
                // do not override existing handlers unless it's the NULL filter
                if (*existingHandler != NullPersistentHandlerGuid) { continue; }
                extPersistentHandlerKey.set_string_value(str::HasNullFilter, str::Empty);
            }
            extPersistentHandlerKey.set_string_value(nullptr, str::guid::PersistentHandler);
        }

        transaction.commit();
        return S_OK;

        COM_NOTHROW_END;
    }

    HRESULT Registrar::UnregisterServer() noexcept
    {
        COM_NOTHROW_BEGIN;

        auto transaction = win32::transaction(STR("unregister iFilter4Archives"));

        // if there is no classes key everything is... fine?
        const auto classesKey = win32::registry_key::local_machine().open_sub_key_readonly_transacted(str::Software_Classes, transaction);
        if (!classesKey) { return S_OK; }

        auto everythingDeleted = true;

        // unregister all extensions
        for (const auto& format : archive::Factory::GetInstance().Formats)
        {
            {
                const auto extensionKey = classesKey->open_sub_key_readonly_transacted(format.first, transaction);
                if (!extensionKey) { continue; } // nothing to do
                if (!CleanupAndDeleteIfEmpty(*extensionKey, str::PersistentHandler, transaction, [](auto& persistentHandlerKey, const auto& transaction)
                {
                    const auto hasNullFilter = !!persistentHandlerKey.get_string_value(str::HasNullFilter);
                    if (hasNullFilter)
                    {
                        persistentHandlerKey.delete_value(str::HasNullFilter);
                    }
                    const auto persistentHandler = GetDefaultAsGuid(persistentHandlerKey);
                    if (persistentHandler)
                    {
                        if (*persistentHandler != PersistentHandlerGuid) { return false; } // only delete our handler
                        if (hasNullFilter)
                        {
                            persistentHandlerKey.set_string_value(nullptr, str::guid::NullPersistentHandler);
                        }
                        else
                        {
                            persistentHandlerKey.delete_value(nullptr);
                        }
                    }
                    return true;
                }))
                {
                    everythingDeleted = false;
                    continue;
                }
                if (!extensionKey->empty()) { continue; } // this is actually quite likely
            }
            classesKey->delete_sub_key_transacted(format.first, transaction);
        }

        const auto clsidKey = classesKey->open_sub_key_readonly_transacted(str::CLSID, transaction);
        if (clsidKey)
        {
            // unregister persistent handler
            everythingDeleted &= CleanupAndDeleteIfEmpty(*clsidKey, str::guid::PersistentHandler, transaction, [](auto& persistentHandlerKey, const auto& transaction)
            {
                return CleanupAndDeleteIfEmpty(persistentHandlerKey, str::PersistentAddinsRegistered, transaction, [](auto& persistentAddinsKey, const auto& transaction)
                {
                    return CleanupAndDeleteIfEmpty(persistentAddinsKey, str::guid::IID_IFilter, transaction, [](auto& filterInterfaceKey, const auto& transaction)
                    {
                        filterInterfaceKey.delete_value(nullptr);
                        return true;
                    });
                });
            });

            // unregister filter handler
            everythingDeleted &= CleanupAndDeleteIfEmpty(*clsidKey, str::guid::CLSID_Filter, transaction, [](auto& filterHandlerKey, const auto& transaction)
            {
                filterHandlerKey.delete_value(nullptr);
                return CleanupAndDeleteIfEmpty(filterHandlerKey, str::InprocServer32, transaction, [](auto& inprocServerKey, const auto& transaction)
                {
                    inprocServerKey.delete_value(nullptr);
                    inprocServerKey.delete_value(str::ThreadingModel);
                    return true;
                });
            });
        }

        transaction.commit();
        return everythingDeleted ? S_OK : S_FALSE;

        COM_NOTHROW_END;
    }
}
