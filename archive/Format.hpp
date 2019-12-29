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
#include "sevenzip.hpp"

#include "Module.hpp"

#include <string>
#include <unordered_set>

namespace archive
{
    class Format; // description of an instanceable 7-Zip format

    /******************************************************************************/

    SIMPLE_CLASS_DECLARATION(Format,
public:
    using ExtensionsCollection = std::unordered_set<std::wstring>;

    Format(const Module& library, UINT32 index);

    PROPERTY_READONLY(const Module&, Library, PIMPL_GETTER_ATTRIB);
    PROPERTY_READONLY(const std::wstring&, Name, PIMPL_GETTER_ATTRIB);
    PROPERTY_READONLY(const ExtensionsCollection&, Extensions, PIMPL_GETTER_ATTRIB); // guaranteed to be lower-case

    sevenzip::IInArchivePtr CreateArchive() const;
    );
}
