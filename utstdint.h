/* Define our own local stdint that works correctly most of the time
   for our target platforms (Win32 or better) since we're in limbo
   with old compilers not including the `stdint.h' header.  */
#ifndef UTSTDINT_H
#define UTSTDINT_H

typedef signed char UTint8;
typedef unsigned char UTuint8;
typedef short UTint16;
typedef unsigned short UTuint16;
typedef int UTint32;
typedef unsigned int UTuint32;
typedef long long UTint64;
typedef unsigned long long UTuint64;

#endif /* not STDINT_H */
