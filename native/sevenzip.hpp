/*
 * iFilter4Archives
 * Copyright (C) 2019  Manuel Meitinger
 *
 * 7-Zip
 * Copyright (C) 1999-2019  Igor Pavlov
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

#include <Windows.h>
#include <comip.h>
#include <comdef.h>

namespace sevenzip
{
	enum class AskMode : INT32
	{
		Extract = 0,
		Test,
		Skip
	};

	enum class OperationResult : INT32
	{
		OK = 0,
		UnsupportedMethod,
		DataError,
		CRCError,
		Unavailable,
		UnexpectedEnd,
		DataAfterEnd,
		IsNotArc,
		HeadersError,
		WrongPassword
	};

	enum class PropertyId : PROPID
	{
		NoProperty = 0,
		MainSubfile,
		HandlerItemIndex,
		Path,
		Name,
		Extension,
		IsDir,
		Size,
		PackSize,
		Attrib,
		CTime,
		ATime,
		MTime,
		Solid,
		Commented,
		Encrypted,
		SplitBefore,
		SplitAfter,
		DictionarySize,
		CRC,
		Type,
		IsAnti,
		Method,
		HostOS,
		FileSystem,
		User,
		Group,
		Block,
		Comment,
		Position,
		Prefix,
		NumSubDirs,
		NumSubFiles,
		UnpackVer,
		Volume,
		IsVolume,
		Offset,
		Links,
		NumBlocks,
		NumVolumes,
		TimeType,
		Bit64,
		BigEndian,
		Cpu,
		PhySize,
		HeadersSize,
		Checksum,
		Characts,
		Va,
		Id,
		ShortName,
		CreatorApp,
		SectorSize,
		PosixAttrib,
		SymLink,
		Error,
		TotalSize,
		FreeSpace,
		ClusterSize,
		VolumeName,
		LocalName,
		Provider,
		NtSecure,
		IsAltStream,
		IsAux,
		IsDeleted,
		IsTree,
		Sha1,
		Sha256,
		ErrorType,
		NumErrors,
		ErrorFlags,
		WarningFlags,
		Warning,
		NumStreams,
		NumAltStreams,
		AltStreamsSize,
		VirtualSize,
		UnpackSize,
		TotalPhySize,
		VolumeIndex,
		SubType,
		ShortComment,
		CodePage,
		IsNotArcType,
		PhySizeCantBeDetected,
		ZerosTailIsAllowed,
		TailSize,
		EmbeddedStubSize,
		NtReparse,
		HardLink,
		INode,
		StreamId,
		ReadOnly,
		OutName,
		CopyLink
	};

	enum class HandlerPropertyId : PROPID
	{
		Name = 0,        // VT_BSTR
		ClassID,         // binary GUID in VT_BSTR
		Extension,       // VT_BSTR
		AddExtension,    // VT_BSTR
		Update,          // VT_BOOL
		KeepName,        // VT_BOOL
		Signature,       // binary in VT_BSTR
		MultiSignature,  // binary in VT_BSTR
		SignatureOffset, // VT_UI4
		AltStreams,      // VT_BOOL
		NtSecure,        // VT_BOOL
		Flags            // VT_UI4
	};

	enum class ErrorFlags : UINT32
	{
		IsNotArc = 1 << 0,
		HeadersError = 1 << 1,
		EncryptedHeadersError = 1 << 2,
		UnavailableStart = 1 << 3,
		UnconfirmedStart = 1 << 4,
		UnexpectedEnd = 1 << 5,
		DataAfterEnd = 1 << 6,
		UnsupportedMethod = 1 << 7,
		UnsupportedFeature = 1 << 8,
		DataError = 1 << 9,
		CrcError = 1 << 10
	};

	/******************************************************************************/

#define SEVENZIP_GUID(guid) #guid
#define SEVENZIP_INTERFACE(yy,xx,name,base,...) \
	struct DECLSPEC_UUID(SEVENZIP_GUID(23170F69-40C1-278A-0000-00 ## yy ## 00 ## xx ## 0000)) DECLSPEC_NOVTABLE name : public base { __VA_ARGS__ }; \
	_COM_SMARTPTR_TYPEDEF(name, __uuidof(name))

	SEVENZIP_INTERFACE(00, 05, IProgress, IUnknown,
public:
	STDMETHOD(SetTotal)(UINT64 total) PURE;
	STDMETHOD(SetCompleted)(const UINT64* completeValue) PURE;
	);

	SEVENZIP_INTERFACE(03, 01, ISequentialInStream, IUnknown,
public:
	STDMETHOD(Read)(void* data, UINT32 size, UINT32* processedSize) PURE;
	);

	SEVENZIP_INTERFACE(03, 02, ISequentialOutStream, IUnknown,
public:
	STDMETHOD(Write)(const void* data, UINT32 size, UINT32* processedSize) PURE;
	);

	SEVENZIP_INTERFACE(03, 03, IInStream, ISequentialInStream,
public:
	STDMETHOD(Seek)(INT64 offset, UINT32 seekOrigin, UINT64* newPosition) PURE;
	);

	SEVENZIP_INTERFACE(03, 06, IStreamGetSize, IUnknown,
public:
	STDMETHOD(GetSize)(UINT64* size) PURE;
	);

	SEVENZIP_INTERFACE(06, 10, IArchiveOpenCallback, IUnknown,
public:
	STDMETHOD(SetTotal)(const UINT64* files, const UINT64* bytes) PURE;
	STDMETHOD(SetCompleted)(const UINT64* files, const UINT64* bytes) PURE;
	);

	SEVENZIP_INTERFACE(06, 20, IArchiveExtractCallback, IProgress,
public:
	STDMETHOD(GetStream)(UINT32 index, ISequentialOutStream** outStream, AskMode askExtractMode) PURE;
	STDMETHOD(PrepareOperation)(AskMode askExtractMode) PURE;
	STDMETHOD(SetOperationResult)(OperationResult opRes) PURE;
	);

	SEVENZIP_INTERFACE(06, 60, IInArchive, IUnknown,
public:
	STDMETHOD(Open)(IInStream* stream, const UINT64* maxCheckStartPosition, IArchiveOpenCallback* openCallback) PURE;
	STDMETHOD(Close)() PURE;
	STDMETHOD(GetNumberOfItems)(UINT32* numItems) PURE;
	STDMETHOD(GetProperty)(UINT32 index, PropertyId propID, PROPVARIANT* value) PURE;
	STDMETHOD(Extract)(const UINT32* indices, UINT32 numItems, INT32 testMode, IArchiveExtractCallback* extractCallback) PURE;
	STDMETHOD(GetArchiveProperty)(PropertyId propID, PROPVARIANT* value) PURE;
	STDMETHOD(GetNumberOfProperties)(UINT32* numProps) PURE;
	STDMETHOD(GetPropertyInfo)(UINT32 index, BSTR* name, PROPID* propID, VARTYPE* varType) PURE;
	STDMETHOD(GetNumberOfArchiveProperties)(UINT32* numProps) PURE;
	STDMETHOD(GetArchivePropertyInfo)(UINT32 index, BSTR* name, PROPID* propID, VARTYPE* varType) PURE;
	);

#undef SEVENZIP_INTERFACE
#undef SEVENZIP_GUID

	/******************************************************************************/

	extern "C"
	{
		typedef HRESULT(WINAPI* Func_CreateObject)(const GUID* clsID, const GUID* iid, void** outObject);
		typedef HRESULT(WINAPI* Func_GetNumberOfFormats)(UINT32* numFormats);
		typedef HRESULT(WINAPI* Func_GetHandlerProperty2)(UINT32 index, HandlerPropertyId propID, PROPVARIANT* value);
	}
}
