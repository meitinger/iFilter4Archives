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

    COM_CLASS_DECLARATION(Filter, com::object IMPLEMENTS(IFilter) IMPLEMENTS(IInitializeWithStream) IMPLEMENTS(IPersistStream) IMPLEMENTS(sevenzip::IArchiveExtractCallback) IMPLEMENTS(IFilter4Archives),
                          COM_VISIBLE(IFilter, IInitializeWithStream, IPersistStream, sevenzip::IArchiveExtractCallback, IFilter4Archives)
public:
    Filter();

    STDMETHOD_(SCODE, Init)(ULONG grfFlags, ULONG cAttributes, const FULLPROPSPEC* aAttributes, ULONG* pFlags);
    STDMETHOD_(SCODE, GetChunk)(STAT_CHUNK* pStat);
    STDMETHOD_(SCODE, GetText)(ULONG* pcwcBuffer, WCHAR* awcBuffer);
    STDMETHOD_(SCODE, GetValue)(PROPVARIANT** ppPropValue);
    STDMETHOD_(SCODE, BindRegion)(FILTERREGION origPos, REFIID riid, void** ppunk);

    STDMETHOD(Initialize)(IStream* pstream, DWORD grfMode);

    STDMETHOD(GetClassID)(CLSID* pClassID);
    STDMETHOD(IsDirty)(void);
    STDMETHOD(Load)(IStream* pStm);
    STDMETHOD(Save)(IStream* pStm, BOOL fClearDirty);
    STDMETHOD(GetSizeMax)(ULARGE_INTEGER* pcbSize);

    STDMETHOD(SetTotal)(UINT64 total);
    STDMETHOD(SetCompleted)(const UINT64* completeValue);
    STDMETHOD(GetStream)(UINT32 index, sevenzip::ISequentialOutStream** outStream, sevenzip::AskMode askExtractMode);
    STDMETHOD(PrepareOperation)(sevenzip::AskMode askExtractMode);
    STDMETHOD(SetOperationResult)(sevenzip::OperationResult opRes);

    STDMETHOD(SetRecursionDepth)(ULONG depth);
    );

    /******************************************************************************/

    SIMPLE_CLASS_DECLARATION(FilterAttributes,
public:
    FilterAttributes(ULONG grfFlags, ULONG cAttributes, const FULLPROPSPEC* aAttributes);

    HRESULT Init(IFilter* filter) const noexcept;
    );
}
