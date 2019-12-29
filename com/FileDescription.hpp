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
#include "sevenzip.hpp"

#include <string>

namespace com
{
    class FileDescription; // stores basic information for (compressed) files 

    /******************************************************************************/

    SIMPLE_CLASS_DECLARATION(FileDescription,
private:
    FileDescription();

public:
    PROPERTY_READONLY(const std::wstring&, Name, PIMPL_GETTER_ATTRIB);
    PROPERTY_READONLY(const std::wstring&, Extension, const); // guaranteed to be lower-case and dot-prefixed
    PROPERTY_READONLY(bool, IsDirectory, PIMPL_GETTER_ATTRIB);
    PROPERTY_READONLY(ULONGLONG, Size, PIMPL_GETTER_ATTRIB);
    PROPERTY_READONLY(bool, SizeIsValid, const noexcept);
    PROPERTY_READONLY(FILETIME, ModificationTime, PIMPL_GETTER_ATTRIB);
    PROPERTY_READONLY(FILETIME, CreationTime, PIMPL_GETTER_ATTRIB);
    PROPERTY_READONLY(FILETIME, AccessTime, PIMPL_GETTER_ATTRIB);

    HRESULT ToStat(STATSTG* stat, bool includeName) const noexcept;

    static FileDescription FromArchiveItem(sevenzip::IInArchive* archive, UINT32 index);
    static FileDescription FromIStream(IStream* stream);
    );
}
