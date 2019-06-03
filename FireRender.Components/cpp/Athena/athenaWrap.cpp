#include "athenaWrap.h"
#include <codecvt>
#include <string>
#include <experimental/filesystem>
#include <chrono>
#include <thread>

std::wstring s2ws(const std::string& str);
std::string ws2s(const std::wstring& wstr);

AthenaWrapper* AthenaWrapper::GetAthenaWrapper(void)
{
	static AthenaWrapper instance;
	return &instance;
}

AthenaWrapper::AthenaWrapper()
	: m_athenaOptions(nullptr)
	, m_athenaFile(nullptr)
	, m_isEnabled(true)
	, m_folderPath()
	, sendFileAsync()
{
	m_athenaOptions = athenaCreateOptions();

	AthenaStatus aStatus = athenaInit(m_athenaOptions);
	CHECK_ASTATUS(aStatus);

	StartNewFile();

	std::string temp(getenv("TEMP"));
	m_folderPath = s2ws(temp);
	m_folderPath += L"\\rprathena";
}

AthenaWrapper::~AthenaWrapper()
{
	AthenaStatus aStatus = athenaShutdown(m_athenaOptions);

	athenaDestroyOptions(m_athenaOptions);
}

void AthenaWrapper::Finalize()
{
	sendFileAsync.clear();
}

void AthenaWrapper::StartNewFile(void)
{
	if (m_athenaFile != nullptr)
		return; // TODO : make this check more pretty

	// Begin new file
	m_athenaFile = athenaFileOpen();
}

void AthenaWrapper::SetEnabled(bool enable /*= true*/)
{
	m_isEnabled = enable;
}

void AthenaWrapper::SetTempFolder(const std::wstring& folderPath)
{
	m_folderPath = folderPath;
}

bool AthenaWrapper::AthenaSendFile(void)
{
	// athena disabled by ui => return
	if (!m_isEnabled)
		return true;

	// back-off if not inited
	if (!m_athenaOptions)
		return false;

	// generate file uid
	char* pAthenaUID = athenaGuidStr(m_athenaOptions);
	std::string athenaUID (pAthenaUID);
	free (pAthenaUID);
	const PathStringType* pUniqueFileName = athenaUniqueFilename(athenaUID.c_str());
	const std::wstring uniqueFileName (pUniqueFileName);
	free ((PathStringType*)pUniqueFileName);

	// create folder
	bool folderCreated = std::experimental::filesystem::create_directory(m_folderPath);

	// create and write file
	AthenaStatus aStatus;
	std::wstring fullPath = m_folderPath + L"\\" + uniqueFileName;;
	aStatus = athenaFileWrite(m_athenaFile, fullPath.c_str());
	if (aStatus != AthenaStatus::kSuccess)
	{
		return false;
	}

	aStatus = athenaFileClose(m_athenaFile);
	m_athenaFile = nullptr;
	if (aStatus != AthenaStatus::kSuccess)
	{
		return false;
	}

	// upload file
	// - need to copy filepath when passing it to keep string data in other thread
	auto handle = std::async(std::launch::async, [] (AthenaOptionsPtr pOptions, std::wstring fullpath)->bool 
	{
		AthenaStatus aStatus = athenaUpload(pOptions, fullpath.c_str(), L"json");
		if (aStatus != AthenaStatus::kSuccess)
		{
			return false;
		}

		// clear temp file
		int res = std::remove(ws2s(fullpath).c_str());
		if (res != 0)
			return false;

		return true;
	}, 
	m_athenaOptions, fullPath);

	// move future so that routine could be executed in background
	sendFileAsync.push_back(std::move(handle));

	return true;
}

bool AthenaWrapper::WriteField(const std::string& fieldName, const std::string& value)
{
	// ensure valid input
	if ((fieldName.length() == 0) || (value.length() == 0))
	{
		return false;
	}

	if (m_athenaFile == nullptr)
		return false; // file not opened

	// proceed writing
	AthenaStatus aStatus;
	aStatus = athenaFileSetField(m_athenaFile, fieldName.c_str(), value.c_str());
	CHECK_ASTATUS(aStatus);
	if (aStatus != AthenaStatus::kSuccess)
	{
		return false;
	}

	// success!
	return true;
}

AthenaOptionsPtr AthenaWrapper::GetAthenaOptions(void)
{
	return m_athenaOptions;
}

AthenaFilePtr AthenaWrapper::GetAthenaFile(void)
{
	return m_athenaFile;
}

std::wstring s2ws(const std::string& str)
{
	using convert_typeX = std::codecvt_utf8<wchar_t>;

	std::wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.from_bytes(str);
}

std::string ws2s(const std::wstring& wstr)
{
	using convert_typeX = std::codecvt_utf8<wchar_t>;

	std::wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.to_bytes(wstr);
}

