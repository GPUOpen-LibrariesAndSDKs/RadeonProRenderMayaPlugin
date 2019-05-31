/*********************************************************************************************************************************
* Radeon ProRender for plugins
* Copyright (c) 2019 AMD
* All Rights Reserved
*
* Project Athena
*********************************************************************************************************************************/

#include "RprAthena.h"

#include <string>
#include <vector>
#include <fstream>

#ifdef WIN32
#include <Windows.h>
#endif

#include <json.hpp>

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/ratelimiter/DefaultRateLimiter.h>
#include <aws/core/utils/UUID.h>
#include <aws/core/utils/StringUtils.h>

//
//	Radeon ProRender Athena data capture component
//
//	Below are the AWS details :
//		Developer guide : https://docs.aws.amazon.com/sdk-for-cpp/v1/developer-guide/setup.html
//		Git link : https://github.com/aws/aws-sdk-cpp
//		Modules used : aws-cpp-sdk-core and aws-cpp-sdk-s3
//
//	AWS Access details :
//		Access key : "AKIAJDKGBNLE6Z6QBVPA"
//		Secret key : " / 3pIiluDh0yRYkiJsmkvm + Ktlprqp4CQSi41nzwj"
//		Bucket Name : "amd - athena - prorender"
//		Code snippet : "//depot/stg/ppc/apps/ppc/Source/AMDPPC/AMDPPCMaster/AMDPPCDU.cpp::UploadS3()"
//
//	Software developer helping me:
//		ManoharReddy Pallela <ManoharReddy.Pallela@amd.com>
//
//	To verify your upload is working:
//		After upload please send details to Jagadeeshyadav.Paga@amd.com. He will confirm about the uploads.
//
//	There is no specific format.  It is based on client need.  Filename should be unique across 
//	the globe.  If file name is duplicate it can be overwritten at AWS.
//	We take the following approach for file naming.
//		Format: GUID_STARTTIME_ENDTIME_CLIENTVERSION
//			GUID			: it is created at the time of installation of client.  it should be retained on 
//								upgrading / uninstallation as well.By this way, we can identify how many unique 
//								clients are using out application.
//			STARTTIME		: collection starting time
//			ENDTIME			: collection ending time(mostly it is upload time).For next collection file this could be STARTTIME.
//			CLIENTVERSION	: product version of client(optional)
//

namespace
{
	std::vector<std::wstring> GetFiles(const std::wstring& folder, const std::wstring& extension)
	{
		std::vector<std::wstring> existingFiles;
#ifdef WIN32
		WIN32_FIND_DATAW findData;
		std::wstring globPath = folder + L"\\*." + extension;
		HANDLE hFind = ::FindFirstFileW(globPath.c_str(), &findData);
		if (hFind != INVALID_HANDLE_VALUE) 
		{
			do
			{
				if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					existingFiles.push_back(std::wstring((PathStringType*)findData.cFileName));
				}
			} while (::FindNextFileW(hFind, &findData));
			::FindClose(hFind);
		}
		else
		{
			std::wcerr << "# GetFiles failed  " << globPath.c_str() << std::endl;
		}
#else
		// Use opendir() on Linux and macOS
		assert(false);
#endif
		return existingFiles;
	}

	bool SplitWString(std::wstring& sourceString, WCHAR delim, std::vector<std::wstring>& splitString)
	{

		if (!sourceString.empty())
		{
			PathStringType* nextToken;
			PathStringType* token = wcstok_s(&sourceString[0], &delim, &nextToken);
			while (token) {

				splitString.push_back(std::wstring(token));
				token = wcstok_s(nullptr, &delim, &nextToken);
			}
		}
		else
		{
			return false;
		}
		return true;
	}

	double GetTimeDiffMilliseconds(SYSTEMTIME &recentTime, SYSTEMTIME &previousTime)
	{
		FILETIME v_ftime;
		ULARGE_INTEGER v_ui;
		__int64 v_right, v_left, v_res;
		SystemTimeToFileTime(&recentTime, &v_ftime);
		v_ui.LowPart = v_ftime.dwLowDateTime;
		v_ui.HighPart = v_ftime.dwHighDateTime;
		v_right = v_ui.QuadPart;

		SystemTimeToFileTime(&previousTime, &v_ftime);
		v_ui.LowPart = v_ftime.dwLowDateTime;
		v_ui.HighPart = v_ftime.dwHighDateTime;
		v_left = v_ui.QuadPart;

		v_res = v_right - v_left;
		// 60000000000 ns in a minute
		// 600000000 100-ns ticks in a minute
		// double totalValue = v_res / (double)1000000.0;
		double totalValue = v_res / (double)10000.0;
		return totalValue;

	}

	long long CurrentTimeMilliseconds(SYSTEMTIME currentTime)
	{
		SYSTEMTIME epochTime;
		epochTime.wYear = 1970;
		epochTime.wMonth = 1;
		epochTime.wDay = 1;
		epochTime.wHour = 0;
		epochTime.wMinute = 0;
		epochTime.wSecond = 0;
		epochTime.wMilliseconds = 0;
		long long returnValue = static_cast<long long>(GetTimeDiffMilliseconds(currentTime, epochTime));
		return returnValue;
	}

	_int64 CurrentTimeMilliseconds()
	{
#ifdef WIN32
		SYSTEMTIME time = {};
		GetSystemTime(&time);
		return (time.wSecond * 1000) + time.wMilliseconds;
#else
		assert(false);
		return 0;
#endif
	}

	__int64 JHash(std::wstring& key, int len)
	{
		unsigned int hash = 0;
		for (int i = 0; i < len; ++i)
		{
			hash += key[i];
			hash += (hash << 10);
			hash ^= (hash >> 6);
		}
		hash += (hash << 3);
		hash ^= (hash >> 11);
		hash += (hash << 15);
		return hash;
	}

	std::wstring GetHashFromString(const std::wstring& sourceString)
	{
		std::string convertTester(sourceString.begin(), sourceString.end());
		void* data = static_cast<void*>(const_cast<char*>(convertTester.c_str()));
		size_t data_size = convertTester.size();
		HCRYPTPROV hProv = NULL;
		std::wstring returnValue = L"";
		if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
			return returnValue;
		}

		BOOL hash_ok = FALSE;
		HCRYPTPROV hHash = NULL;
		hash_ok = CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash);

		if (!hash_ok)
		{
			CryptReleaseContext(hProv, 0);
			return returnValue;
		}
		if (!CryptHashData(hHash, static_cast<const BYTE *>(data), data_size, 0)) {
			CryptDestroyHash(hHash);
			CryptReleaseContext(hProv, 0);
			return returnValue;
		}

		DWORD cbHashSize = 0, dwCount = sizeof(DWORD);
		if (!CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE *)&cbHashSize, &dwCount, 0)) {
			CryptDestroyHash(hHash);
			CryptReleaseContext(hProv, 0);
			return returnValue;
		}

		std::vector<BYTE> buffer(cbHashSize);
		if (!CryptGetHashParam(hHash, HP_HASHVAL, reinterpret_cast<BYTE*>(&buffer[0]), &cbHashSize, 0)) {
			CryptDestroyHash(hHash);
			CryptReleaseContext(hProv, 0);
			return returnValue;
		}

		std::ostringstream oss;

		for (std::vector<BYTE>::const_iterator iter = buffer.begin(); iter != buffer.end(); ++iter) {
			oss.fill('0');
			oss.width(2);
			oss << std::hex << static_cast<const int>(*iter);
		}

		CryptDestroyHash(hHash);
		CryptReleaseContext(hProv, 0);
		std::string finalString = oss.str();
		std::wstring finalWString(finalString.begin(), finalString.end());
		return finalWString;
	}

	__int64 SystemGetFileSize(const std::wstring& fileName)
	{
		WIN32_FILE_ATTRIBUTE_DATA fad;
		if (!GetFileAttributesEx((LPCSTR)fileName.c_str(), GetFileExInfoStandard, &fad))
		{
			return -1;
		}
		LARGE_INTEGER size;
		size.HighPart = fad.nFileSizeHigh;
		size.LowPart = fad.nFileSizeLow;
		return size.QuadPart;
	}

	std::wstring KS1(const std::wstring& sk1, const std::wstring& fileName, const std::wstring& uploadTimestamp, const std::wstring& PREPEND_META)
	{
		std::wstring buffer = fileName + sk1 + uploadTimestamp;
		__int64 dynamic_number = JHash(buffer, buffer.length());
		return PREPEND_META + std::to_wstring(dynamic_number);
	}

	std::wstring KS2(const std::wstring& sk2, __int64 fileSize, const std::wstring& fileName, __int64 uploadTimestampMilliseconds)
	{
		__int64 numberValue = fileSize + uploadTimestampMilliseconds;
		std::wstring buffer = fileName + std::to_wstring(numberValue) + sk2;
		std::wstring modBuffer = GetHashFromString(buffer);
		return modBuffer;
	}

	static std::vector<std::string> gNames;
	static std::string gEmpty;

	std::string& GetJsonFieldName(unsigned index)
	{
		if (gNames.empty())
		{
			gNames.push_back("cpuname");		// TODO change to real name once we get list from Brian
			gNames.push_back("gpuname");		// TODO change to real name once we get list from Brian
		}
		// TODO : Must get list from Brian Savery
		if (index < gNames.size())
			return gNames[index];
		return gEmpty;
	}

#ifdef WIN32
	std::wstring kPATH_SEPARATOR = L"\\";
#else
	std::wstring kPATH_SEPARATOR = L"/";
#endif
}

//
// The impls that follow must be kept outside of the anonymous namespace
// as they are forward declared in the header file
//

struct AthenaOptionsImpl
{
	AthenaOptionsImpl()
		: mAccessKey(L"AKIAJDKGBNLE6Z6QBVPA")
		, mSecretKey(L"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")
		, mBucketName(L"amd-athena-prorender")
		, mALLOCATION_TAG("S3UploadData")
		, mProtocol(L"https")
		, mIsOpen(false)
		, mRateLimit(200000.0f)
	{
		memset(&mOptions, 0, sizeof(mOptions));
	}

	// String settings
	const wchar_t* mAccessKey;
	const wchar_t* mSecretKey;
	const wchar_t* mBucketName;
	const char* mALLOCATION_TAG;
	const wchar_t *mProtocol;

	// AWS
	Aws::SDKOptions mOptions;
	float mRateLimit;	// TODO: double check setting

	bool mIsOpen;
};

struct AthenaFileImpl
{
	nlohmann::json mJson;
};

//
// Function implementations follow
//

AthenaOptionsPtr athenaCreateOptions()
{
	AthenaOptionsPtr pOptions = new AthenaOptions;
	pOptions->pImpl = new AthenaOptionsImpl;
	return pOptions;
}

void athenaDestroyOptions(AthenaOptionsPtr pOptions)
{
	if (!pOptions)
	{
		return;
	}
	if (pOptions->pImpl)
	{
		delete pOptions->pImpl;
		pOptions->pImpl = NULL;
	}
	delete pOptions;
}

AthenaStatus athenaInit(AthenaOptionsPtr pOptions)
{
	if (!pOptions)
	{
		return kInvalidParam;
	}
	Aws::InitAPI(pOptions->pImpl->mOptions);

	pOptions->pImpl->mIsOpen = true;
	return kSuccess;
}

AthenaStatus athenaShutdown(AthenaOptionsPtr pOptions)
{
	if (!pOptions)
	{
		return kInvalidParam;
	}
	Aws::ShutdownAPI(pOptions->pImpl->mOptions);
	pOptions->pImpl->mIsOpen = false;

	return kSuccess;
}

AthenaStatus athenaUpload(AthenaOptionsPtr pOptions, const PathStringType* sendFile, PathStringType* fileExtension)
{
	if (!pOptions || !sendFile || !fileExtension || !pOptions->pImpl->mIsOpen)
	{
		return kInvalidParam;
	}

	std::wstring ext(L"json");
	if (ext != fileExtension)
	{
		return kInvalidParam;
	}

	AthenaStatus successFlag = kSuccess;

	auto limiter = Aws::MakeShared<Aws::Utils::RateLimits::DefaultRateLimiter<>>(pOptions->pImpl->mALLOCATION_TAG, pOptions->pImpl->mRateLimit);

	Aws::Client::ClientConfiguration config;
	config.scheme = Aws::Http::Scheme::HTTP;
	config.connectTimeoutMs = 30000;
	config.requestTimeoutMs = 30000;
	// Use same limiter for read/write
	config.readRateLimiter = limiter;
	config.writeRateLimiter = limiter;

	std::wstring localProtocol = pOptions->pImpl->mProtocol;
	config.scheme = Aws::Http::Scheme::HTTP;
	if (localProtocol == L"https")
	{
		config.scheme = Aws::Http::Scheme::HTTPS;
	}

	Aws::S3::S3Client s3_client;

	s3_client = 
		Aws::S3::S3Client(
			Aws::Auth::AWSCredentials(Aws::Utils::StringUtils::FromWString(pOptions->pImpl->mAccessKey),
			Aws::Utils::StringUtils::FromWString(pOptions->pImpl->mSecretKey)), 
			config);

	std::wstring fileUpload = sendFile;
	std::wstring currentFile(fileUpload);
	currentFile.resize(currentFile.length() - (ext.length() + 1));

	//std::wcerr << "### File to upload : " << fileUpload.c_str() << std::endl;

	Aws::S3::Model::PutObjectRequest object_request;
	object_request.WithBucket(
		Aws::Utils::StringUtils::FromWString(pOptions->pImpl->mBucketName)).WithKey(Aws::Utils::StringUtils::FromWString(currentFile.c_str())).WithStorageClass(Aws::S3::Model::StorageClass::STANDARD);

	std::shared_ptr<Aws::FStream> fileToUpload = Aws::MakeShared<Aws::FStream>(pOptions->pImpl->mALLOCATION_TAG, fileUpload, std::ios_base::in | std::ios_base::binary);

	object_request.SetBody(fileToUpload);
	auto put_object_outcome = s3_client.PutObject(object_request);

	if (put_object_outcome.IsSuccess())
	{
		//std::cerr << "### Upload successful\n";
		fileToUpload->close();
		DeleteFile((LPCSTR)fileUpload.c_str());
	}
	else
	{
		put_object_outcome.GetError().GetExceptionName();
		put_object_outcome.GetError().GetMessage();
		std::cerr << "### Upload failed : " << put_object_outcome.GetError().GetExceptionName().c_str() <<
			" : message : " << put_object_outcome.GetError().GetMessage().c_str() << std::endl;
	}

	return successFlag;
}

char *athenaGuidStr(AthenaOptionsPtr pOptions)
{
	if (!pOptions)
	{
		return NULL;
	}
	//
	Aws::Utils::UUID uuid = Aws::Utils::UUID::RandomUUID();
	Aws::String guidstr = uuid;

	char* guid = strdup(guidstr.c_str());

	return guid;
}

AthenaFilePtr athenaFileOpen()
{
	AthenaFilePtr p = new AthenaFile;
	p->pImpl = new AthenaFileImpl;
	return p;
}

AthenaStatus athenaFileClose(AthenaFilePtr pJson)
{
	if (!pJson)
	{
		return kInvalidParam;
	}
	//
	if (pJson->pImpl)
	{
		delete pJson->pImpl;
		pJson->pImpl = NULL;
	}
	delete pJson;

	return kSuccess;
}

unsigned athenaGetFieldCount()
{
	return (unsigned) gNames.size();
}

const char* athenaFileGetFieldName(unsigned index)
{
	std::string& str = GetJsonFieldName(index);
	return str.c_str();
}

AthenaStatus athenaFileSetField(AthenaFilePtr pJson, const char* fieldName, const char* value)
{
	if (!pJson || !fieldName|| !value)
	{
		return kInvalidParam;
	}
	//
	pJson->pImpl->mJson[fieldName] = value;
	//
	return kSuccess;
}

const PathStringType* athenaUniqueFilename(const char* guidstr)
{
	if (!guidstr)
	{
		return NULL;
	}
	std::wstring uniquename = Aws::Utils::StringUtils::ToWString(guidstr).c_str();
	uniquename += L"_";

	SYSTEMTIME systme;
	GetSystemTime(&systme);

	std::wostringstream stream;
	//1111111_2019 02 22 21 27 18 0342
	stream << std::setfill(L'0') << std::setw(4) << systme.wYear;
	stream << std::setfill(L'0') << std::setw(2) << systme.wMonth;
	stream << std::setfill(L'0') << std::setw(2) << systme.wDay;
	stream << std::setfill(L'0') << std::setw(2) << systme.wHour;
	stream << std::setfill(L'0') << std::setw(2) << systme.wMinute;
	stream << std::setfill(L'0') << std::setw(2) << systme.wSecond;
	stream << std::setfill(L'0') << std::setw(4) << systme.wMilliseconds;

	uniquename += stream.str();
	uniquename += L".json";

	return wcsdup(uniquename.c_str());
}

AthenaStatus athenaFileWrite(AthenaFilePtr pJson, const PathStringType* filePath)
{
	if (!pJson || !filePath)
	{
		return kInvalidParam;
	}
	std::ofstream o(filePath);
	o << std::setw(4) << pJson->pImpl->mJson << std::endl;
	o.close();
	return kSuccess;
}

//
// Testing code
//

class Tester
{
public:
	Tester()
	{
		std::cerr << "# athenaUniqueFilename : " << athenaUniqueFilename("1111111") << std::endl;
		std::cerr << "# athenaGetFieldCount : " << athenaGetFieldCount() << std::endl;
		std::cerr << "# athenaFileGetFieldName : " << athenaFileGetFieldName(0) << std::endl;
		std::cerr << "# athenaFileGetFieldName : " << athenaFileGetFieldName(1) << std::endl;
		std::cerr << "# athenaFileGetFieldName : " << athenaFileGetFieldName(2) << std::endl;
		// json tests
		{
			AthenaFilePtr jptr = athenaFileOpen();
			std::cerr << "# athenaFileOpen : " << jptr << std::endl;
			std::cerr << "# athenaFileSetField : " << athenaFileSetField(jptr, athenaFileGetFieldName(0), "abc") << std::endl;
			std::cerr << "# athenaFileSetField : " << athenaFileSetField(jptr, athenaFileGetFieldName(1),"xyz") << std::endl;
			std::wstring fpath = L"c:\\tmp\\";
			fpath += athenaUniqueFilename("1112111");
			std::cerr << "# athenaFileWrite : " << athenaFileWrite(jptr, fpath.c_str()) << std::endl;
			std::cerr << "# athenaFileClose : " << athenaFileClose(jptr) << std::endl;
		}
		// aws test for init/shutdown
		{
			AthenaOptionsPtr aopt = athenaCreateOptions();
			std::cerr << "# athenaCreateOptions : " << aopt << std::endl;
			std::cerr << "# athenaInit : " << athenaInit(aopt) << std::endl;
			Sleep(2);
			std::cerr << "# athenaShutdown : " << athenaShutdown(aopt) << std::endl;
			athenaDestroyOptions(aopt);
		}
		// aws test to create and upload a file
		{
			AthenaOptionsPtr aopt = athenaCreateOptions();
			std::cerr << "# athenaCreateOptions : " << aopt << std::endl;
			std::cerr << "# athenaInit : " << athenaInit(aopt) << std::endl;
			char *guidstr = athenaGuidStr(aopt);
			std::wstring fpath = L"c:\\tmp\\awsupload\\";
			fpath += athenaUniqueFilename(guidstr);
			{
				AthenaFilePtr jptr = athenaFileOpen();
				std::cerr << "# athenaFileOpen : " << jptr << std::endl;
				std::cerr << "# athenaFileSetField : " << athenaFileSetField(jptr, athenaFileGetFieldName(0), "0101ytr") << std::endl;
				std::cerr << "# athenaFileSetField : " << athenaFileSetField(jptr, athenaFileGetFieldName(1), "0F0Fbvc") << std::endl;
				std::cerr << "# athenaFileWrite : " << athenaFileWrite(jptr, fpath.c_str()) << std::endl;
				std::cerr << "# athenaFileClose : " << athenaFileClose(jptr) << std::endl;
			}
			const std::wstring wfpath = L"C:\\tmp\\awsupload";
			std::vector<std::wstring> flist = GetFiles(wfpath, L"*");
			for (auto &f : flist)
			{
				std::wcerr << "# Found file : " << f.c_str() << std::endl;
			}
			std::cerr << "# athenaUpload : " << athenaUpload(aopt, wfpath.c_str(), L"json") << std::endl;
			std::cerr << "# athenaShutdown : " << athenaShutdown(aopt) << std::endl;
			athenaDestroyOptions(aopt);
			free(guidstr);
		}
		//
		std::cerr << std::endl;
	}
};

void athenaSelfTest()
{
	Tester tester;
}
