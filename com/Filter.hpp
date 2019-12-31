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

namespace com
{
    struct DECLSPEC_UUID("E22C9972-6449-4137-BA03-D75B570A0251") IFilter4Archives; // allows communication between different filter instances
    class DECLSPEC_UUID("DD88FF21-CD20-449E-B0B1-E84B1911F381") Filter; // the main interface to/for the Windows Search service and 7-Zip
    class FilterAttributes; // holds all initialization flags and properties for an iFilter

    /******************************************************************************/

    struct DECLSPEC_NOVTABLE IFilter4Archives : public IUnknown
    {
        STDMETHOD(SetRecursionDepth)(ULONG depth) PURE;
    };
    _COM_SMARTPTR_TYPEDEF(IFilter4Archives, __uuidof(IFilter4Archives));

    /******************************************************************************/

    COM_CLASS_DECLARATION(Filter, com::object
                          IMPLEMENTS(IFilter) IMPLEMENTS(IInitializeWithStream) IMPLEMENTS(IPersistStream) IMPLEMENTS(IPersistFile)
                          IMPLEMENTS(sevenzip::IArchiveExtractCallback) IMPLEMENTS(IFilter4Archives),
                          COM_VISIBLE(IFilter, IInitializeWithStream, IPersistStream, IPersistFile, IFilter4Archives)
public:
    Filter();

    STDMETHOD_(SCODE, Init)(ULONG grfFlags, ULONG cAttributes, const FULLPROPSPEC* aAttributes, ULONG* pFlags) noexcept override; // IFilter
    STDMETHOD_(SCODE, GetChunk)(STAT_CHUNK* pStat) noexcept override; // IFilter
    STDMETHOD_(SCODE, GetText)(ULONG* pcwcBuffer, WCHAR* awcBuffer) noexcept override; // IFilter
    STDMETHOD_(SCODE, GetValue)(PROPVARIANT** ppPropValue) noexcept override; // IFilter
    STDMETHOD_(SCODE, BindRegion)(FILTERREGION origPos, REFIID riid, void** ppunk) noexcept override; // IFilter

    STDMETHOD(Initialize)(IStream* pstream, DWORD grfMode) noexcept override; // IInitializeWithStream

    STDMETHOD(GetClassID)(CLSID* pClassID) noexcept override; // IPersist
    STDMETHOD(IsDirty)(void) noexcept override; // IPersistStream & IPersistFile

    STDMETHOD(Load)(IStream* pStm) noexcept override; // IPersistStream
    STDMETHOD(Save)(IStream* pStm, BOOL fClearDirty) noexcept override; // IPersistStream
    STDMETHOD(GetSizeMax)(ULARGE_INTEGER* pcbSize) noexcept override; // IPersistStream

    STDMETHOD(Load)(LPCOLESTR pszFileName, DWORD dwMode) noexcept override; // IPersistFile
    STDMETHOD(Save)(LPCOLESTR pszFileName, BOOL fRemember) noexcept override; // IPersistFile
    STDMETHOD(SaveCompleted)(LPCOLESTR pszFileName) noexcept override; // IPersistFile
    STDMETHOD(GetCurFile)(LPOLESTR* ppszFileName) noexcept override; // IPersistFile

    STDMETHOD(SetTotal)(UINT64 total) noexcept override; // IProgress
    STDMETHOD(SetCompleted)(const UINT64* completeValue) noexcept override; // IProgress
    STDMETHOD(GetStream)(UINT32 index, sevenzip::ISequentialOutStream** outStream, sevenzip::AskMode askExtractMode) noexcept override; // IArchiveExtractCallback
    STDMETHOD(PrepareOperation)(sevenzip::AskMode askExtractMode) noexcept override; // IArchiveExtractCallback
    STDMETHOD(SetOperationResult)(sevenzip::OperationResult opRes) noexcept override; // IArchiveExtractCallback

    STDMETHOD(SetRecursionDepth)(ULONG depth) noexcept override; // IFilter4Archives
    );

    /******************************************************************************/

    SIMPLE_CLASS_DECLARATION(FilterAttributes,
public:
    FilterAttributes(ULONG grfFlags, ULONG cAttributes, const FULLPROPSPEC* aAttributes);

    HRESULT Init(IFilter* filter) const noexcept;
    );
}
