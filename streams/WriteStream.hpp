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

#include "FileBuffer.hpp"

namespace streams
{
    class WriteStream; // provides an sevenzip::ISequentialOutStream writer for a FileBuffer

    /******************************************************************************/

    COM_CLASS_DECLARATION(WriteStream, (sevenzip::ISequentialOutStream),
public:
    explicit WriteStream(FileBuffer& buffer);

    STDMETHOD(Write)(const void* data, UINT32 size, UINT32* processedSize) noexcept override;
    );
}
