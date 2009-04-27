
#ifndef REMOTE_DEBUG_H
#define REMOTE_DEBUG_H

#ifdef CS_DEBUG
#include <stdio.h>
#define DBG(format, args...) printf(format, ## args)

char* optotext(unsigned);

#else
#define DBG(format, args...)
#endif

#endif
