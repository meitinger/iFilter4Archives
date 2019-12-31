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

#include "object.hpp"

#include "ClassFactory.hpp"
#include "Registrar.hpp"

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) noexcept
{
    UNREFERENCED_PARAMETER(lpvReserved);
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        ::DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    return com::ClassFactory::GetClassObject(rclsid, riid, ppv);
}

STDAPI DllCanUnloadNow()
{
    return com::object::CanUnloadNow();
}

STDAPI DllRegisterServer()
{
    return com::Registrar::RegisterServer();
}

STDAPI DllUnregisterServer()
{
    return com::Registrar::UnregisterServer();
}
