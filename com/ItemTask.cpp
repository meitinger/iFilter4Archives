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

#include "ItemTask.hpp"

#include "settings.hpp"

#include "ReadStream.hpp"

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>

namespace com
{
    CLASS_IMPLEMENTATION(ItemTask,
                         PIMPL_CONSTRUCTOR(const FilterAttributes& attributes, streams::FileBuffer& buffer) : attributes(attributes), buffer(buffer) {}
public:
    const FilterAttributes attributes;
    streams::FileBuffer buffer;
    CLSID filterClsid;
    ULONG recursionDepth;
    std::thread gatherer;
    std::mutex m;
    std::condition_variable cv;
    std::list<CachedChunk> chunks;
    CachedChunk::IdMap idMap;
    HRESULT result = S_OK;
    bool isExtractionDone = false;
    bool isFilterDone = false;
    std::atomic_bool aborted = false;
    );

    ItemTask::ItemTask(const FilterAttributes& attributes, streams::FileBuffer& buffer, REFCLSID filterClsid, ULONG recursionDepth) : PIMPL_INIT(attributes, buffer)
    {
        PIMPL_(filterClsid) = filterClsid;
        PIMPL_(recursionDepth) = recursionDepth;
        PIMPL_(chunks).push_back(CachedChunk::FromFileDescription(buffer.Description)); // first chunk will be the file name
    }

    void ItemTask::Run()
    {
        PIMPL_(gatherer) = std::thread([PIMPL_CAPTURE]() -> void
        {
            auto hr = S_OK;
            COM_THREAD_BEGIN(COINIT_MULTITHREADED);

            // initialize the sub filter
            auto filter = IFilterPtr();
            COM_DO_OR_THROW(filter.CreateInstance(PIMPL_(filterClsid), nullptr, CLSCTX_INPROC_SERVER));
            auto initializeWithStream = IInitializeWithStreamPtr();
            if (SUCCEEDED(filter->QueryInterface<IInitializeWithStream>(&initializeWithStream)))
            {
                COM_DO_OR_THROW(initializeWithStream->Initialize(streams::ReadStream::CreateComInstance<IStream>(PIMPL_(buffer)), STGM_READ));
            }
            else
            {
                // no IInitializeWithStream, try IPersistStream
                auto persistStream = IPersistStreamPtr();
                COM_DO_OR_THROW(filter->QueryInterface<IPersistStream>(&persistStream));
                COM_DO_OR_THROW(persistStream->Load(streams::ReadStream::CreateComInstance<IStream>(PIMPL_(buffer))));
            }
            if (PIMPL_(filterClsid) == __uuidof(Filter))
            {
                // propagate recursion depth
                auto filter4Archives = IFilter4ArchivesPtr();
                COM_DO_OR_THROW(filter->QueryInterface<IFilter4Archives>(&filter4Archives));
                COM_DO_OR_THROW(filter4Archives->SetRecursionDepth(PIMPL_(recursionDepth)));
            }
            COM_DO_OR_THROW(PIMPL_(attributes).Init(filter));

            // query all chunks (unless the task got aborted)
            while (!PIMPL_(aborted))
            {
                auto chunk = CachedChunk::FromFilter(filter);
                if (FAILED(chunk.Code)) { break; } // Windows kills us if we report any error, do the same with the sub-filter

                // enqueue the chunk
                PIMPL_LOCK_BEGIN(m);
                PIMPL_(chunks).push_back(chunk);
                PIMPL_LOCK_END;
                PIMPL_(cv).notify_all();
            }

            COM_THREAD_END(hr);

            // signal end and store the result
            PIMPL_LOCK_BEGIN(m);
            if (SUCCEEDED(PIMPL_(result)))
            {
                PIMPL_(result) = hr;
            }
            PIMPL_(isFilterDone) = true;
            PIMPL_LOCK_END;
            PIMPL_(cv).notify_all();
        });
    }

    void ItemTask::SetEndOfExtraction(HRESULT hr)
    {
        // signal end of extraction for the task and stream
        PIMPL_LOCK_BEGIN(m);
        if (FAILED(hr))
        {
            PIMPL_(result) = hr; // extraction errors always override filter failures
        }
        PIMPL_(isExtractionDone) = true;
        PIMPL_LOCK_END;
        PIMPL_(cv).notify_all();
        PIMPL_(buffer).SetEndOfFile();
    }

    void ItemTask::Abort()
    {
        // signal abort and wait for the thread to end
        PIMPL_(aborted) = true;
        if (PIMPL_(gatherer).joinable())
        {
            PIMPL_(gatherer).join();
        }
    }

    std::optional<CachedChunk> ItemTask::NextChunk(ULONG id)
    {
        // needs to join gatherer on final block!
        PIMPL_LOCK_BEGIN(m);
        PIMPL_WAIT(m, cv, !PIMPL_(chunks).empty() || PIMPL_(isFilterDone) && PIMPL_(isExtractionDone));
        if (!PIMPL_(chunks).empty())
        {
            // dequeue the chunk
            auto result = PIMPL_(chunks).front();
            result.Map(id, PIMPL_(idMap));
            PIMPL_(chunks).pop_front();
            return result;
        }
        else if (!SUCCEEDED(PIMPL_(result)))
        {
            // there was an error, so the last chunk is an error chunk
            auto result = CachedChunk::FromHResult(PIMPL_(result));
            result.Map(id, PIMPL_(idMap));
            PIMPL_(result) = S_OK;
            return result;
        }
        PIMPL_LOCK_END;

        // wait for the thread and return
        if (PIMPL_(gatherer).joinable())
        {
            PIMPL_(gatherer).join();
        }
        return std::nullopt;
    }
}
