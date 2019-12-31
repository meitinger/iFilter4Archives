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

#include "CachedChunk.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace com
{
    struct PropvariantCacheDeleter
    {
        void operator()(PROPVARIANT* pPropValue) noexcept
        {
            ::PropVariantClear(pPropValue);
            ::CoTaskMemFree(pPropValue);
        }
    };
    using unique_propvariant_cache_ptr = std::unique_ptr<PROPVARIANT, PropvariantCacheDeleter>;

    /******************************************************************************/

    SIMPLE_CLASS_IMPLEMENTATION(CachedChunk,
public:
    SCODE statResult;
    STAT_CHUNK stat;
    std::wstring propName;
    std::vector<WCHAR> text;
    unique_propvariant_cache_ptr value;
    size_t textOffset = 0;
    bool mapped = false;
    );

    CachedChunk::CachedChunk() : PIMPL_INIT() {}

    SCODE CachedChunk::GetCode() const noexcept { return PIMPL_(statResult); }

    SCODE CachedChunk::GetChunk(STAT_CHUNK* pStat) noexcept
    {
        *pStat = PIMPL_(stat);
        return PIMPL_(statResult);
    }

    SCODE CachedChunk::GetText(ULONG* pcwcBuffer, WCHAR* awcBuffer) noexcept
    {
        COM_CHECK_POINTER(pcwcBuffer);
        COM_CHECK_POINTER(awcBuffer);
        if (*pcwcBuffer == 0) { return E_NOT_SUFFICIENT_BUFFER; } // need at least space for the null terminator
        if (!SUCCEEDED(PIMPL_(statResult)) || !(PIMPL_(stat).flags & CHUNKSTATE::CHUNK_TEXT)) { return FILTER_E_NO_TEXT; }
        if (PIMPL_(textOffset) >= PIMPL_(text).size()) { return FILTER_E_NO_MORE_TEXT; }

        const auto remaining = PIMPL_(text).size() - PIMPL_(textOffset);
        if (*pcwcBuffer >= remaining + 1) // plus one for the null terminator
        {
            std::wmemcpy(awcBuffer, PIMPL_(text).data() + PIMPL_(textOffset), remaining);
            awcBuffer[remaining] = L'\0';
            *pcwcBuffer = static_cast<ULONG>(remaining);
            PIMPL_(textOffset) += remaining;
            return FILTER_S_LAST_TEXT;
        }
        else
        {
            (*pcwcBuffer)--; // always null terminate strings but don't count it as copied character
            std::wmemcpy(awcBuffer, PIMPL_(text).data() + PIMPL_(textOffset), *pcwcBuffer);
            awcBuffer[*pcwcBuffer] = L'\0';
            PIMPL_(textOffset) += *pcwcBuffer;
            return S_OK;
        }
    }

    SCODE CachedChunk::GetValue(PROPVARIANT** ppPropValue) noexcept
    {
        COM_CHECK_POINTER_AND_SET(ppPropValue, nullptr);
        if (!SUCCEEDED(PIMPL_(statResult)) || !(PIMPL_(stat).flags & CHUNKSTATE::CHUNK_VALUE)) { return FILTER_E_NO_VALUES; }
        if (!PIMPL_(value)) { return FILTER_E_NO_MORE_VALUES; } // GetValue has already been called
        *ppPropValue = PIMPL_(value).release();
        return S_OK;
    }

    void CachedChunk::Map(ULONG newId, IdMap& idMap)
    {
        if (PIMPL_(mapped))
        {
            // ensure map isn't called twice with different ids
            if (newId != PIMPL_(stat).idChunk) { throw std::invalid_argument("newId"); }
            return;
        }
        idMap[PIMPL_(stat).idChunk] = newId;
        PIMPL_(stat).idChunk = newId;
        if (PIMPL_(stat).idChunkSource != 0)
        {
            // also try to map the source chunk id
            const auto sourceIdEntry = idMap.find(PIMPL_(stat).idChunkSource);
            PIMPL_(stat).idChunkSource = sourceIdEntry == idMap.end() ? 0 : sourceIdEntry->second;
        }
        PIMPL_(mapped) = true;
    }

    CachedChunk CachedChunk::FromFilter(IFilter* filter)
    {
        if (filter == nullptr) { throw std::invalid_argument("filter"); }

        auto result = CachedChunk();
        auto& stat = result.PIMPL_(stat);
        if (SUCCEEDED(result.PIMPL_(statResult) = filter->GetChunk(&stat)))
        {
            // create a copy of the prop name (the filter's DLL might get unloaded)
            if (stat.attribute.psProperty.ulKind == PRSPEC_LPWSTR)
            {
                auto& propName = result.PIMPL_(propName);
                propName.assign(stat.attribute.psProperty.lpwstr);
                stat.attribute.psProperty.lpwstr = const_cast<wchar_t*>(propName.c_str()); // is handled as const
            }

            // cache the chunk's text
            if (stat.flags & CHUNKSTATE::CHUNK_TEXT)
            {
                auto& text = result.PIMPL_(text);
                while (true)
                {
                    auto length = ULONG(8000); // with null terminator on in, without null terminator on out
                    const auto offset = text.size();
                    text.resize(offset + length);
                    const auto textResult = filter->GetText(&length, text.data() + offset);
                    if (FAILED(textResult))
                    {
                        if (textResult != FILTER_E_NO_MORE_TEXT)
                        {
                            // remove the text on failures
                            text.clear();
                            stat.flags = static_cast<CHUNKSTATE>(stat.flags & ~CHUNKSTATE::CHUNK_TEXT);
                        }
                        break;
                    }
                    text.resize(offset + length);
                    if (textResult == FILTER_S_LAST_TEXT) { break; } // no need to call GetText again
                }
                text.shrink_to_fit();
            }

            // cache the chunk's value
            if (stat.flags & CHUNKSTATE::CHUNK_VALUE)
            {
                auto& value = result.PIMPL_(value);
                auto propVariantPtr = LPPROPVARIANT();
                const auto valueResult = filter->GetValue(&propVariantPtr);
                value.reset(propVariantPtr);
                if (FAILED(valueResult))
                {
                    // remove the value on failures
                    value.release(); // should be nullptr anyway, might be garbage
                    stat.flags = static_cast<CHUNKSTATE>(stat.flags & ~CHUNKSTATE::CHUNK_VALUE);
                }
            }
        }
        return result;
    }

    CachedChunk CachedChunk::FromHResult(HRESULT hr)
    {
        if (SUCCEEDED(hr)) { throw std::invalid_argument("hr"); }

        // for cases like FILTER_E_PASSWORD
        auto result = CachedChunk();
        result.PIMPL_(statResult) = hr;
        return result;
    }
}
