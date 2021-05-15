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

#include "FileBuffer.hpp"

#include "settings.hpp"

#include <algorithm>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <stdexcept>

namespace streams
{
    CLASS_IMPLEMENTATION(FileBuffer,
                         PIMPL_CONSTRUCTOR(const com::FileDescription& description) : Description(description), size(description.Size) {}
public:
    const com::FileDescription Description;
    const ULONGLONG size;
    std::mutex m;
    std::condition_variable cv;
    std::vector<BYTE> buffer;
    win32::unique_handle_ptr fileHandle;
    win32::unique_handle_ptr fileMapping;
    ULARGE_INTEGER fileViewPosition;
    SIZE_T fileViewSize;
    win32::unique_fileview_ptr fileView;
    ULONGLONG position = 0;
    bool endOfFile = false;
    );

    static void TryCreateTempFile(const std::filesystem::path path, win32::unique_handle_ptr& handle)
    {
        const auto filePath = path / utils::get_temp_file_name();
        handle.reset(::CreateFileW(filePath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr));
        if (handle.get() == INVALID_HANDLE_VALUE) { handle.release(); }
    }

    FileBuffer::FileBuffer(const com::FileDescription& description) : PIMPL_INIT(description)
    {
        const auto maxBufferSize = settings::maximum_buffer_size();
        if (PIMPL_(size) > maxBufferSize)
        {
            // get the file view size
            auto systemInfo = SYSTEM_INFO();
            ::GetSystemInfo(&systemInfo);
            PIMPL_(fileViewSize) = std::max(maxBufferSize - (maxBufferSize % systemInfo.dwAllocationGranularity), static_cast<SIZE_T>(systemInfo.dwAllocationGranularity));

            // create a temporary file
            const auto tempPath = utils::get_temp_path();
            TryCreateTempFile(tempPath, PIMPL_(fileHandle)); // this will most likely fail under Windows Search since the default temp directory is not writable
            if (!PIMPL_(fileHandle))
            {
                const auto lastError = ::GetLastError();
                const auto systemTempPath = utils::get_system_temp_path();
                if (tempPath == systemTempPath) { WIN32_THROW(lastError); } // no point in trying the same path twice
                TryCreateTempFile(systemTempPath, PIMPL_(fileHandle));
                if (!PIMPL_(fileHandle)) { WIN32_THROW(lastError); } // better throw the original error
            }

            // extend the file to the full size
            auto endOfFilePosition = LARGE_INTEGER();
            endOfFilePosition.QuadPart = PIMPL_(size);
            WIN32_DO_OR_THROW(::SetFilePointerEx(PIMPL_(fileHandle).get(), endOfFilePosition, nullptr, FILE_BEGIN));
            WIN32_DO_OR_THROW(::SetEndOfFile(PIMPL_(fileHandle).get()));
            WIN32_DO_OR_THROW(::SetFilePointerEx(PIMPL_(fileHandle).get(), LARGE_INTEGER(), nullptr, FILE_BEGIN));

            // map the file to allow simultaneous reading
            PIMPL_(fileMapping).reset(::CreateFileMappingW(PIMPL_(fileHandle).get(), nullptr, PAGE_READONLY, endOfFilePosition.HighPart, endOfFilePosition.LowPart, nullptr));
            if (PIMPL_(fileMapping).get() == INVALID_HANDLE_VALUE) { PIMPL_(fileMapping).release(); }
            WIN32_DO_OR_THROW(PIMPL_(fileMapping));
        }
        else
        {
            PIMPL_(buffer).resize(static_cast<size_t>(PIMPL_(size))); // simply allocate the buffer
        }
    }

    PIMPL_GETTER(FileBuffer, const com::FileDescription&, Description);

    ULONG FileBuffer::Append(const void* buffer, ULONG count)
    {
        if (buffer == nullptr) { throw std::invalid_argument("buffer"); }

        // preliminary checks
        PIMPL_LOCK_BEGIN(m);
        if (PIMPL_(endOfFile)) { COM_THROW(E_ABORT); } // no further file writes are allowed
        PIMPL_LOCK_END;
        if (PIMPL_(position) >= PIMPL_(size)) { return 0; } // no writes beyond the size
        const auto bytesToWrite = static_cast<ULONG>(std::min(PIMPL_(size) - PIMPL_(position), static_cast<ULONGLONG>(count))); // limit to available size

        // write the data and advance the position if successful
        auto bytesWritten = ULONG(0);
        if (PIMPL_(fileHandle))
        {
            auto bytesToWriteRemaining = static_cast<DWORD>(bytesToWrite);
            auto startPosition = LARGE_INTEGER();
            startPosition.QuadPart = PIMPL_(position);
            WIN32_DO_OR_THROW(::SetFilePointerEx(PIMPL_(fileHandle).get(), startPosition, nullptr, FILE_BEGIN));
            while (bytesToWriteRemaining > 0)
            {
                auto nativeBytesWritten = DWORD();
                WIN32_DO_OR_THROW(::WriteFile(PIMPL_(fileHandle).get(), reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(buffer) + bytesWritten), bytesToWriteRemaining, &nativeBytesWritten, nullptr));
                if (nativeBytesWritten == 0) { break; } // EOF reached
                if (nativeBytesWritten > bytesToWriteRemaining) { COM_THROW(E_UNEXPECTED); } // sanity check, just to be sure
                bytesWritten += nativeBytesWritten;
                bytesToWriteRemaining -= nativeBytesWritten;
            }
        }
        else
        {
            std::memcpy(PIMPL_(buffer).data() + PIMPL_(position), buffer, bytesToWrite);
            bytesWritten = bytesToWrite;
        }

        // increment the position and notify all listeners
        PIMPL_LOCK_BEGIN(m);
        PIMPL_(position) += bytesWritten;
        PIMPL_LOCK_END;
        PIMPL_(cv).notify_all();

        return bytesWritten;
    }

    ULONG FileBuffer::Read(ULONGLONG offset, void* buffer, ULONG count) const
    {
        if (buffer == nullptr) { throw std::invalid_argument("buffer"); }
        if (MAXULONGLONG - offset < count) { throw std::length_error("offset + count"); }

        // preliminary checks
        if (offset >= PIMPL_(size)) { return 0; }
        auto availableBytes = PIMPL_(size) - offset;
        PIMPL_LOCK_BEGIN(m);
        const auto requiredSize = std::min(offset + count, PIMPL_(size));
        PIMPL_WAIT(m, cv, PIMPL_(position) >= requiredSize || PIMPL_(endOfFile));
        if (PIMPL_(position) < requiredSize)
        {
            if (PIMPL_(position) <= offset) { return 0; } // will not become available anymore
            availableBytes = PIMPL_(position) - offset;
        }
        PIMPL_LOCK_END;
        const auto bytesToRead = static_cast<ULONG>(std::min(availableBytes, static_cast<ULONGLONG>(count))); // limit to available size

        // read the data
        auto bytesRead = ULONG(0);
        if (PIMPL_(fileHandle))
        {
            auto bytesToReadRemaining = static_cast<SIZE_T>(bytesToRead);
            while (bytesToReadRemaining > 0)
            {
                const auto currentOffset = offset + bytesRead;
                const auto bytesBeforeOffset = static_cast<SIZE_T>(currentOffset % PIMPL_(fileViewSize));
                const auto startPosition = currentOffset - bytesBeforeOffset;
                if (!PIMPL_(fileView) || PIMPL_(fileViewPosition).QuadPart != startPosition)
                {
                    // map another region of the file
                    PIMPL_(fileView).reset();
                    PIMPL_(fileViewPosition).QuadPart = startPosition;
                    PIMPL_(fileView).reset(::MapViewOfFile(PIMPL_(fileMapping).get(), FILE_MAP_READ, PIMPL_(fileViewPosition).HighPart, PIMPL_(fileViewPosition).LowPart, static_cast<SIZE_T>(std::min(static_cast<ULONGLONG>(PIMPL_(fileViewSize)), PIMPL_(size) - startPosition))));
                    WIN32_DO_OR_THROW(PIMPL_(fileView));
                }
                const auto bytesToReadThisPass = static_cast<ULONG>(std::min(PIMPL_(fileViewSize) - bytesBeforeOffset, bytesToReadRemaining));
                std::memcpy(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(buffer) + bytesRead), reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(PIMPL_(fileView).get()) + bytesBeforeOffset), bytesToReadThisPass);
                bytesRead += bytesToReadThisPass;
                bytesToReadRemaining -= bytesToReadThisPass;
            }
        }
        else
        {
            std::memcpy(buffer, PIMPL_(buffer).data() + offset, bytesToRead);
            bytesRead = bytesToRead;
        }

        return bytesRead;
    }

    void FileBuffer::SetEndOfFile()
    {
        PIMPL_LOCK_BEGIN(m);
        PIMPL_(endOfFile) = true;
        PIMPL_LOCK_END;
        PIMPL_(cv).notify_all();
    }
}
