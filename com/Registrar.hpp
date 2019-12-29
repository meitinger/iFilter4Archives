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

#include <optional>
#include <string>

namespace com
{
    class Registrar; // registers this iFilter and looks up other iFilters in the registry

/******************************************************************************/

    SIMPLE_CLASS_DECLARATION(Registrar,
public:
    Registrar();

    std::optional<CLSID> FindClsid(const std::wstring& extension) const; // extension must be lower-case and dot-prefixed

    static HRESULT RegisterServer() noexcept;
    static HRESULT UnregisterServer() noexcept;
    );
}
