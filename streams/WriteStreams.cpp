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

#include "settings.hpp"

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
        PIMPL_LOCK_END;
        COM_NOTHROW_END;
        return S_OK;
    }

    HRESULT WriteStream::WaitUntilEndOfFile() const noexcept
    {
        return WaitUntilAvailable(MAXULONGLONG);
    }

    STDMETHODIMP WriteStream::Write(const void* data, UINT32 size, UINT32* processedSize) noexcept
    {
        COM_CHECK_POINTER(data);
        if (processedSize != nullptr) { *processedSize = 0; }
        COM_NOTHROW_BEGIN;

        // if a write beyond EOF is received, EOF was most likely set due to an abort, not because 7-Zip had an incorrect size
        PIMPL_LOCK_BEGIN(m);
        if (PIMPL_(endOfFile)) { return E_ABORT; }
        PIMPL_LOCK_END;

        // forward write to the actual implementation
        auto bytesWritten = DWORD(0);
        const auto result = WriteInteral(data, size, bytesWritten);
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
    ULONGLONG totalBytesWritten = 0;
    );

    BufferWriteStream::BufferWriteStream(const com::FileDescription& description) : PIMPL_INIT(), base(description)
    {
        PIMPL_(buffer).resize(description.Size);
    }

    IStreamPtr BufferWriteStream::OpenReadStream() const
    {
        return BufferReadStream::CreateComInstance<IStream>(*this);
    }

    ULONG BufferWriteStream::Read(void* buffer, ULONGLONG offset, ULONG count) const
    {
        // calculate the maximum available bytes within the lock
        ULONG bytesToRead;
        PIMPL_LOCK_BEGIN(m);
        if (offset >= PIMPL_(totalBytesWritten)) { return 0; }
        bytesToRead = static_cast<ULONG>(std::min(static_cast<ULONGLONG>(count), PIMPL_(totalBytesWritten) - offset));
        PIMPL_LOCK_END;

        std::memcpy(buffer, PIMPL_(buffer).data() + offset, bytesToRead);
        return bytesToRead;
    }

    HRESULT BufferWriteStream::WriteInteral(LPCVOID buffer, DWORD bytesToWrite, DWORD& bytesWritten) noexcept
    {
        // calculate the writeable bytes and increment the bytes written counter within a lock
        ULONGLONG offset;
        DWORD actualBytesToWrite;
        COM_NOTHROW_BEGIN;
        PIMPL_LOCK_BEGIN(m);
        offset = PIMPL_(totalBytesWritten);
        if (offset >= PIMPL_(buffer).size()) { return E_BOUNDS; } // so 7-Zip was lying about the size
        actualBytesToWrite = static_cast<DWORD>(std::min(PIMPL_(buffer).size() - offset, static_cast<ULONGLONG>(bytesToWrite)));
        PIMPL_(totalBytesWritten) += actualBytesToWrite;
        PIMPL_LOCK_END;
        COM_NOTHROW_END;

        std::memcpy(PIMPL_(buffer).data() + offset, buffer, actualBytesToWrite);
        bytesWritten = actualBytesToWrite;
        return S_OK;
    }

    bool BufferWriteStream::GetAvailableMemory(ULONGLONG& availableMemory)
    {
        auto status = MEMORYSTATUSEX();
        status.dwLength = sizeof(MEMORYSTATUSEX);
        if (!::GlobalMemoryStatusEx(&status)) { return false; }
        availableMemory = status.ullAvailVirtual;
        const auto minAvailableMemory = settings::min_available_memory();
        if (minAvailableMemory)
        {
            if (*minAvailableMemory > availableMemory)
            {
                availableMemory = 0;
            }
            else
            {
                availableMemory -= *minAvailableMemory;
            }
        }
        else
        {
            availableMemory = availableMemory * 9 / 10; // always keep a bit left
        }
        return true;
    }

    /******************************************************************************/

    COM_CLASS_IMPLEMENTATION(FileWriteStream,
public:
    std::filesystem::path filePath;
    win32::unique_handle_ptr fileHandle;
    bool checkSizeLimitEachMegabyte;
    ULONGLONG totalBytesWritten = 0;
    ULONGLONG bytesWrittenSinceLastSizeCheck = 0;
    );

    static const auto TempPath = utils::get_temp_path();

    FileWriteStream::FileWriteStream(const com::FileDescription& description) : PIMPL_INIT(), base(description)
    {
        const auto filePath = TempPath / utils::get_temp_file_name();
        PIMPL_(filePath) = filePath;
        auto nativeHandle = HANDLE(INVALID_HANDLE_VALUE);
        nativeHandle = ::CreateFileW(filePath.c_str(), GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
        PIMPL_(fileHandle).reset(nativeHandle);
        WIN32_DO_OR_THROW(nativeHandle && nativeHandle != INVALID_HANDLE_VALUE);
        PIMPL_(checkSizeLimitEachMegabyte) = !description.SizeIsValid;
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

    HRESULT FileWriteStream::WriteInteral(LPCVOID buffer, DWORD bytesToWrite, DWORD& bytesWritten) noexcept
    {
        if (PIMPL_(checkSizeLimitEachMegabyte))
        {
            if (PIMPL_(bytesWrittenSinceLastSizeCheck) > 1 * 1024 * 1024)
            {
                const auto maxFileSize = settings::max_file_size();
                if (maxFileSize && PIMPL_(totalBytesWritten) > * maxFileSize) { return E_OUTOFMEMORY; } // file larger than configured limit
                const auto minFreeDiskSpace = settings::min_free_disk_space();
                if (minFreeDiskSpace)
                {
                    auto value = ULARGE_INTEGER();
                    WIN32_DO_OR_RETURN(::GetDiskFreeSpaceExW(TempPath.c_str(), &value, nullptr, nullptr));
                    if (value.QuadPart < *minFreeDiskSpace) { return E_OUTOFMEMORY; } // too little space left
                }
                PIMPL_(bytesWrittenSinceLastSizeCheck) = 0;
            }
        }
        else
        {
            const auto bytesToWriteRemaing = Description.Size - PIMPL_(totalBytesWritten);
            if (bytesToWrite > bytesToWriteRemaing)
            {
                if (bytesToWriteRemaing == 0) { return E_BOUNDS; } // so 7-Zip was lying about the size
                bytesToWrite = static_cast<DWORD>(bytesToWriteRemaing);
            }
        }
        auto const result = ::WriteFile(PIMPL_(fileHandle).get(), buffer, bytesToWrite, &bytesWritten, nullptr);
        assert(bytesWritten <= bytesToWrite);
        PIMPL_(totalBytesWritten) += bytesWritten;
        PIMPL_(bytesWrittenSinceLastSizeCheck) += bytesWritten;
        WIN32_DO_OR_RETURN(result);
        return S_OK;
    }

    win32::unique_handle_ptr FileWriteStream::OpenReadFile() const
    {
        auto result = win32::unique_handle_ptr();
        auto nativeHandle = HANDLE(INVALID_HANDLE_VALUE);
        nativeHandle = ::ReOpenFile(PIMPL_(fileHandle).get(), GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_FLAG_DELETE_ON_CLOSE);
        result.reset(nativeHandle);
        WIN32_DO_OR_THROW(nativeHandle && nativeHandle != INVALID_HANDLE_VALUE);
        return result;
    }

    IStreamPtr FileWriteStream::OpenReadStream() const
    {
        return FileReadStream::CreateComInstance<IStream>(*this);
    }

    bool FileWriteStream::GetFreeDiskSpace(ULONGLONG& freeDiskSpace)
    {
        auto value = ULARGE_INTEGER();
        if (!::GetDiskFreeSpaceExW(TempPath.c_str(), &value, nullptr, nullptr)) { return false; }
        freeDiskSpace = value.QuadPart;
        const auto minFreeDiskSpace = settings::min_free_disk_space();
        if (minFreeDiskSpace)
        {
            if (*minFreeDiskSpace > freeDiskSpace)
            {
                freeDiskSpace = 0;
            }
            else
            {
                freeDiskSpace -= *minFreeDiskSpace;
            }
        }
        else
        {
            freeDiskSpace = freeDiskSpace * 9 / 10; // always keep a bit left
        }
        return true;
    }
}
