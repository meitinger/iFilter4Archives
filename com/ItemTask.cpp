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

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>

namespace com
{
    SIMPLE_CLASS_IMPLEMENTATION(ItemTask,
                                PIMPL_CONSTRUCTOR(const FilterAttributes& attributes) : attributes(attributes) {}
public:
    const FilterAttributes attributes;
    CLSID filterClsid;
    std::unique_ptr<streams::WriteStream> writeStream;
    ULONG recursionDepth;
    DWORD maxConsecutiveErrors;
    std::thread gatherer;
    std::mutex m;
    std::condition_variable cv;
    std::list<CachedChunk> chunks;
    ChunkIdMap idMap;
    HRESULT result = S_OK;
    bool isExtractionDone = false;
    bool isFilterDone = false;
    std::atomic_bool aborted = false;
    );

    ItemTask::ItemTask(const FilterAttributes& attributes, REFCLSID filterClsid, std::unique_ptr<streams::WriteStream> writeStream, ULONG recursionDepth) : PIMPL_INIT(attributes)
    {
        if (!writeStream) { throw std::invalid_argument("writeStream"); }

        PIMPL_(filterClsid) = filterClsid;
        PIMPL_(writeStream) = std::move(writeStream);
        PIMPL_(recursionDepth) = recursionDepth;
        PIMPL_(maxConsecutiveErrors) = settings::allowed_consecutive_get_chunk_errors_before_fail() / recursionDepth;
    }

    void ItemTask::Run()
    {
        PIMPL_(gatherer) = std::thread([*this]() // this needs to be copied, since it will either go out of scope, or worse: reused at the same stack location
        {
            auto hr = S_OK;
            try
            {
                // initialize the sub filter
                COM_DO_OR_THROW(::CoInitialize(nullptr));
                auto filter = IFilterPtr();
                COM_DO_OR_THROW(::CoCreateInstance(PIMPL_(filterClsid), nullptr, CLSCTX_INPROC_SERVER, IID_IFilter, reinterpret_cast<LPVOID*>(&filter)));
                auto initializeWithStream = IInitializeWithStreamPtr();
                if (SUCCEEDED(filter->QueryInterface<IInitializeWithStream>(&initializeWithStream)))
                {
                    COM_DO_OR_THROW(initializeWithStream->Initialize(PIMPL_(writeStream)->OpenReadStream(), STGM_READ));
                }
                else
                {
                    // no IInitializeWithStream, try IPersistStream
                    auto persistStream = IPersistStreamPtr();
                    COM_DO_OR_THROW(filter->QueryInterface<IPersistStream>(&persistStream));
                    COM_DO_OR_THROW(persistStream->Load(PIMPL_(writeStream)->OpenReadStream()));
                }
                if (PIMPL_(filterClsid) == __uuidof(Filter))
                {
                    // propagate recursion depth
                    auto filter4Archives = IFilter4ArchivesPtr();
                    COM_DO_OR_THROW(filter->QueryInterface<IFilter4Archives>(&filter4Archives));
                    filter4Archives->SetRecursionDepth(PIMPL_(recursionDepth));
                }
                COM_DO_OR_THROW(PIMPL_(attributes).Init(filter));

                // query all chunks (unless the task got aborted)
                auto consecutiveErrors = DWORD(0);
                while (!PIMPL_(aborted))
                {
                    auto chunk = CachedChunk::FromFilter(filter);
                    if (chunk.Code == FILTER_E_END_OF_CHUNKS) { break; } // nothing more to come
                    if (FAILED(chunk.Code))
                    {
                        if (++consecutiveErrors >= PIMPL_(maxConsecutiveErrors)) { break; } // too many failures in a row
                    }
                    else { consecutiveErrors = 0; } // got at least one valid chunk

                    // enqueue the chunk
                    PIMPL_LOCK_BEGIN(m);
                    PIMPL_(chunks).push_back(chunk);
                    PIMPL_LOCK_END;
                    PIMPL_(cv).notify_all();
                }
            }
            catch (const std::bad_alloc&) { hr = E_OUTOFMEMORY; }
            catch (const std::system_error & e) { hr = utils::hresult_from_system_error(e); }
            catch (...) { hr = E_UNEXPECTED; }

            // signal end and store the result
            PIMPL_LOCK_BEGIN(m);
            if (SUCCEEDED(PIMPL_(result))) { PIMPL_(result) = hr; }
            PIMPL_(isFilterDone) = true;
            PIMPL_LOCK_END;
            PIMPL_(cv).notify_all();
        });
    }

    void ItemTask::SetEndOfExtraction(HRESULT hr)
    {
        // signal end of extraction for the task and stream
        PIMPL_LOCK_BEGIN(m);
        if (FAILED(hr)) { PIMPL_(result) = hr; } // extraction errors always override filter failures
        PIMPL_(isExtractionDone) = true;
        PIMPL_LOCK_END;
        PIMPL_(cv).notify_all();
        PIMPL_(writeStream)->SetEndOfFile();
    }

    void ItemTask::Abort()
    {
        // signal abort and wait for the thread to end
        PIMPL_(aborted) = true;
        if (PIMPL_(gatherer).joinable()) { PIMPL_(gatherer).join(); }
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
        if (PIMPL_(gatherer).joinable()) { PIMPL_(gatherer).join(); }
        return std::nullopt;
    }
}
