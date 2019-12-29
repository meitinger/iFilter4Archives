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
    static std::optional<DWORD> read_dword(win32::czwstring name)
    {
        const auto key = win32::registry_key::local_machine().open_sub_key_readonly(STR("SOFTWARE\\iFilter4Archives"));
        if (key)
        {
            return key->get_dword_value(name);
        }
        return std::nullopt;
    }

    DWORD allowed_consecutive_get_chunk_errors_before_fail()
    {
        return read_dword(STR("AllowedConsecutiveGetChunkErrorsBeforeFail")).value_or(10);
    }
    DWORD concurrent_filter_threads()
    {
        const auto value = read_dword(STR("ConcurrentFilterThreads"));
        if (value)
        {
            return *value;
        }
        return std::thread::hardware_concurrency();
    }
    bool ignore_null_persistent_handler()
    {
        return read_dword(STR("IgnoreNullPersistentHandler")).value_or(1) != 0;
    }
    bool ignore_registered_persistent_handler_if_archive()
    {
        return read_dword(STR("IgnoreRegisteredPersistentHandlerIfArchive")).value_or(0) != 0;
    }
    std::optional<DWORD> max_buffer_size() { return read_dword(STR("MaximumBufferSize")); }
    std::optional<DWORD> max_file_size() { return read_dword(STR("MaximumFileSize")); }
    std::optional<DWORD> min_available_memory() { return read_dword(STR("MinimumAvailableMemory")); }
    std::optional<DWORD> min_free_disk_space() { return read_dword(STR("MinimumFreeDiskSpace")); }
    DWORD recursion_depth_limit() { return read_dword(STR("RecursionDepthLimit")).value_or(1); }
    bool use_internal_persistent_handler_if_none_registered()
    {
        return read_dword(STR("UseInternalPersistentHandlerIfNoneRegistered")).value_or(1) != 0;
    }
}
