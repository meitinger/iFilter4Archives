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

#include "ReadStream.hpp"

#include <algorithm>

namespace streams
{
    CLASS_IMPLEMENTATION(ReadStream,
                         PIMPL_CONSTRUCTOR(FileBuffer& buffer) : buffer(buffer) {}
public:
    FileBuffer buffer;
    ULONGLONG position = 0;
    );

    ReadStream::ReadStream(FileBuffer& buffer) : PIMPL_INIT(buffer) {}

    STDMETHODIMP ReadStream::Read(void* pv, ULONG cb, ULONG* pcbRead) noexcept
    {
        COM_CHECK_POINTER(pv);
        COM_CHECK_POINTER_AND_SET(pcbRead, 0);
        COM_NOTHROW_BEGIN;

        const auto bytesRead = PIMPL_(buffer).Read(PIMPL_(position), pv, cb);
        *pcbRead = bytesRead;
        PIMPL_(position) += bytesRead;
        return bytesRead < cb ? S_FALSE : S_OK;

        COM_NOTHROW_END;
    }

    STDMETHODIMP ReadStream::Write(const void* pv, ULONG cb, ULONG* pcbWritten) noexcept
    {
        if (pcbWritten != nullptr) { *pcbWritten = 0; }
        return STG_E_ACCESSDENIED;
    }

    STDMETHODIMP ReadStream::SetSize(ULARGE_INTEGER libNewSize) noexcept
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP ReadStream::Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) noexcept
    {
        if (plibNewPosition != nullptr) { plibNewPosition->QuadPart = PIMPL_(position); }

        // get the starting position
        ULONGLONG start;
        switch (dwOrigin)
        {
        case STREAM_SEEK_SET: start = 0; break;
        case STREAM_SEEK_CUR: start = PIMPL_(position); break;
        case STREAM_SEEK_END: start = PIMPL_(buffer).Description.Size; break;
        default: return E_INVALIDARG;
        }

        // analyze the offset
        if (dlibMove.QuadPart < 0)
        {
            // seeks before start is illegal, check for that
            const auto offset = static_cast<ULONGLONG>(-dlibMove.QuadPart);
            if (offset > start) { return STG_E_SEEKERROR; }
            PIMPL_(position) = start - offset;
        }
        else
        {
            // make sure we stay within ULONGLONG bounds
            const auto offset = static_cast<ULONGLONG>(dlibMove.QuadPart);
            const auto remaining = MAXULONGLONG - start;
            if (offset > remaining) { return STG_E_SEEKERROR; }
            PIMPL_(position) = start + offset;
        }
        if (plibNewPosition != nullptr) { plibNewPosition->QuadPart = PIMPL_(position); };
        return S_OK;
    }

    STDMETHODIMP ReadStream::CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten) noexcept
    {
        COM_CHECK_POINTER(pstm);
        if (pcbRead != nullptr) { pcbRead->QuadPart = 0; }
        if (pcbWritten != nullptr) { pcbWritten->QuadPart = 0; }
        char buffer[8000];
        auto bytesToCopyRemaining = cb.QuadPart;
        while (bytesToCopyRemaining > 0)
        {
            const auto bytesToRead = static_cast<ULONG>(std::min(bytesToCopyRemaining, static_cast<ULONGLONG>(sizeof(buffer))));

            // read operation
            auto bytesRead = ULONG(0);
            COM_DO_OR_RETURN(Read(buffer, bytesToRead, &bytesRead));
            if (pcbRead != nullptr) { pcbRead->QuadPart += bytesRead; }

            // write operation
            auto bytesWritten = ULONG(0);
            COM_DO_OR_RETURN(pstm->Write(buffer, bytesRead, &bytesWritten));
            if (pcbWritten != nullptr) { pcbWritten->QuadPart += bytesWritten; }

            // check for EOF (according to docs)
            if (bytesRead < bytesToRead || bytesWritten < bytesRead) { return S_FALSE; }
            bytesToCopyRemaining -= bytesRead;
        }
        return S_OK;
    }

    STDMETHODIMP ReadStream::Commit(DWORD grfCommitFlags) noexcept
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP ReadStream::Revert(void) noexcept
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP ReadStream::LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) noexcept
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP ReadStream::UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) noexcept
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP ReadStream::Stat(STATSTG* pstatstg, DWORD grfStatFlag) noexcept
    {
        COM_CHECK_POINTER(pstatstg);
        std::memset(pstatstg, 0, sizeof(STATSTG));
        pstatstg->type = STGTY_STREAM;
        pstatstg->grfMode = STGM_READ | STGM_SIMPLE;
        switch (grfStatFlag)
        {
        case STATFLAG_DEFAULT: return PIMPL_(buffer).Description.ToStat(pstatstg, true);
        case STATFLAG_NONAME: return PIMPL_(buffer).Description.ToStat(pstatstg, false);
        case STATFLAG_NOOPEN: return STG_E_INVALIDFLAG;
        default: return E_NOTIMPL;
        }
    }

    STDMETHODIMP ReadStream::Clone(IStream** ppstm) noexcept
    {
        if (ppstm != nullptr) { *ppstm = nullptr; }
        return E_NOTIMPL; // limit access to one thread, also not supported by Windows Search
    }
}
