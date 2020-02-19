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

#include "pimpl.hpp"
#include "win32.hpp"

#include "FileDescription.hpp"

namespace streams
{
    class FileBuffer; // memory or disk-backed buffer for extracted files

    /******************************************************************************/

    CLASS_DECLARATION(FileBuffer,
public:
    explicit FileBuffer(const com::FileDescription& description);

    PROPERTY_READONLY(const com::FileDescription&, Description, PIMPL_GETTER_ATTRIB);

    ULONG Append(const void* buffer, ULONG count); // tries to write the most bytes
    ULONG Read(ULONGLONG offset, void* buffer, ULONG count) const; // tries to write the most bytes
    void SetEndOfFile(); // will not call COM
    );
}
