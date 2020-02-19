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

#include "FileBuffer.hpp"

namespace streams
{
    class ReadStream; // provides an ::IStream reader for a FileBuffer

    /******************************************************************************/

    COM_CLASS_DECLARATION(ReadStream, (IStream),
public:
    explicit ReadStream(FileBuffer& buffer);

    STDMETHOD(Read)(void* pv, ULONG cb, ULONG* pcbRead) noexcept override;
    STDMETHOD(Write)(const void* pv, ULONG cb, ULONG* pcbWritten) noexcept override;
    STDMETHOD(SetSize)(ULARGE_INTEGER libNewSize) noexcept override;
    STDMETHOD(Seek)(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) noexcept override;
    STDMETHOD(CopyTo)(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten) noexcept override;
    STDMETHOD(Commit)(DWORD grfCommitFlags) noexcept override;
    STDMETHOD(Revert)(void) noexcept override;
    STDMETHOD(LockRegion)(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) noexcept override;
    STDMETHOD(UnlockRegion)(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) noexcept override;
    STDMETHOD(Stat)(STATSTG* pstatstg, DWORD grfStatFlag) noexcept override;
    STDMETHOD(Clone)(IStream** ppstm) noexcept override;
    );
}
