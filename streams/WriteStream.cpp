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

#include "WriteStream.hpp"

namespace streams
{
    CLASS_IMPLEMENTATION(WriteStream,
                         PIMPL_CONSTRUCTOR(FileBuffer& buffer) : buffer(buffer) {}
public:
    FileBuffer buffer;
    );

    WriteStream::WriteStream(FileBuffer& buffer) : PIMPL_INIT(buffer) {}

    STDMETHODIMP WriteStream::Write(const void* data, UINT32 size, UINT32* processedSize) noexcept
    {
        COM_CHECK_POINTER(data);
        if (processedSize != nullptr) { *processedSize = 0; }
        COM_NOTHROW_BEGIN;

        const auto bytesAppended = PIMPL_(buffer).Append(data, size);
        if (processedSize != nullptr) { *processedSize = bytesAppended; }
        return bytesAppended < size ? S_FALSE : S_OK;

        COM_NOTHROW_END;
    }
}
