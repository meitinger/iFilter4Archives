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

namespace settings
{
    DWORD allowed_consecutive_get_chunk_errors_before_fail();
    DWORD concurrent_filter_threads();
    bool ignore_null_persistent_handler();
    bool ignore_registered_persistent_handler_if_archive();
    std::optional<DWORD> max_buffer_size();
    std::optional<DWORD> max_file_size();
    std::optional<DWORD> min_available_memory();
    std::optional<DWORD> min_free_disk_space();
    DWORD recursion_depth_limit();
    bool use_internal_persistent_handler_if_none_registered();
}
