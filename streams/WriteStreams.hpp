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
#include "sevenzip.hpp"

#include "FileDescription.hpp"

namespace streams
{
    class WriteStream; // base class, handles waiting for content availability (i.e. subfilters wait for decompress)
    class BufferWriteStream; // uses memory for decompression
    class FileWriteStream; // uses a temporary file for decompression

    /******************************************************************************/

    COM_CLASS_DECLARATION(WriteStream, com::object IMPLEMENTS(sevenzip::ISequentialOutStream), COM_VISIBLE(sevenzip::ISequentialOutStream)
protected:
    explicit WriteStream(const com::FileDescription& description);

    virtual HRESULT WriteInteral(LPCVOID buffer, DWORD bytesToWrite, DWORD &bytesWritten) noexcept = 0;

public:
    PROPERTY_READONLY(const com::FileDescription&, Description, PIMPL_GETTER_ATTRIB);

    virtual IStreamPtr OpenReadStream() const = 0;
    void SetEndOfFile();
    HRESULT WaitUntilAvailable(ULONGLONG size) const noexcept;
    HRESULT WaitUntilEndOfFile() const noexcept;
    STDMETHOD(Write)(const void* data, UINT32 size, UINT32* processedSize);
    );

    /******************************************************************************/

    COM_CLASS_DECLARATION(BufferWriteStream, WriteStream,
protected:
    HRESULT WriteInteral(LPCVOID buffer, DWORD bytesToWrite, DWORD &bytesWritten) noexcept override;

public:
    explicit BufferWriteStream(const com::FileDescription& description);

    IStreamPtr OpenReadStream() const override;
    ULONG Read(void* buffer, ULONGLONG offset, ULONG count) const;

    static bool GetAvailableMemory(ULONGLONG& availableMemory);
    );

    /******************************************************************************/

    COM_CLASS_DECLARATION(FileWriteStream, WriteStream,
protected:
    HRESULT WriteInteral(LPCVOID buffer, DWORD bytesToWrite, DWORD &bytesWritten) noexcept override;

public:
    explicit FileWriteStream(const com::FileDescription& description);

    win32::unique_handle_ptr OpenReadFile() const;
    IStreamPtr OpenReadStream() const override;

    static bool GetFreeDiskSpace(ULONGLONG& freeDiskSpace);
    );
}
