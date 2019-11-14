#ifndef ATHENA_SYSTEM_INFO_MAC_H
#define ATHENA_SYSTEM_INFO_MAC_H

#if __cplusplus
extern "C" {
#endif

void getOSVersion(char* output, unsigned int size);
void getOSName(char* output, unsigned int size);
void getTimeZone(char* output, unsigned int size);

#if __cplusplus
}   // Extern C
#endif

#endif  // ATHENA_SYSTEM_INFO_MAC_H
