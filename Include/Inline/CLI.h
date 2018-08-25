#pragma once

#include "IR/Module.h"
#include "IR/Validate.h"
#include "Inline/BasicTypes.h"
#include "Inline/Floats.h"
#include "Inline/Timing.h"
#include "Platform/Platform.h"
#include "Runtime/Runtime.h"
#include "WASM/WASM.h"
#include "WAST/WAST.h"

#include <vector>

inline bool loadFile(const char* filename, std::vector<U8>& outFileContents)
{
	Platform::File* file = Platform::openFile(
		filename, Platform::FileAccessMode::readOnly, Platform::FileCreateMode::openExisting);
	if(!file)
	{
		Log::printf(Log::error, "Couldn't read %s: couldn't open file.\n");
		return false;
	}

	U64 numFileBytes = 0;
	errorUnless(Platform::seekFile(file, 0, Platform::FileSeekOrigin::end, &numFileBytes));
	if(numFileBytes > UINTPTR_MAX)
	{
		Log::printf(Log::error, "Couldn't read %s: file is too large.\n");
		errorUnless(Platform::closeFile(file));
		return false;
	}

	std::vector<U8> fileContents;
	outFileContents.resize(numFileBytes);
	errorUnless(Platform::seekFile(file, 0, Platform::FileSeekOrigin::begin));
	errorUnless(Platform::readFile(file, const_cast<U8*>(outFileContents.data()), numFileBytes));
	errorUnless(Platform::closeFile(file));

	return true;
}

inline bool saveFile(const char* filename, const void* fileBytes, Uptr numFileBytes)
{
	Platform::File* file = Platform::openFile(
		filename, Platform::FileAccessMode::writeOnly, Platform::FileCreateMode::createAlways);
	if(!file)
	{
		Log::printf(Log::error, "Couldn't write %s: couldn't open file.\n");
		return false;
	}

	errorUnless(Platform::writeFile(file, fileBytes, numFileBytes));
	errorUnless(Platform::closeFile(file));

	return true;
}

inline void reportParseErrors(const char* filename, const std::vector<WAST::Error>& parseErrors)
{
	// Print any parse errors.
	Log::printf(Log::error, "Error parsing WebAssembly text file:\n");
	for(auto& error : parseErrors)
	{
		Log::printf(Log::error,
					"%s:%s: %s\n%s\n%*s\n",
					filename,
					error.locus.describe().c_str(),
					error.message.c_str(),
					error.locus.sourceLine.c_str(),
					error.locus.column(8),
					"^");
	}
}

inline bool loadTextModuleFromFile(const char* filename, IR::Module& outModule)
{
	std::vector<U8> wastBytes;
	if(!loadFile(filename, wastBytes)) { return false; }

	// Make sure the WAST is null terminated.
	wastBytes.push_back(0);

	std::vector<WAST::Error> parseErrors;
	if(WAST::parseModule((const char*)wastBytes.data(), wastBytes.size(), outModule, parseErrors))
	{ return true; }
	else
	{
		reportParseErrors(filename, parseErrors);
		return false;
	}
}

inline bool loadBinaryModule(const void* wasmBytes,
							 Uptr numBytes,
							 IR::Module& outModule,
							 Log::Category errorCategory = Log::error)
{
	// Load the module from a binary WebAssembly file.
	try
	{
		Timing::Timer loadTimer;

		Serialization::MemoryInputStream stream((const U8*)wasmBytes, numBytes);
		WASM::serialize(stream, outModule);

		Timing::logRatePerSecond("Loaded WASM", loadTimer, numBytes / 1024.0 / 1024.0, "MB");
		return true;
	}
	catch(Serialization::FatalSerializationException exception)
	{
		Log::printf(errorCategory,
					"Error deserializing WebAssembly binary file:\n%s\n",
					exception.message.c_str());
		return false;
	}
	catch(IR::ValidationException exception)
	{
		Log::printf(errorCategory,
					"Error validating WebAssembly binary file:\n%s\n",
					exception.message.c_str());
		return false;
	}
	catch(std::bad_alloc)
	{
		Log::printf(errorCategory, "Memory allocation failed: input is likely malformed\n");
		return false;
	}
}

inline bool loadBinaryModuleFromFile(const char* filename,
									 IR::Module& outModule,
									 Log::Category errorCategory = Log::error)
{
	std::vector<U8> wasmBytes;
	if(!loadFile(filename, wasmBytes)) { return false; }
	return loadBinaryModule(wasmBytes.data(), wasmBytes.size(), outModule);
}
