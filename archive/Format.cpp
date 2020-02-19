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

#include "Format.hpp"

#include "com.hpp"

namespace archive
{
    CLASS_IMPLEMENTATION(Format,
                         PIMPL_CONSTRUCTOR(const Module& library) : Library(library) {}
public:
    const Module Library;
    GUID clsid;
    std::wstring defaultName;
    std::wstring Name;
    ExtensionsCollection Extensions;
    );

    Format::Format(const Module& library, UINT32 index) : PIMPL_INIT(library)
    {
        auto propv = win32::propvariant();

        // name
        PIMPL_(defaultName).append(STR("#")).append(std::to_wstring(index));
        COM_DO_OR_THROW(library.GetFormatProperty(index, sevenzip::HandlerPropertyId::Name, propv));
        PIMPL_(Name) = ::PropVariantToStringWithDefault(propv, PIMPL_(defaultName).c_str());
        propv.clear();

        // clsid
        COM_DO_OR_THROW(library.GetFormatProperty(index, sevenzip::HandlerPropertyId::ClassID, propv));
        if (propv.vt != VT_BSTR) { COM_THROW(E_NOT_SET); }
        std::memcpy(&PIMPL_(clsid), reinterpret_cast<GUID*>(propv.bstrVal), sizeof(GUID)); // alas, GUIDs aren't stored properly
        propv.clear();

        // extensions
        COM_DO_OR_THROW(library.GetFormatProperty(index, sevenzip::HandlerPropertyId::Extension, propv));
        auto exts = std::wstring(::PropVariantToStringWithDefault(propv, STR("").c_str()));
        const auto result = _wcslwr_s(exts.data(), exts.length() + 1);
        if (result != 0) { throw std::system_error(result, std::generic_category()); }
        propv.clear();

        // split extensions
        auto offset = std::size_t();
        while ((offset = exts.find_last_of(CHR(' '))) != std::string::npos)
        {
            if (offset + 1 < exts.length()) // ignore empty extensions
            {
                PIMPL_(Extensions).insert(CHR('.') + exts.substr(offset + 1));
            }
            exts.resize(offset);
        }
        if (!exts.empty())
        {
            PIMPL_(Extensions).insert(CHR('.') + exts);
        }
    }

    PIMPL_GETTER(Format, const Module&, Library);
    PIMPL_GETTER(Format, const std::wstring&, Name);
    PIMPL_GETTER(Format, const Format::ExtensionsCollection&, Extensions);

    sevenzip::IInArchivePtr Format::CreateArchive() const
    {
        auto ptr = sevenzip::IInArchivePtr();
        COM_DO_OR_THROW(PIMPL_(Library).CreateObject(PIMPL_(clsid), __uuidof(sevenzip::IInArchive), *reinterpret_cast<void**>(&ptr)));
        if (!ptr) { COM_THROW(E_NOINTERFACE); } // this check is done because we don't blindly trust the result of modules
        return ptr;
    }
}
