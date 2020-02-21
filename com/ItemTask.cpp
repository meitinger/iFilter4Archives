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
#include "WriteStream.hpp"

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>

namespace com
{
    CLASS_IMPLEMENTATION(ItemTask,
                         PIMPL_CONSTRUCTOR(const FileDescription& description) : description(description) {}
public:
    const FileDescription description;
    std::mutex m;
    std::condition_variable cv;
    std::list<CachedChunk> chunks;
    CachedChunk::IdMap idMap;
    std::optional<streams::FileBuffer> buffer;
    std::thread gatherer;
    HRESULT result = S_OK;
    bool isExtractionDone = false;
    bool wasFilterStarted = false;
    bool isFilterDone = false;
    std::atomic<bool> aborted = false;
    );

    ItemTask::ItemTask(const FileDescription& description) : PIMPL_INIT(description)
    {
        PIMPL_(chunks).push_back(CachedChunk::FromFileDescription(description)); // first chunk will be the file name
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
        PIMPL_LOCK_END;

        // everything has been extracted and gathered, check if an error occurred
        if (FAILED(PIMPL_(result)))
        {
            // return an error chunk and clear the error
            auto result = CachedChunk::FromHResult(PIMPL_(result));
            result.Map(id, PIMPL_(idMap));
            PIMPL_(result) = S_OK;
            return result;
        }

        // wait for the thread and return
        if (PIMPL_(gatherer).joinable())
        {
            PIMPL_(gatherer).join();
        }
        return std::nullopt;
    }

    sevenzip::ISequentialOutStreamPtr ItemTask::Run(const FilterAttributes& attributes, const Registrar& registrar, ULONG recursionDepth)
    {
        // preliminary checks on the file type
        if (PIMPL_(description).IsDirectory) { return nullptr; } // only handle files
        if (!PIMPL_(description).SizeIsValid || PIMPL_(description).Size > settings::maximum_file_size()) { return nullptr; } // file size unknown or too large
        const auto clsid = registrar.FindClsid(PIMPL_(description).Extension);
        if (!clsid) { return nullptr; } // no filter available
        if (*clsid == __uuidof(Filter) && recursionDepth >= settings::recursion_depth_limit()) { return nullptr; } // limit recursion

        PIMPL_LOCK_BEGIN(m);
        if (PIMPL_(wasFilterStarted) || PIMPL_(isFilterDone)) { return nullptr; } // Run and/or SetEndOfExtraction already called

        // allocate the buffer and start the gatherer
        PIMPL_(buffer) = streams::FileBuffer(PIMPL_(description));
        PIMPL_(gatherer) = std::thread([attributes, filterClsid = *clsid, recursionDepth, PIMPL_CAPTURE]() -> void
        {
            COM_THREAD_BEGIN(COINIT_MULTITHREADED);

            // initialize the sub filter
            auto filter = IFilterPtr();
            COM_DO_OR_THROW(filter.CreateInstance(filterClsid, nullptr, CLSCTX_INPROC_SERVER));
            auto initializeWithStream = IInitializeWithStreamPtr();
            if (SUCCEEDED(filter->QueryInterface<IInitializeWithStream>(&initializeWithStream)))
            {
                COM_DO_OR_THROW(initializeWithStream->Initialize(streams::ReadStream::CreateComInstance<IStream>(*PIMPL_(buffer)), STGM_READ));
            }
            else
            {
                // no IInitializeWithStream, try IPersistStream
                auto persistStream = IPersistStreamPtr();
                COM_DO_OR_THROW(filter->QueryInterface<IPersistStream>(&persistStream));
                COM_DO_OR_THROW(persistStream->Load(streams::ReadStream::CreateComInstance<IStream>(*PIMPL_(buffer))));
            }
            if (filterClsid == __uuidof(Filter))
            {
                // propagate recursion depth
                auto filter4Archives = IFilter4ArchivesPtr();
                COM_DO_OR_THROW(filter->QueryInterface<IFilter4Archives>(&filter4Archives));
                COM_DO_OR_THROW(filter4Archives->SetRecursionDepth(recursionDepth));
            }
            COM_DO_OR_THROW(attributes.Init(filter));

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

            COM_THREAD_END(PIMPL_(result));

            // signal end
            PIMPL_LOCK_BEGIN(m);
            PIMPL_(isFilterDone) = true;
            PIMPL_LOCK_END;
            PIMPL_(cv).notify_all();
        });

        PIMPL_(wasFilterStarted) = true; // set the start flag
        PIMPL_LOCK_END;

        // return the write stream
        return streams::WriteStream::CreateComInstance<sevenzip::ISequentialOutStream>(*PIMPL_(buffer));
    }

    void ItemTask::SetEndOfExtraction()
    {
        // signal end of extraction for the task and stream
        PIMPL_LOCK_BEGIN(m);
        PIMPL_(isExtractionDone) = true;
        if (!PIMPL_(wasFilterStarted))
        {
            PIMPL_(isFilterDone) = true; // ItemTask::Run was not called (successfully)
        }
        PIMPL_LOCK_END;
        PIMPL_(cv).notify_all();
        if (PIMPL_(buffer))
        {
            PIMPL_(buffer)->SetEndOfFile();
        }
    }
}
