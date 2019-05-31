/*********************************************************************************************************************************
* Radeon ProRender for plugins
* Copyright (c) 2019 AMD
* All Rights Reserved
*
* Project Athena
*********************************************************************************************************************************/

#pragma once


#ifdef __cplusplus
extern "C" {
#endif

	//
	// Struct storage and types
	//

	struct AthenaOptionsImpl;
	struct AthenaOptions
	{
		AthenaOptions() : pImpl(0) {}
		//
		AthenaOptionsImpl* pImpl;
	};

	typedef AthenaOptions* AthenaOptionsPtr;

	struct AthenaFileImpl;
	struct AthenaFile
	{
		AthenaFile() : pImpl(0) {}
		//
		AthenaFileImpl* pImpl;
	};

	typedef AthenaFile* AthenaFilePtr;

	typedef wchar_t PathStringType;

	//
	// Athena support:
	//	- Pair create options and destroy options
	//	- Pair initialized and shutdown functions and
	//	these are called after options are created
	//	- Create json files using athenFile* functions
	//	- Create a unique json file name using the
	//	guid string function that is provided
	//

	typedef enum 
	{
		kInvalidParam = -2,
		kInvalidPath = -1,
		kFailure = 0,
		kSuccess = 1,
	} AthenaStatus;

	// Options creation and destruction.  Calls should be paired together
	AthenaOptionsPtr athenaCreateOptions();
	void athenaDestroyOptions(AthenaOptionsPtr pOptions);

	// Initialize and shutdown.  Calls should be paired together
	AthenaStatus athenaInit(AthenaOptionsPtr pOptions);
	AthenaStatus athenaShutdown(AthenaOptionsPtr pOptions);

	// Upload the collected data.  Athena should be initialized first
	AthenaStatus athenaUpload(AthenaOptionsPtr pOptions, const PathStringType* sendFile, PathStringType* fileExtension);

	// Get a guid string.  Caller must release memory.
	char *athenaGuidStr(AthenaOptionsPtr pOptions);

	//
	// Json file support
	//

	AthenaFilePtr athenaFileOpen();
	AthenaStatus athenaFileClose(AthenaFilePtr pJson);

	unsigned athenaGetFieldCount();
	const char* athenaFileGetFieldName(unsigned index);
	AthenaStatus athenaFileSetField(AthenaFilePtr pJson, const char* fieldName, const char* value);

	const PathStringType* athenaUniqueFilename(const char* guidstr = 0);
	AthenaStatus athenaFileWrite(AthenaFilePtr pJson, const PathStringType* filePath);

	//
	// Self test call
	//
	void athenaSelfTest();

#ifdef __cplusplus
}
#endif
