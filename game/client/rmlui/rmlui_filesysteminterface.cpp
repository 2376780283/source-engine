// ==================================================================
// Den Urakolouy AKA URAKOLOUY5
// 2025
// 
// Feel free to use it as you want to use.
// Major code based on open source references from Source SDK.
// ==================================================================

// Include pre-include header to fix NULL macro conflicts
#include "rmlui_preinclude.h"

// 强行解除引擎可能存在的宏污染
#ifdef malloc
#undef malloc
#endif
#ifdef free
#undef free
#endif
#ifdef realloc
#undef realloc
#endif

#include "cbase.h"

// Fix NULL macro after basetypes.h has been included - override with 0 for RmlUI compatibility
#ifdef NULL
#undef NULL
#endif
#define NULL 0
#include "filesystem.h"

#include "rmlui_filesysteminterface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

Rml::FileHandle RmlUiFileInterface::Open(const Rml::String& path)
{
	FileHandle_t handle = g_pFullFileSystem->Open(path.c_str(), "r", "MOD");
	return reinterpret_cast<Rml::FileHandle>(handle);
}

void RmlUiFileInterface::Close(Rml::FileHandle file)
{
	FileHandle_t handle = reinterpret_cast<FileHandle_t>(file);

	if (handle)
		filesystem->Close(handle);
}

size_t RmlUiFileInterface::Read(void* buffer, size_t size, Rml::FileHandle file)
{
	FileHandle_t handle = reinterpret_cast<FileHandle_t>(file);

	if (handle)
		return filesystem->Read(buffer, size, handle);
	
	Error("RmlUIFileInterface - Failed to read file using IFileSystem, handle is not FileHandle_t");
	return size_t();
}

bool RmlUiFileInterface::Seek(Rml::FileHandle file, long offset, int origin)
{
	FileHandle_t handle = reinterpret_cast<FileHandle_t>(file);

	if (handle)
	{
		long currentPos = filesystem->Tell(handle);
		if (currentPos == -1)
			return false;

		long targetPos = 0;
		switch (origin)
		{
			case SEEK_SET:
				targetPos = offset;
				break;
			case SEEK_CUR:
				targetPos = currentPos + offset;
				break;
			case SEEK_END:
			{
				long fileSize = filesystem->Size(handle);
				if (fileSize == -1)
					return false;
				targetPos = fileSize + offset;
				break;
			}
			default:
				return false;
		}

		filesystem->Seek(handle, offset, static_cast<FileSystemSeek_t>(origin));

		long newPos = filesystem->Tell(handle);
		return (newPos == targetPos);
	}

	return false;
}


size_t RmlUiFileInterface::Tell(Rml::FileHandle file)
{
	FileHandle_t handle = reinterpret_cast<FileHandle_t>(file);

	if (handle)
		return filesystem->Tell(handle);

	return size_t();
}