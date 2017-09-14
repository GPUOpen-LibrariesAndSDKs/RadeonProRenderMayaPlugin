//
//  MyInstallerPane.m
//  RDRRegistrationCheck
//
//  Created by Mark Pursey on 5/10/2016.
//  Copyright © 2016 AMD. All rights reserved.
//

#import "MyInstallerPane.h"

@implementation MyInstallerPane

- (NSString *)title
{
    return [[NSBundle bundleForClass:[self class]] localizedStringForKey:@"PaneTitle" value:nil table:nil];
}

- (void)willEnterPane:(InstallerSectionDirection)dir
{
    [self setNextEnabled: false];
}

- (IBAction)textAction:(NSTextField *)sender
{
    if ([sender.stringValue  isEqual: @"GPUOpen2016"])
        [self setNextEnabled: true];
    else
        [self setNextEnabled: false];
}
- (IBAction)onGetLink:(id)sender {
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://experience.amd.com/plugin-registration?registrationid=5A1E27D27D97ECF5&appname=autodeskmaya&frversion=1.10.11.0&os=osx"]];

}

/*
 std::wstring getRegistationLink(MSIHANDLE hInstall)
 {
	GetSystemInfo();
 
	// get maya version with installed plugin
	std::wstring appversion;
	std::vector<std::wstring> versions = getMayaVersionWithInstalledPlugin(hInstall);
	for (size_t i = 0; i < versions.size(); i++)
	{
 if (i > 0)
 appversion += L";";
 appversion += versions[i];
	}
	
	// get FireRender version
	uint32_t majorVersion = VERSION_MAJOR(FR_API_VERSION);
	uint32_t minorVersion = VERSION_MINOR(FR_API_VERSION);
	TCHAR paramVal[MAX_PATH];
	wsprintf(paramVal, L"%X.%X", majorVersion, minorVersion);
	std::wstring frVersion = paramVal;
 
	std::wstring registrationid = L"5A1E27D27D97ECF5";
	std::wstring appname = L"autodeskmaya";
	std::wstring osVersion = g_systemInfo.osversion;
	std::wstring driverVersion = g_systemInfo.gpuDriver.size() > 0 ? g_systemInfo.gpuDriver[0] : std::wstring(L"");
	std::wstring gfxcard = g_systemInfo.gpuName.size()   > 0 ? g_systemInfo.gpuName[0] : std::wstring(L"");
	
	//link must look like :
	//"http://experience.amd.com/plugin-registration?registrationid=5A1E27D27D97ECF5&appname=autodeskmaya&appversion=2016&frversion=1.6.30&os=win6.1.7601&gfxcard=AMD_FirePro_W8000__FireGL_V_&driverversion=15.201.2401.0"
	
	std::wstring sLink = L"http://experience.amd.com/plugin-registration";
 
	sLink += L"?registrationid=" + URLfirendly(registrationid);
	sLink += L"&appname="		+ URLfirendly(appname);
	sLink += L"&appversion="	+ URLfirendly(appversion);
	sLink += L"&frversion="		+ URLfirendly(frVersion);
	sLink += L"&os="			+ URLfirendly(osVersion);
	sLink += L"&gfxcard="		+ URLfirendly(gfxcard);
	sLink += L"&driverversion=" + URLfirendly(driverVersion);
 
	LogSystem("getRegistationLink return: %s", WstringToString(sLink).c_str());
 
	return sLink;
 }

 */
@end
