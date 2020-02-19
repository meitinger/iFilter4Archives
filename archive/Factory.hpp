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

#include "Format.hpp"

#include <string>
#include <unordered_map>

namespace archive
{
    class Factory; // loads all 7-Zip modules and provides a format lookup based on file extensions

    /******************************************************************************/

    CLASS_DECLARATION(Factory,
private:
    Factory();

public:
    using FormatsCollection = std::unordered_map<std::wstring, Format>;

    PROPERTY_READONLY(const FormatsCollection&, Formats, PIMPL_GETTER_ATTRIB);

    static const Factory& GetInstance(); // sadly, there are no static properties
    static sevenzip::IInArchivePtr CreateArchiveFromExtension(const std::wstring& extension); // extension must be lower-case and dot-prefixed
    );
}
