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

#include "Filter.hpp"

#include "settings.hpp"

#include "BridgeStream.hpp"
#include "CachedChunk.hpp"
#include "Factory.hpp"
#include "FileDescription.hpp"
#include "ItemTask.hpp"
#include "Registrar.hpp"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>
#include <string>

namespace com
{
    class ExtractCallbackForwarder : public sevenzip::IArchiveExtractCallback // ensures that 7-Zip doesn't mess with com::Filter's refcount or queries additional interfaces
    {
    private:
        std::atomic<ULONG> _refCount = 0;
        sevenzip::IArchiveExtractCallback* const _inner;

    public:
        explicit ExtractCallbackForwarder(sevenzip::IArchiveExtractCallback* inner) noexcept : _inner(inner) {}
        ~ExtractCallbackForwarder() noexcept { assert(_refCount == 0); } // check whether 7-Zip released all references

        STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) noexcept override
        {
            COM_CHECK_POINTER_AND_SET(ppvObject, nullptr);
            if (riid == __uuidof(IUnknown) || riid == __uuidof(sevenzip::IProgress) || riid == __uuidof(sevenzip::IArchiveExtractCallback))
            {
                *ppvObject = this;
                return S_OK;
            }
            return E_NOINTERFACE;
        }
        STDMETHOD_(ULONG, AddRef)(void) noexcept override { return ++_refCount; }
        STDMETHOD_(ULONG, Release)(void) noexcept override { return --_refCount; }
        STDMETHOD(SetTotal)(UINT64 total) noexcept override { return _inner->SetTotal(total); }
        STDMETHOD(SetCompleted)(const UINT64* completeValue) noexcept override { return _inner->SetCompleted(completeValue); }
        STDMETHOD(GetStream)(UINT32 index, sevenzip::ISequentialOutStream** outStream, sevenzip::AskMode askExtractMode) noexcept override { return _inner->GetStream(index, outStream, askExtractMode); }
        STDMETHOD(PrepareOperation)(sevenzip::AskMode askExtractMode) noexcept override { return _inner->PrepareOperation(askExtractMode); }
        STDMETHOD(SetOperationResult)(sevenzip::OperationResult opRes) noexcept override { return _inner->SetOperationResult(opRes); }
    };

    /******************************************************************************/

    CLASS_IMPLEMENTATION(Filter,
                         PIMPL_DECONSTRUCTOR() { AbortAnyExtractionOrTasksAndReset(); }
public:
    // used exclusively in the Windows thread
    std::optional<FilterAttributes> attributes;
    IStreamPtr stream;
    sevenzip::IInArchivePtr archive;
    std::thread extractor;
    ULONG currentChunkId;
    std::optional<CachedChunk> currentChunk;
    std::optional<ItemTask> currentChunkTask;

    // shared between extractor and Windows thread, must not be synced
    std::mutex m;
    std::condition_variable cv;
    ULONG recursionDepth = 0;

    // shared between extractor and Windows thread, must be synced
    std::list<ItemTask> tasks;
    bool extractionFinished;
    HRESULT extractionResult;
    bool abortExtraction;

    // used exclusively in the extractor thread
    const Registrar registrar;
    std::optional<ItemTask> currentExtractTask;

    // called from Windows thread (IFilter::Init, final IUnknown::Release)
    void AbortAnyExtractionOrTasksAndReset()
    {
        // signal abort (needs to be synced if there is an extractor running)
        auto lk = std::unique_lock<std::mutex>(m);
        abortExtraction = true;
        lk.unlock();
        cv.notify_all();

        // wait for the extractor to finish
        if (extractor.joinable())
        {
            extractor.join();
        }
        abortExtraction = false;

        // reset, no need to sync, but do it in a way that in theory IFilter::Get* can be called even after failure
        if (currentChunkTask)
        {
            currentChunkTask->Abort();
            currentChunkTask = std::nullopt;
        }
        while (!tasks.empty())
        {
            currentChunkTask = tasks.front();
            tasks.pop_front();
            currentChunkTask->Abort();
            currentChunkTask = std::nullopt;
        }
        currentChunk = std::nullopt;
        currentChunkId = 0;
    }
    );

    //----------------------------------------------------------------------------//

    static void EndExtractionTaskIfAny(std::optional<ItemTask>& task) // will not call COM
    {
        // end and clear the task if there is one
        if (task)
        {
            task->SetEndOfExtraction();
            task = std::nullopt;
        }
    }

    //----------------------------------------------------------------------------//

    Filter::Filter() : PIMPL_INIT() {}

    //----------------------------------------------------------------------------//

    STDMETHODIMP_(SCODE) Filter::Init(ULONG grfFlags, ULONG cAttributes, const FULLPROPSPEC* aAttributes, ULONG* pFlags) noexcept // called from Windows thread
    {
        COM_CHECK_ARG(cAttributes == 0 || aAttributes != nullptr); // according to doc better than E_POINTER
        COM_CHECK_POINTER_AND_SET(pFlags, 0);
        if (!PIMPL_(stream)) { return E_FAIL; } // according to doc
        COM_NOTHROW_BEGIN;

        // prerequisites
        PIMPL_(AbortAnyExtractionOrTasksAndReset)(); // Init method might be called multiple times, so stop any running extraction
        COM_DO_OR_RETURN(PIMPL_(stream)->Seek(LARGE_INTEGER(), STREAM_SEEK_SET, nullptr)); // rewind the stream (necessary for iFiltTst)

        // capture the attributes and open the archive
        PIMPL_(attributes) = FilterAttributes(grfFlags, cAttributes, aAttributes);
        PIMPL_(archive) = archive::Factory::CreateArchiveFromExtension(FileDescription::FromIStream(PIMPL_(stream)).Extension);
        const auto scanSize = UINT64(1 << 23); // taken from 7-Zip source
        COM_DO_OR_RETURN(PIMPL_(archive)->Open(streams::BridgeStream::CreateComInstance<sevenzip::IInStream>(PIMPL_(stream)), &scanSize, nullptr));

        // start the extractor thread
        PIMPL_(extractionFinished) = false; // no need to sync yet
        PIMPL_(extractionResult) = S_OK;
        PIMPL_(extractor) = std::thread([PIMPL_CAPTURE, callback = this]() -> void
        {
            COM_THREAD_BEGIN(COINITBASE_MULTITHREADED);

            // extract everything and close the archive
            COM_DO_OR_THROW(PIMPL_(archive)->Extract(nullptr, MAXUINT32, 0, &ExtractCallbackForwarder(callback)));
            COM_DO_OR_THROW(PIMPL_(archive)->Close());

            COM_THREAD_END(PIMPL_(extractionResult));

            // necessary if 7-Zip formats don't call SetOperationResult, this must succeed to avoid deadlocks
            EndExtractionTaskIfAny(PIMPL_(currentExtractTask)); // will not call COM

            // signal finished
            PIMPL_LOCK_BEGIN(m);
            PIMPL_(extractionFinished) = true;
            PIMPL_LOCK_END;
            PIMPL_(cv).notify_all();
        });
        return S_OK;

        COM_NOTHROW_END;
    }

    STDMETHODIMP_(SCODE) Filter::GetChunk(STAT_CHUNK* pStat) noexcept // called from Windows thread
    {
        COM_NOTHROW_BEGIN;

        if (!PIMPL_(currentChunkTask))
        {
        get_next_task:
            PIMPL_LOCK_BEGIN(m);
            PIMPL_WAIT(m, cv, !PIMPL_(tasks).empty() || PIMPL_(extractionFinished));
            if (PIMPL_(tasks).empty()) { goto finished; } // all done, nothing more to come, need to exit lock
            PIMPL_(currentChunkTask) = PIMPL_(tasks).front();
            PIMPL_(tasks).pop_front();
            PIMPL_LOCK_END;
            PIMPL_(cv).notify_all(); // notify extractor that another task may be queued
        }
        PIMPL_(currentChunk) = PIMPL_(currentChunkTask)->NextChunk(++PIMPL_(currentChunkId));
        if (!PIMPL_(currentChunk))
        {
            // this item is done, fetch the next
            PIMPL_(currentChunkTask) = std::nullopt;
            goto get_next_task;
        }
        return PIMPL_(currentChunk)->GetChunk(pStat);

    finished:
        PIMPL_(currentChunk) = std::nullopt; // should already be the case
        if (FAILED(PIMPL_(extractionResult)))
        {
            const auto hr = PIMPL_(extractionResult);
            PIMPL_(extractionResult) = S_OK;
            return hr;
        }
        if (PIMPL_(extractor).joinable())
        {
            PIMPL_(extractor).join(); // thread should be done as well, join to avoid race condition with destructor
        }
        return FILTER_E_END_OF_CHUNKS;

        COM_NOTHROW_END;
    }

    STDMETHODIMP_(SCODE) Filter::GetText(ULONG* pcwcBuffer, WCHAR* awcBuffer) noexcept // called from Windows thread
    {
        return PIMPL_(currentChunk) ? PIMPL_(currentChunk)->GetText(pcwcBuffer, awcBuffer) : FILTER_E_NO_MORE_TEXT;
    }

    STDMETHODIMP_(SCODE) Filter::GetValue(PROPVARIANT** ppPropValue) noexcept // called from Windows thread
    {
        return PIMPL_(currentChunk) ? PIMPL_(currentChunk)->GetValue(ppPropValue) : FILTER_E_NO_MORE_VALUES;
    }

    STDMETHODIMP_(SCODE) Filter::BindRegion(FILTERREGION origPos, REFIID riid, void** ppunk) noexcept { return E_NOTIMPL; } // see iFilter docs

    //----------------------------------------------------------------------------//

    STDMETHODIMP Filter::Initialize(IStream* pstream, DWORD grfMode) noexcept // called from Windows thread
    {
        COM_CHECK_POINTER(pstream);
        COM_CHECK_ARG(grfMode == STGM_READ || grfMode == STGM_READWRITE);
        if (PIMPL_(stream)) { return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED); } // according to docs
        PIMPL_(stream) = pstream; // will call AddRef
        return S_OK;
    }

    //----------------------------------------------------------------------------//

    STDMETHODIMP Filter::GetClassID(CLSID* pClassID) noexcept // called from Windows thread
    {
        COM_CHECK_POINTER_AND_SET(pClassID, __uuidof(Filter));
        return S_OK;
    }

    STDMETHODIMP Filter::IsDirty(void) noexcept { return E_NOTIMPL; } // see iFilter docs

    //----------------------------------------------------------------------------//

    STDMETHODIMP Filter::Load(IStream* pStm) noexcept // called from Windows thread
    {
        COM_CHECK_POINTER(pStm);
        PIMPL_(stream) = pStm; // will call AddRef and release the old one
        return S_OK;
    }

    STDMETHODIMP Filter::Save(IStream* pStm, BOOL fClearDirty) noexcept { return E_NOTIMPL; } // see iFilter docs

    STDMETHODIMP Filter::GetSizeMax(ULARGE_INTEGER* pcbSize) noexcept { return E_NOTIMPL; } // see iFilter docs

    //----------------------------------------------------------------------------//

    STDMETHODIMP Filter::Load(LPCOLESTR pszFileName, DWORD dwMode) noexcept // called from Windows thread
    {
#pragma comment(lib, "Shlwapi")
        return ::SHCreateStreamOnFileEx(pszFileName, dwMode, FILE_ATTRIBUTE_READONLY, false, nullptr, &PIMPL_(stream));
    }

    STDMETHODIMP Filter::Save(LPCOLESTR pszFileName, BOOL fRemember) noexcept { return E_NOTIMPL; } // see iFilter docs

    STDMETHODIMP Filter::SaveCompleted(LPCOLESTR pszFileName) noexcept { return E_NOTIMPL; } // see iFilter docs

    STDMETHODIMP Filter::GetCurFile(LPOLESTR* ppszFileName) noexcept { return E_NOTIMPL; } // see iFilter docs

    //----------------------------------------------------------------------------//

    STDMETHODIMP Filter::SetTotal(UINT64 total) noexcept { return S_OK; } // no status needed

    STDMETHODIMP Filter::SetCompleted(const UINT64* completeValue) noexcept { return S_OK; } // no status needed

    STDMETHODIMP Filter::GetStream(UINT32 index, sevenzip::ISequentialOutStream** outStream, sevenzip::AskMode askExtractMode) noexcept // called from extraction thread
    {
        COM_CHECK_POINTER_AND_SET(outStream, nullptr); // if we return S_OK with outStream set to NULL the entry is skipped
        COM_CHECK_STATE(PIMPL_(attributes) && PIMPL_(archive)); // ensure all required members are set
        if (askExtractMode != sevenzip::AskMode::Extract) { return S_OK; } // do nothing if not extracting (e.g. corrupted archive)
        auto streamPtr = sevenzip::ISequentialOutStreamPtr(); // reserve the com pointer and enter nothrow
        COM_NOTHROW_BEGIN;

        // end any pending task and create the current one
        EndExtractionTaskIfAny(PIMPL_(currentExtractTask));
        PIMPL_(currentExtractTask) = ItemTask(FileDescription::FromArchiveItem(PIMPL_(archive), index));

        // limit concurrency and enqueue the task
        PIMPL_LOCK_BEGIN(m);
        PIMPL_WAIT(m, cv, PIMPL_(tasks).size() <= settings::concurrent_filter_threads() || PIMPL_(abortExtraction));
        if (PIMPL_(abortExtraction)) { return E_ABORT; } // this will abort the entire extraction, not just the current entry
        PIMPL_(tasks).push_back(*PIMPL_(currentExtractTask));
        PIMPL_LOCK_END;
        PIMPL_(cv).notify_all(); // let GetChunk know

        // start the task (needs to be after enqueue to ensure ItemTask::Abort will get called if necessary)
        streamPtr = PIMPL_(currentExtractTask)->Run(*PIMPL_(attributes), PIMPL_(registrar), PIMPL_(recursionDepth));

        // leave nothrow and return the pointer
        COM_NOTHROW_END;
        *outStream = streamPtr.Detach();
        return S_OK;
    }

    STDMETHODIMP Filter::PrepareOperation(sevenzip::AskMode askExtractMode) noexcept { return S_OK; } // result sometimes ignored by 7-Zip

    STDMETHODIMP Filter::SetOperationResult(sevenzip::OperationResult opRes) noexcept { return S_OK; } // ignore result (can't change these failures)

    //----------------------------------------------------------------------------//

    STDMETHODIMP Filter::SetRecursionDepth(ULONG depth) noexcept // called from ItemTask thread (acting as Windows Thread)
    {
        PIMPL_(recursionDepth) = depth;
        return S_OK;
    }

    /******************************************************************************/

    CLASS_IMPLEMENTATION(FilterAttributes,
public:
    ULONG flags;
    std::vector<FULLPROPSPEC> attributes;
    std::list<std::wstring> attributeNames;
    );

    FilterAttributes::FilterAttributes(ULONG grfFlags, ULONG cAttributes, const FULLPROPSPEC* aAttributes) : PIMPL_INIT()
    {
        PIMPL_(flags) = grfFlags;
        PIMPL_(attributes).resize(cAttributes);
        std::memcpy(PIMPL_(attributes).data(), aAttributes, sizeof(FULLPROPSPEC) * cAttributes);

        // use emplace_back with a list to ensure that no string gets reallocated
        for (auto i = ULONG(0); i < cAttributes; i++)
        {
            if (aAttributes[i].psProperty.ulKind == PRSPEC_LPWSTR)
            {
                PIMPL_(attributes)[i].psProperty.lpwstr = const_cast<wchar_t*>(PIMPL_(attributeNames).emplace_back(aAttributes[i].psProperty.lpwstr).c_str());
            }
        }
    }

    HRESULT FilterAttributes::Init(IFilter* filter) const noexcept
    {
        COM_CHECK_POINTER(filter);

        // initialize the filter and ignore IFILTER_FLAGS_OLE_PROPERTIES if set
        auto flags = ULONG(0);
        return filter->Init(PIMPL_(flags), static_cast<ULONG>(PIMPL_(attributes).size()), PIMPL_(attributes).data(), &flags);
    }
}
