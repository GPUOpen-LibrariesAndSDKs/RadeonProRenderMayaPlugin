//
//  athenaSystemInfo.m
//  Maya2017
//
//  Created by Radeon Buildmaster on 08/11/2019.
//

#import <Foundation/Foundation.h>
#include "athenaSystemInfo_Mac.h"

#if __cplusplus
extern "C" {
#endif

void getOSVersion(char* output, unsigned int size)
{
    NSString* version = [[NSProcessInfo processInfo] operatingSystemVersionString];

    // TODO: check buffer capacity
    strcpy(output, [version cStringUsingEncoding:NSUTF8StringEncoding]);
}

void getOSName(char* output, unsigned int size)
{
    // TODO: check buffer capacity
    strcpy(output, "macOS");

    char const* versionStrings[] =
    {
        "Kodiak", "Puma", "Jaguar", "Panther", "Tiger", "Leopard", "Snow Leopard",
        "Lion", "Mountain Lion", "Mavericks", "Yosemite", "El Capitan", "Sierra",
        "High Sierra", "Mojave", "Catalina",
    };

    NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
    if(version.majorVersion == 10 && version.minorVersion < sizeof(versionStrings) / sizeof(versionStrings[0])) {
        strcat(output, " ");
        strcat(output, versionStrings[version.minorVersion]);
    }
}

void getTimeZone(char* output, unsigned int size)
{
    NSTimeZone* zone = [NSTimeZone systemTimeZone];
    NSString* result = [NSString stringWithFormat:@"%@ %@", zone.name, zone.abbreviation];

    // TODO: check buffer capacity
    strcpy(output, [result cStringUsingEncoding:NSUTF8StringEncoding]);
}

#if __cplusplus
}   // Extern C
#endif
