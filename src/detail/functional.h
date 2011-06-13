#ifndef _IV_DETAIL_FUNCTIONAL_H_
#define _IV_DETAIL_FUNCTIONAL_H_
#include "platform.h"

#if defined(IV_COMPILER_MSVC) || defined(__GXX_EXPERIMENTAL_CXX0X__)
#include <functional>

#if defined(IV_COMPILER_MSVC) && !defined(IV_COMPILER_MSVC_10)
namespace std { using namespace tr1; }
#endif

#else
#include <tr1/functional>
namespace std { using namespace tr1; }
#endif

#if defined(IV_COMPILER_MSVC_10) || defined(__GXX_EXPERIMENTAL_CXX0X__)
#define IV_HASH_NAMESPACE_START std
#define IV_HASH_NAMESPACE_END
#else
#define IV_HASH_NAMESPACE_START std { namespace tr1
#define IV_HASH_NAMESPACE_END }
#endif

#endif  // _IV_DETAIL_FUNCTIONAL_H_