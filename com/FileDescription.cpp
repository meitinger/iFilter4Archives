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

#include "FileDescription.hpp"

#include <filesystem>
#include <functional>
#include <mutex>
#include <stdexcept>

namespace com
{
    SIMPLE_CLASS_IMPLEMENTATION(FileDescription,
public:
    std::wstring Name;
    std::mutex extensionCacheMutex;
    std::wstring extensionCache;
    bool extensionCacheValid = false;
    bool IsDirectory;
    ULONGLONG Size;
    FILETIME ModificationTime;
    FILETIME CreationTime;
    FILETIME AccessTime;
    );

    FileDescription::FileDescription() : PIMPL_INIT() {}

    PIMPL_GETTER(FileDescription, const std::wstring&, Name);
    const std::wstring& FileDescription::GetExtension() const
    {
        // this function may be called from multiple threads, so ensure locking
        PIMPL_LOCK_BEGIN(extensionCacheMutex);
        if (!PIMPL_(extensionCacheValid))
        {
            // get the lower-case extension
            PIMPL_(extensionCache).assign(std::filesystem::path(PIMPL_(Name)).extension());
            const auto result = _wcslwr_s(PIMPL_(extensionCache).data(), PIMPL_(extensionCache).length() + 1); // not portable, assumes data() == c_str()
            if (result != 0) { throw std::system_error(result, std::generic_category()); }
            PIMPL_(extensionCacheValid) = true;
        }
        return PIMPL_(extensionCache);
        PIMPL_LOCK_END;
    }
    PIMPL_GETTER(FileDescription, bool, IsDirectory);
    PIMPL_GETTER(FileDescription, ULONGLONG, Size);
    bool FileDescription::GetSizeIsValid() const noexcept { return PIMPL_(Size) != 0 && PIMPL_(Size) != MAXULONGLONG; }
    PIMPL_GETTER(FileDescription, FILETIME, ModificationTime);
    PIMPL_GETTER(FileDescription, FILETIME, CreationTime);
    PIMPL_GETTER(FileDescription, FILETIME, AccessTime);

    HRESULT FileDescription::ToStat(STATSTG* stat, bool includeName) const noexcept
    {
        COM_CHECK_POINTER(stat);

        stat->pwcsName = nullptr;
        if (includeName)
        {
            const auto sizeIncludingNullTerminator = (PIMPL_(Name).length() + 1) * sizeof(WCHAR);
            const auto string = reinterpret_cast<LPOLESTR>(::CoTaskMemAlloc(sizeIncludingNullTerminator));
            if (string == nullptr) { return E_OUTOFMEMORY; }
            std::memcpy(string, PIMPL_(Name).c_str(), sizeIncludingNullTerminator); // must not fail afterwards or we leak memory
            stat->pwcsName = string;
        }
        stat->cbSize.QuadPart = PIMPL_(Size);
        stat->mtime = PIMPL_(ModificationTime);
        stat->ctime = PIMPL_(CreationTime);
        stat->atime = PIMPL_(AccessTime);
        return S_OK;
    }

    template<typename T>
    static T GetPropertyFromArchiveItem(sevenzip::IInArchive* archive, UINT32 index, sevenzip::PropertyId propId, std::function<T(const PROPVARIANT&)> extractor)
    {
        auto propVariant = win32::propvariant();
        COM_DO_OR_THROW(archive->GetProperty(index, propId, &propVariant));
        return extractor(propVariant);
    }

    static FILETIME GetFileTimePropertyFromArchiveItem(sevenzip::IInArchive* archive, UINT32 index, sevenzip::PropertyId propId)
    {
        return GetPropertyFromArchiveItem<FILETIME>(archive, index, propId, [](const auto& propVariant)
        {
            auto ft = FILETIME();
            if (FAILED(::PropVariantToFileTime(propVariant, PSTF_UTC, &ft)))
            {
                ft.dwLowDateTime = 0;
                ft.dwHighDateTime = 0;
            }
            return ft;
        });
    }

    FileDescription FileDescription::FromArchiveItem(sevenzip::IInArchive* archive, UINT32 index)
    {
        if (archive == nullptr) { throw std::invalid_argument("archive"); }

        auto result = FileDescription();
        result.PIMPL_(Name) = GetPropertyFromArchiveItem<std::wstring>(archive, index, sevenzip::PropertyId::Path, [](const auto& propVariant) { return std::wstring(::PropVariantToStringWithDefault(propVariant, L"")); });
        result.PIMPL_(IsDirectory) = GetPropertyFromArchiveItem<bool>(archive, index, sevenzip::PropertyId::IsDir, [](const auto& propVariant) { return ::PropVariantToBooleanWithDefault(propVariant, false); });
        result.PIMPL_(Size) = GetPropertyFromArchiveItem<ULONGLONG>(archive, index, sevenzip::PropertyId::Size, [](const auto& propVariant) { return ::PropVariantToUInt64WithDefault(propVariant, MAXULONGLONG); });
        result.PIMPL_(ModificationTime) = GetFileTimePropertyFromArchiveItem(archive, index, sevenzip::PropertyId::MTime);
        result.PIMPL_(CreationTime) = GetFileTimePropertyFromArchiveItem(archive, index, sevenzip::PropertyId::CTime);
        result.PIMPL_(AccessTime) = GetFileTimePropertyFromArchiveItem(archive, index, sevenzip::PropertyId::ATime);
        return result;
    }

    FileDescription FileDescription::FromIStream(IStream* stream)
    {
        if (stream == nullptr) { throw std::invalid_argument("stream"); }

        auto stat = STATSTG();
        auto oleName = win32::unique_localmem_ptr<WCHAR[]>();
        COM_DO_OR_THROW(stream->Stat(&stat, STATFLAG_DEFAULT));
        oleName.reset(stat.pwcsName);
        auto result = FileDescription();
        result.PIMPL_(Name).assign(oleName.get());
        result.PIMPL_(IsDirectory) = false;
        result.PIMPL_(Size) = stat.cbSize.QuadPart;
        result.PIMPL_(ModificationTime) = stat.mtime;
        result.PIMPL_(CreationTime) = stat.ctime;
        result.PIMPL_(AccessTime) = stat.atime;
        return result;
    }
}
