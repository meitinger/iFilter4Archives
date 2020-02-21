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

#include "BridgeStream.hpp"

#include <stdexcept>

namespace streams
{
    CLASS_IMPLEMENTATION(BridgeStream,
public:
    IStreamPtr stream;
    );

    BridgeStream::BridgeStream(IStreamPtr stream) : PIMPL_INIT()
    {
        if (!stream) { throw std::invalid_argument("stream"); }
        PIMPL_(stream) = stream;
    }

    STDMETHODIMP BridgeStream::Read(void* data, UINT32 size, UINT32* processedSize) noexcept
    {
        COM_CHECK_POINTER(data);
        COM_CHECK_POINTER_AND_SET(processedSize, 0);

        auto positionBefore = ULARGE_INTEGER();
        COM_DO_OR_RETURN(PIMPL_(stream)->Seek(LARGE_INTEGER(), STREAM_SEEK_CUR, &positionBefore));
    try_again:
        auto bytesRead = ULONG(*processedSize);
        const auto result = PIMPL_(stream)->Read(data, size, &bytesRead);
        *processedSize = bytesRead;
        if (!SUCCEEDED(result)) { return result; }
        auto positionAfter = ULARGE_INTEGER();
        COM_DO_OR_RETURN(PIMPL_(stream)->Seek(LARGE_INTEGER(), STREAM_SEEK_CUR, &positionAfter));
        if (positionBefore.QuadPart + bytesRead != positionAfter.QuadPart)
        {
            // It appears that the stream we get from Windows Search "jumps" to the end on some reads.
            // In addition, when such a jump occurs, the returned data is also not always valid.
            // This does _not_ happen with filtdump or iFiltTst. It does, however, also happen if we
            // switch to STA and only let the main thread read from the stream, so MTA and accessing
            // the stream from different threads has nothing to do with it.
            // Luckily, seeking always works.
            auto offset = LARGE_INTEGER();
            offset.QuadPart = positionBefore.QuadPart;
            COM_DO_OR_RETURN(PIMPL_(stream)->Seek(offset, STREAM_SEEK_SET, nullptr));
            goto try_again;
        }
        return result;
    }

    STDMETHODIMP BridgeStream::Seek(INT64 offset, UINT32 seekOrigin, UINT64* newPosition) noexcept
    {
        auto liOffset = LARGE_INTEGER();
        liOffset.QuadPart = offset;
        if (newPosition == nullptr) { return PIMPL_(stream)->Seek(liOffset, seekOrigin, nullptr); }
        auto uliNewPosition = ULARGE_INTEGER();
        uliNewPosition.QuadPart = *newPosition;
        const auto result = PIMPL_(stream)->Seek(liOffset, seekOrigin, &uliNewPosition);
        *newPosition = uliNewPosition.QuadPart;
        return result;
    }

    STDMETHODIMP BridgeStream::GetSize(UINT64* size) noexcept
    {
        COM_CHECK_POINTER_AND_SET(size, 0);
        auto stat = STATSTG();
        COM_DO_OR_RETURN(PIMPL_(stream)->Stat(&stat, STATFLAG_NONAME));
        *size = stat.cbSize.QuadPart;
        return S_OK;
    }
}
