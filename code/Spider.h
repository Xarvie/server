#ifndef MAIN_SPIDER_H
#define MAIN_SPIDER_H

#include "SystemReader.h"
#include "Buffer.h"
#include "NetStruct.h"

#if defined(SELECT_SERVER)

#include "SelectPoller.h"

#elif defined(OS_LINUX)

#include "EpollPoller.h"

#elif defined(OS_DARWIN)

#include "KqueuePoller.h"

#elif defined(OS_WINDOWS)

#include "IOCPPoller.h"

#endif

#endif //MAIN_SPIDER_H
