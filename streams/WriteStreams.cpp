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

#include "WriteStreams.hpp"

#include "ReadStreams.hpp"

#include <algorithm>
#include <condition_variable>
#include <filesystem>
#include <mutex>

namespace streams
{
    COM_CLASS_IMPLEMENTATION(WriteStream,
                             PIMPL_CONSTRUCTOR(const com::FileDescription& description) : Description(description) {}
public:
    const com::FileDescription Description;
    std::mutex m;
    std::condition_variable cv;
    ULONGLONG bytesAvailable = 0;
    bool endOfFile = false;
    );

    WriteStream::WriteStream(const com::FileDescription& description) : PIMPL_INIT(description) {}

    PIMPL_GETTER(WriteStream, const com::FileDescription&, Description);

    void WriteStream::SetEndOfFile()
    {
        PIMPL_LOCK_BEGIN(m);
        PIMPL_(endOfFile) = true;
        PIMPL_LOCK_END;
        PIMPL_(cv).notify_all();
    }

    HRESULT WriteStream::WaitUntilAvailable(ULONGLONG size) const noexcept
    {
        COM_NOTHROW_BEGIN;
        PIMPL_LOCK_BEGIN(m);
        PIMPL_WAIT(m, cv, PIMPL_(bytesAvailable) >= size || PIMPL_(endOfFile));
        if (PIMPL_(bytesAvailable) < size) { return E_BOUNDS; } // beyond EOF
        PIMPL_LOCK_END;
        COM_NOTHROW_END;
        return S_OK;
    }

    HRESULT WriteStream::WaitUntilEndOfFile() const noexcept
    {
        COM_NOTHROW_BEGIN;
        PIMPL_LOCK_BEGIN(m);
        PIMPL_WAIT(m, cv, PIMPL_(endOfFile));
        PIMPL_LOCK_END;
        COM_NOTHROW_END;
        return S_OK;
    }

    STDMETHODIMP WriteStream::Write(const void* data, UINT32 size, UINT32* processedSize)
    {
        COM_CHECK_POINTER(data);
        if (processedSize != nullptr) { *processedSize = 0; }
        COM_NOTHROW_BEGIN;

        // if a write beyond EOF is received, EOF was most likely set due to an abort, not because 7-Zip had an incorrect size
        PIMPL_LOCK_BEGIN(m);
        if (PIMPL_(endOfFile)) { return E_ABORT; }
        PIMPL_LOCK_END;

        // TODO ensure unknown-sized files don't exceed thresholds
        auto bytesWritten = DWORD(0);
        const auto result = WriteInteral(data, size, &bytesWritten);
        if (processedSize != nullptr) { *processedSize = bytesWritten; }

        // increment the available bytes counter and notify all listeners
        PIMPL_LOCK_BEGIN(m);
        PIMPL_(bytesAvailable) += bytesWritten;
        PIMPL_LOCK_END;
        PIMPL_(cv).notify_all();

        return result;
        COM_NOTHROW_END;
    }

    /******************************************************************************/

    COM_CLASS_IMPLEMENTATION(BufferWriteStream,
public:
    std::vector<BYTE> buffer;
    std::mutex m;
    ULONGLONG bytesWritten = 0;
    );

    BufferWriteStream::BufferWriteStream(const com::FileDescription& description) : PIMPL_INIT(), base(description)
    {
        PIMPL_(buffer).resize(description.Size);
    }

    IStreamPtr BufferWriteStream::OpenReadStream() const
    {
        return BufferReadStream(*this).GetComInterface<IStream>();
    }

    ULONG BufferWriteStream::Read(void* buffer, ULONGLONG offset, ULONG count) const
    {
        // calculate the maximum available bytes within the lock
        ULONG bytesToRead;
        PIMPL_LOCK_BEGIN(m);
        if (offset >= PIMPL_(bytesWritten)) { return 0; }
        bytesToRead = static_cast<ULONG>(std::min(static_cast<ULONGLONG>(count), PIMPL_(bytesWritten) - offset));
        PIMPL_LOCK_END;

        std::memcpy(buffer, PIMPL_(buffer).data() + offset, bytesToRead);
        return bytesToRead;
    }

    HRESULT BufferWriteStream::WriteInteral(LPCVOID buffer, DWORD bytesToWrite, LPDWORD bytesWritten) noexcept
    {
        // calculate the writeable bytes and increment the bytes written counter within a lock
        ULONGLONG offset;
        DWORD actualBytesToWrite;
        COM_NOTHROW_BEGIN;
        PIMPL_LOCK_BEGIN(m);
        offset = PIMPL_(bytesWritten);
        if (offset >= PIMPL_(buffer).size()) { return E_BOUNDS; } // so 7zip was lying about the size
        actualBytesToWrite = static_cast<DWORD>(std::min(PIMPL_(buffer).size() - offset, static_cast<ULONGLONG>(bytesToWrite)));
        PIMPL_(bytesWritten) += actualBytesToWrite;
        PIMPL_LOCK_END;
        COM_NOTHROW_END;

        std::memcpy(PIMPL_(buffer).data() + offset, buffer, actualBytesToWrite);
        *bytesWritten = actualBytesToWrite;
        return S_OK;
    }

    /******************************************************************************/

    COM_CLASS_IMPLEMENTATION(FileWriteStream,
public:
    std::filesystem::path filePath;
    win32::unique_handle_ptr fileHandle;
    );

    static win32::unique_handle_ptr CreateOrOpenFile(const std::filesystem::path& filePath, DWORD desiredAccess, DWORD creationDisposition)
    {
        auto result = win32::unique_handle_ptr();
        auto nativeHandle = HANDLE(INVALID_HANDLE_VALUE);
        nativeHandle = ::CreateFileW(filePath.c_str(), desiredAccess, FILE_SHARE_DELETE | FILE_SHARE_READ, nullptr, creationDisposition, FILE_ATTRIBUTE_TEMPORARY, nullptr);
        result.reset(nativeHandle);
        WIN32_DO_OR_THROW(nativeHandle && nativeHandle != INVALID_HANDLE_VALUE);
        return result;
    }

    FileWriteStream::FileWriteStream(const com::FileDescription& description) : PIMPL_INIT(), base(description)
    {
        const auto filePath = utils::get_temp_path() / utils::get_temp_file_name();
        PIMPL_(filePath) = filePath;
        PIMPL_(fileHandle) = CreateOrOpenFile(filePath, GENERIC_WRITE, CREATE_ALWAYS);
        if (description.SizeIsValid)
        {
            // reserve space beforehand
            auto sizeDistance = LARGE_INTEGER();
            sizeDistance.QuadPart = description.Size;
            WIN32_DO_OR_THROW(::SetFilePointerEx(PIMPL_(fileHandle).get(), sizeDistance, nullptr, FILE_BEGIN));
            WIN32_DO_OR_THROW(::SetEndOfFile(PIMPL_(fileHandle).get()));
            sizeDistance.QuadPart = 0;
            WIN32_DO_OR_THROW(::SetFilePointerEx(PIMPL_(fileHandle).get(), sizeDistance, nullptr, FILE_BEGIN));
        }
    }

    HRESULT FileWriteStream::WriteInteral(LPCVOID buffer, DWORD bytesToWrite, LPDWORD bytesWritten) noexcept
    {
        WIN32_DO_OR_RETURN(::WriteFile(PIMPL_(fileHandle).get(), buffer, bytesToWrite, bytesWritten, nullptr));
        return S_OK;
    }

    win32::unique_handle_ptr FileWriteStream::OpenReadFile() const
    {
        return CreateOrOpenFile(PIMPL_(filePath), GENERIC_READ, OPEN_EXISTING);
    }

    IStreamPtr FileWriteStream::OpenReadStream() const
    {
        return FileReadStream(*this).GetComInterface<IStream>();
    }
}
