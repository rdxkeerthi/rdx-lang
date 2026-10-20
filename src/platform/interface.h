#ifndef RDX_PLATFORM_INTERFACE_H
#define RDX_PLATFORM_INTERFACE_H

#ifdef _WIN32
  #include "windows/impl.h"
#else
  #include "linux/impl.h"
#endif

#endif
