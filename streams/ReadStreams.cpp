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

#include "ReadStreams.hpp"

#include <algorithm>
#include <cassert>

namespace streams
{
    COM_CLASS_IMPLEMENTATION(ReadStream,
                             PIMPL_CONSTRUCTOR(const com::FileDescription& description) : description(description) {}
public:
    const com::FileDescription description;
    );

    ReadStream::ReadStream(const com::FileDescription& description) : PIMPL_INIT(description) {}

    STDMETHODIMP ReadStream::Write(const void* pv, ULONG cb, ULONG* pcbWritten)
    {
        if (pcbWritten != nullptr) { *pcbWritten = 0; }
        return STG_E_ACCESSDENIED;
    }

    STDMETHODIMP ReadStream::SetSize(ULARGE_INTEGER libNewSize)
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP ReadStream::CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten)
    {
        COM_CHECK_POINTER(pstm);
        if (pcbRead != nullptr) { pcbRead->QuadPart = 0; }
        if (pcbWritten != nullptr) { pcbWritten->QuadPart = 0; }
        char buffer[8000];
        while (cb.QuadPart > 0)
        {
            const auto size = static_cast<ULONG>(std::min(cb.QuadPart, sizeof(buffer)));

            // read operation
            auto read = ULONG(0);
            COM_DO_OR_RETURN(Read(buffer, size, &read));
            if (pcbRead != nullptr) { pcbRead->QuadPart += read; }

            // write operation
            auto written = ULONG(0);
            COM_DO_OR_RETURN(pstm->Write(buffer, read, &written));
            if (pcbWritten != nullptr) { pcbWritten->QuadPart = written; }

            // check for EOF
            if (read < size || written < read)
            {
                return S_FALSE;
            }
            cb.QuadPart -= read;
        }
        return S_OK;
    }

    STDMETHODIMP ReadStream::Commit(DWORD grfCommitFlags)
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP ReadStream::Revert(void)
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP ReadStream::LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP ReadStream::UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP ReadStream::Stat(STATSTG* pstatstg, DWORD grfStatFlag)
    {
        COM_CHECK_POINTER(pstatstg);
        std::memset(pstatstg, 0, sizeof(STATSTG));
        pstatstg->type = STGTY_STREAM;
        pstatstg->grfMode = STGM_READ | STGM_SIMPLE;
        switch (grfStatFlag)
        {
        case STATFLAG_DEFAULT:
            return PIMPL_(description).ToStat(pstatstg, true);
        case STATFLAG_NONAME:
            return PIMPL_(description).ToStat(pstatstg, false);
        case STATFLAG_NOOPEN:
            return STG_E_INVALIDFLAG;
        default:
            return E_NOTIMPL;
        }
    }

    STDMETHODIMP ReadStream::Clone(IStream** ppstm)
    {
        COM_CHECK_POINTER_AND_SET(ppstm, nullptr);

        auto streamPtr = IStreamPtr();
        COM_NOTHROW_BEGIN;

        // get the current position
        auto move = LARGE_INTEGER();
        move.QuadPart = 0;
        auto position = ULARGE_INTEGER();
        COM_DO_OR_RETURN(Seek(move, STREAM_SEEK_CUR, &position));

        // clone the stream and set its position
        streamPtr = CloneInternal();
        move.QuadPart = position.QuadPart;
        COM_DO_OR_RETURN(streamPtr->Seek(move, STREAM_SEEK_SET, nullptr));

        COM_NOTHROW_END;
        *ppstm = streamPtr.Detach(); // nothing must fail afterwards
        return S_OK;
    }

    /******************************************************************************/

    COM_CLASS_IMPLEMENTATION(BufferReadStream,
                             PIMPL_CONSTRUCTOR(const BufferWriteStream& source) : source(source) {}
public:
    const BufferWriteStream source;
    ULONGLONG position = 0;
    );

    BufferReadStream::BufferReadStream(const BufferWriteStream& source) : PIMPL_INIT(source), base(source.Description) {}

    IStreamPtr BufferReadStream::CloneInternal() const
    {
        return BufferReadStream::CreateComInstance<IStream>(PIMPL_(source));
    }

    STDMETHODIMP BufferReadStream::Read(void* pv, ULONG cb, ULONG* pcbRead)
    {
        COM_CHECK_POINTER(pv);
        COM_CHECK_POINTER_AND_SET(pcbRead, 0);
        COM_DO_OR_RETURN(PIMPL_(source).WaitUntilAvailable(PIMPL_(position) + cb));
        COM_NOTHROW_BEGIN;
        *pcbRead = PIMPL_(source).Read(pv, PIMPL_(position), cb);
        COM_NOTHROW_END;
        PIMPL_(position) += *pcbRead;
        return *pcbRead == 0 ? S_FALSE : S_OK;
    }

    STDMETHODIMP BufferReadStream::Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition)
    {
        if (plibNewPosition != nullptr) { plibNewPosition->QuadPart = PIMPL_(position); }

        // get the starting position
        ULONGLONG start;
        switch (dwOrigin)
        {
        case STREAM_SEEK_SET: start = 0; break;
        case STREAM_SEEK_CUR: start = PIMPL_(position); break;
        case STREAM_SEEK_END: start = PIMPL_(source).Description.Size; break;
        default: return E_INVALIDARG;
        }

        // analyze the offset
        if (dlibMove.QuadPart < 0)
        {
            // seeks before start is illegal, check for that
            const auto offset = static_cast<ULONGLONG>(-dlibMove.QuadPart);
            if (offset > start)
            {
                return STG_E_SEEKERROR;
            }
            PIMPL_(position) = start - offset;
        }
        else
        {
            // make sure we stay within ULONGLONG bounds
            const auto offset = static_cast<ULONGLONG>(dlibMove.QuadPart);
            const auto remaining = MAXULONGLONG - start;
            if (offset > remaining)
            {
                return STG_E_SEEKERROR;
            }
            PIMPL_(position) = start + offset;
        }
        if (plibNewPosition != nullptr) { plibNewPosition->QuadPart = PIMPL_(position); };
        return S_OK;
    }

    /******************************************************************************/

    COM_CLASS_IMPLEMENTATION(FileReadStream,
                             PIMPL_CONSTRUCTOR(const FileWriteStream& source) : source(source) {}
public:
    const FileWriteStream source;
    win32::unique_handle_ptr fileHandle;
    ULONGLONG positionCache = 0;
    bool isCacheValid = false; /* better do an initial sync */
    );

    FileReadStream::FileReadStream(const FileWriteStream& source) : PIMPL_INIT(source), base(source.Description)
    {
        PIMPL_(fileHandle) = source.OpenReadFile();
    }

    IStreamPtr FileReadStream::CloneInternal() const
    {
        return FileReadStream::CreateComInstance<IStream>(PIMPL_(source));
    }

    STDMETHODIMP FileReadStream::Read(void* pv, ULONG cb, ULONG* pcbRead)
    {
        COM_CHECK_POINTER(pv);
        COM_CHECK_POINTER_AND_SET(pcbRead, 0);

        // wait until everything is available
        if (!PIMPL_(isCacheValid))
        {
            auto position = LARGE_INTEGER();
            WIN32_DO_OR_RETURN(::SetFilePointerEx(PIMPL_(fileHandle).get(), LARGE_INTEGER(), &position, FILE_CURRENT));
            PIMPL_(positionCache) = position.QuadPart;
            PIMPL_(isCacheValid) = true;
        }
        COM_DO_OR_RETURN(PIMPL_(source).WaitUntilAvailable(PIMPL_(positionCache) + cb));

        // read and update the cache
        while (cb > 0)
        {
            // ::ReadFile might not read the requested number of bytes in one call,
            // but ISequentialStream::Read expects all bytes unless EOF is reached
            auto bytesRead = ULONG(0);
            if (!::ReadFile(PIMPL_(fileHandle).get(), pv, cb, &bytesRead, nullptr))
            {
                PIMPL_(isCacheValid) = false;
                return COM_LAST_WIN32_ERROR;
            }
            if (bytesRead == 0)
            {
                return S_FALSE; // ReadFile indicated EOF, but that's also a success
            }
            PIMPL_(positionCache) += bytesRead;
            assert(bytesRead <= cb);
            pv = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(pv) + bytesRead);
            cb -= bytesRead;
            *pcbRead += bytesRead;
        }

        // success
        return S_OK;
    }

    STDMETHODIMP FileReadStream::Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition)
    {
        if (dlibMove.QuadPart == 0 && dwOrigin == STREAM_SEEK_CUR && PIMPL_(isCacheValid))
        {
            if (plibNewPosition != nullptr) { plibNewPosition->QuadPart = PIMPL_(positionCache); }
            return S_OK;
        }
        if (dwOrigin == STREAM_SEEK_END)
        {
            if (PIMPL_(source).Description.SizeIsValid)
            {
                dwOrigin = STREAM_SEEK_SET;
                dlibMove.QuadPart += PIMPL_(source).Description.Size;
            }
            else
            {
                PIMPL_(source).WaitUntilEndOfFile();
            }
        }
        auto newPosition = LARGE_INTEGER();
        newPosition.QuadPart = plibNewPosition != nullptr ? plibNewPosition->QuadPart : PIMPL_(positionCache); // not really necessary
        const auto result = ::SetFilePointerEx(PIMPL_(fileHandle).get(), dlibMove, &newPosition, dwOrigin); // dwOrigin is compatible with SetFilePointerEx
        if (plibNewPosition != nullptr)
        {
            plibNewPosition->QuadPart = newPosition.QuadPart;
        }
        PIMPL_(positionCache) = newPosition.QuadPart;
        PIMPL_(isCacheValid) = result;
        return result ? S_OK : COM_LAST_WIN32_ERROR; // last error still valid, only assignments inbetween
    }
}
