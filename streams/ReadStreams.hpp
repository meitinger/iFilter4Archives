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

#include "FileDescription.hpp"
#include "WriteStreams.hpp"

namespace streams
{
    class ReadStream; // base class, handles common ::IStream stuff
    class BufferReadStream; // reads decompressed data from memory
    class FileReadStream; // reads decompressed data from a temporary file

    /******************************************************************************/

    COM_CLASS_DECLARATION(ReadStream, com::object IMPLEMENTS(IStream), COM_VISIBLE(IStream)
protected:
    explicit ReadStream(const com::FileDescription& description);

    virtual IStreamPtr CloneInternal() const = 0;

public:
    STDMETHOD(Write)(const void* pv, ULONG cb, ULONG* pcbWritten);
    STDMETHOD(SetSize)(ULARGE_INTEGER libNewSize);
    STDMETHOD(CopyTo)(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten);
    STDMETHOD(Commit)(DWORD grfCommitFlags);
    STDMETHOD(Revert)(void);
    STDMETHOD(LockRegion)(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
    STDMETHOD(UnlockRegion)(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
    STDMETHOD(Stat)(STATSTG* pstatstg, DWORD grfStatFlag);
    STDMETHOD(Clone)(IStream** ppstm);
    );

    /******************************************************************************/

    COM_CLASS_DECLARATION(BufferReadStream, ReadStream,
protected:
    IStreamPtr CloneInternal() const override;

public:
    explicit BufferReadStream(const BufferWriteStream& source);

    STDMETHOD(Read)(void* pv, ULONG cb, ULONG* pcbRead);
    STDMETHOD(Seek)(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition);
    );

    /******************************************************************************/

    COM_CLASS_DECLARATION(FileReadStream, ReadStream,
protected:
    IStreamPtr CloneInternal() const override;

public:
    explicit FileReadStream(const FileWriteStream& source);

    STDMETHOD(Read)(void* pv, ULONG cb, ULONG* pcbRead);
    STDMETHOD(Seek)(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition);
    );
}
