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

#include "Module.hpp"

#include "win32.hpp"

namespace archive
{
    CLASS_IMPLEMENTATION(Module,
public:
    std::filesystem::path Path;
    win32::unique_library_ptr modulePtr;
    sevenzip::Func_CreateObject createObjectFunc;
    sevenzip::Func_GetNumberOfFormats getNumberOfFormatsFunc;
    sevenzip::Func_GetHandlerProperty2 getFormatPropertyFunc;
    );

    Module::Module(const std::filesystem::path& path) : PIMPL_INIT()
    {
        PIMPL_(Path) = path;
        PIMPL_(modulePtr) = utils::load_module(path);
        WIN32_DO_OR_THROW(PIMPL_(createObjectFunc) = reinterpret_cast<sevenzip::Func_CreateObject>(::GetProcAddress(PIMPL_(modulePtr).get(), "CreateObject")));
        WIN32_DO_OR_THROW(PIMPL_(getNumberOfFormatsFunc) = reinterpret_cast<sevenzip::Func_GetNumberOfFormats>(::GetProcAddress(PIMPL_(modulePtr).get(), "GetNumberOfFormats")));
        WIN32_DO_OR_THROW(PIMPL_(getFormatPropertyFunc) = reinterpret_cast<sevenzip::Func_GetHandlerProperty2>(::GetProcAddress(PIMPL_(modulePtr).get(), "GetHandlerProperty2")));
    }

    PIMPL_GETTER(Module, const std::filesystem::path&, Path);

    HRESULT Module::CreateObject(REFCLSID rclsid, REFIID riid, void*& ppv) const noexcept
    {
        return PIMPL_(createObjectFunc)(&rclsid, &riid, &ppv);
    }

    HRESULT Module::GetNumberOfFormats(UINT32& count) const noexcept
    {
        return PIMPL_(getNumberOfFormatsFunc)(&count);
    }

    HRESULT Module::GetFormatProperty(UINT32 index, sevenzip::HandlerPropertyId propId, PROPVARIANT& value) const noexcept
    {
        return PIMPL_(getFormatPropertyFunc)(index, propId, &value);
    }
}
