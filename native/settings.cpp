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

#include "settings.hpp"

#include "registry.hpp"

#include <thread>

namespace settings
{
    static DWORD read_dword(win32::czwstring name, DWORD default_value)
    {
        const auto key = win32::registry_key::local_machine().open_sub_key_readonly(STR("SOFTWARE\\iFilter4Archives"));
        return key ? key->get_dword_value(name).value_or(default_value) : default_value;
    }

    DWORD concurrent_filter_threads()
    {
        return read_dword(STR("ConcurrentFilterThreads"), std::thread::hardware_concurrency());
    }

    bool ignore_null_persistent_handler()
    {
        return read_dword(STR("IgnoreNullPersistentHandler"), 1);
    }

    bool ignore_registered_persistent_handler_if_archive()
    {
        return read_dword(STR("IgnoreRegisteredPersistentHandlerIfArchive"), 0);
    }

    ULONGLONG maximum_file_size()
    {
        return read_dword(STR("MaximumFileSize"), 16) * 1048576ull; // should be equal to MaxDownloadSize
    }

    SIZE_T maximum_buffer_size()
    {
        return read_dword(STR("MaximumBufferSize"), 4194304); // should harmonize with FilterProcessMemoryQuota
    }

    DWORD recursion_depth_limit()
    {
        return read_dword(STR("RecursionDepthLimit"), 1);
    }

    bool use_internal_persistent_handler_if_none_registered()
    {
        return read_dword(STR("UseInternalPersistentHandlerIfNoneRegistered"), 1);
    }
}
