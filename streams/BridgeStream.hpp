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

#include "object.hpp"
#include "sevenzip.hpp"

namespace streams
{
    class BridgeStream; // allows 7-Zip to read from ::IStream

    /******************************************************************************/

    COM_CLASS_DECLARATION(BridgeStream, com::object IMPLEMENTS(sevenzip::IInStream) IMPLEMENTS(sevenzip::IStreamGetSize),
                          COM_VISIBLE(sevenzip::IInStream, sevenzip::IStreamGetSize)
public:
    BridgeStream(IStreamPtr stream);

    STDMETHOD(Read)(void* data, UINT32 size, UINT32* processedSize);
    STDMETHOD(Seek)(INT64 offset, UINT32 seekOrigin, UINT64* newPosition);
    STDMETHOD(GetSize)(UINT64* size);
    );
}
