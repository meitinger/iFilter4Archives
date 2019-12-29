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

#include "CachedChunk.hpp"
#include "Filter.hpp"
#include "WriteStreams.hpp"

#include <memory>
#include <optional>

namespace com
{
    class ItemTask; // calls the iFilter for an item in an archive, never ~ItemTask without SetEndOfExtraction and neither Abort() or NextChunk(...) == std::nullopt

    /******************************************************************************/

    SIMPLE_CLASS_DECLARATION(ItemTask,
public:
    ItemTask(const FilterAttributes& attributes, REFCLSID filterClsid, std::unique_ptr<streams::WriteStream> writeStream, ULONG recursionDepth);

    void Run();
    void SetEndOfExtraction(HRESULT hr);
    void Abort();
    std::optional<CachedChunk> NextChunk(ULONG id);
    );
}
