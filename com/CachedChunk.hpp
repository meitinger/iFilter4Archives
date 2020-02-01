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

#include <unordered_map>

namespace com
{
    class CachedChunk; // holds all information from an iFilter chunk

    /******************************************************************************/

    SIMPLE_CLASS_DECLARATION(CachedChunk,
private:
    CachedChunk();

public:
    using IdMap = std::unordered_map<ULONG, ULONG>;

    PROPERTY_READONLY(SCODE, Code, const noexcept);

    SCODE GetChunk(STAT_CHUNK* pStat) noexcept;
    SCODE GetText(ULONG* pcwcBuffer, WCHAR* awcBuffer) noexcept;
    SCODE GetValue(PROPVARIANT** ppPropValue) noexcept;

    void Map(ULONG newId, IdMap& idMap);

    static CachedChunk FromFilter(IFilter* filter);
    static CachedChunk FromHResult(HRESULT hr);
    );
}
