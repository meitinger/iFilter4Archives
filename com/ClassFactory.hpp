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

namespace com
{
    class ClassFactory; // simple COM class factory that creates com::Filter instances

    /******************************************************************************/

    COM_CLASS_DECLARATION(ClassFactory, com::object IMPLEMENTS(IClassFactory), COM_VISIBLE(IClassFactory)
public:
    ClassFactory();

    STDMETHOD(CreateInstance)(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) noexcept override;
    STDMETHOD(LockServer)(BOOL fLock) noexcept override;

    static HRESULT GetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) noexcept;
    );
}
