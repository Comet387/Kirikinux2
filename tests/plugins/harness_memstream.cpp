// SPDX-License-Identifier: AGPL-3.0-only
// tTVPMemoryStream for the headless plugin harness: the methods below are the
// engine's own implementation (src/core/base/UtilStreams.cpp), compiled here
// without the rest of the storage subsystem.
#include "tjsCommHead.h"
#include "UtilStreams.h"
#include <cstdlib>
#include <cstring>
#define TVPThrowExceptionMessage(x) TJS::TJS_eTJSError(TJS_W("memory stream error"))
tTVPMemoryStream::tTVPMemoryStream()
{
	Init();
}
//---------------------------------------------------------------------------
tTVPMemoryStream::tTVPMemoryStream(const void * block, tjs_uint size)
{
	Init();
	Block = (void*)block;
	if(!Block)
	{
		Block = Alloc(size);
		if(!Block) TVPThrowExceptionMessage(TVPInsufficientMemory);
	}
	else
	{
		Reference = true; // memory block was given
	}
	Size = size;
	AllocSize = size;
	CurrentPos = 0;
}
//---------------------------------------------------------------------------
tTVPMemoryStream::~tTVPMemoryStream()
{
	if(Block && !Reference) Free(Block);
}
//---------------------------------------------------------------------------
tjs_uint64 TJS_INTF_METHOD tTVPMemoryStream::Seek(tjs_int64 offset, tjs_int whence)
{
	tjs_int64 newpos;
	switch(whence)
	{
	case TJS_BS_SEEK_SET:
		if(offset >= 0)
		{
			if(offset <= Size) CurrentPos = static_cast<tjs_uint>(offset);
		}
		return CurrentPos;

	case TJS_BS_SEEK_CUR:
		if((newpos = offset + (tjs_int64)CurrentPos) >= 0)
		{
			tjs_uint np = (tjs_uint)newpos;
			if(np <= Size) CurrentPos = np;
		}
		return CurrentPos;

	case TJS_BS_SEEK_END:
		if((newpos = offset + (tjs_int64)Size) >= 0)
		{
			tjs_uint np = (tjs_uint)newpos;
			if(np <= Size) CurrentPos = np;
		}
		return CurrentPos;
	}
	return CurrentPos;
}
//---------------------------------------------------------------------------
tjs_uint TJS_INTF_METHOD tTVPMemoryStream::Read(void *buffer, tjs_uint read_size)
{
	if(CurrentPos + read_size >= Size)
	{
		read_size = Size - CurrentPos;
	}

	memcpy(buffer, (tjs_uint8*)Block + CurrentPos, read_size);

	CurrentPos += read_size;

	return read_size;
}
//---------------------------------------------------------------------------
tjs_uint TJS_INTF_METHOD tTVPMemoryStream::Write(const void *buffer, tjs_uint write_size)
{
	// writing may increase the internal buffer size.
	if(Reference) TVPThrowExceptionMessage(TVPWriteError);

	tjs_uint newpos = CurrentPos + write_size;
	if(newpos >= AllocSize)
	{
		// exceeds AllocSize
		tjs_uint onesize;
		if(AllocSize < 64*1024) onesize = 4*1024;
		else if(AllocSize < 512*1024) onesize = 16*1024;
		else if(AllocSize < 4096*1024) onesize = 256*1024;
		else onesize = 2024*1024;
		AllocSize += onesize;

		if(CurrentPos + write_size >= AllocSize) // still insufficient ?
		{
			AllocSize = CurrentPos + write_size;
		}

		Block = Realloc(Block, AllocSize);

		if(AllocSize && !Block)
			TVPThrowExceptionMessage(TVPInsufficientMemory);
			// this exception cannot be repaird; a fatal error.
	}

	memcpy((tjs_uint8*)Block + CurrentPos, buffer, write_size);

	CurrentPos = newpos;

	if(CurrentPos > Size) Size = CurrentPos;

	return write_size;
}
//---------------------------------------------------------------------------
void TJS_INTF_METHOD tTVPMemoryStream::SetEndOfStorage()
{
	if(Reference) TVPThrowExceptionMessage(TVPWriteError);

	Size = CurrentPos;
	AllocSize = Size;
	Block = Realloc(Block, Size);
	if(Size && !Block)
		TVPThrowExceptionMessage(TVPInsufficientMemory);
}
//---------------------------------------------------------------------------
void tTVPMemoryStream::Clear(void)
{
	if(Block && !Reference) Free(Block);
	Init();
}
//---------------------------------------------------------------------------
void tTVPMemoryStream::SetSize(tjs_uint size)
{
	if(Reference) TVPThrowExceptionMessage(TVPWriteError);

	if(Size > size)
	{
		// decrease
		Size = size;
		AllocSize = size;
		Block = Realloc(Block, size);
		if(CurrentPos > Size) CurrentPos = Size;
		if(size && !Block)
			TVPThrowExceptionMessage(TVPInsufficientMemory);
	}
	else
	{
		// increase
		AllocSize = size;
		Size = size;
		Block = Realloc(Block, size);
		if(size && !Block)
			TVPThrowExceptionMessage(TVPInsufficientMemory);

	}
}
//---------------------------------------------------------------------------
void tTVPMemoryStream::Init()
{
	Block = NULL;
	Reference = false;
	Size = 0;
	AllocSize = 0;
	CurrentPos = 0;
}
//---------------------------------------------------------------------------
void * tTVPMemoryStream::Alloc(size_t size)
{
	return TJS_malloc(size);
}
//---------------------------------------------------------------------------
void * tTVPMemoryStream::Realloc(void *orgblock, size_t size)
{
	return TJS_realloc(orgblock, size);
}
//---------------------------------------------------------------------------
void tTVPMemoryStream::Free(void *block)
{
	TJS_free(block);
}
