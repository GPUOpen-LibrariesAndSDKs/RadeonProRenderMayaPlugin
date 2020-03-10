/**********************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/

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
