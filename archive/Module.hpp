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

#include <filesystem>

namespace archive
{
    class Module; // holds a reference to a format library and pointers to its methods

    /******************************************************************************/

    SIMPLE_CLASS_DECLARATION(Module,
public:
    Module(const std::filesystem::path& path);

    PROPERTY_READONLY(const std::filesystem::path&, Path, PIMPL_GETTER_ATTRIB);

    HRESULT CreateObject(REFCLSID rclsid, REFIID riid, void** ppv) const noexcept;
    HRESULT GetNumberOfFormats(UINT32& count) const noexcept;
    HRESULT GetFormatProperty(UINT32 index, sevenzip::HandlerPropertyId propId, PROPVARIANT& value) const noexcept;
    );
}
