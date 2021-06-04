// strings.h
// Copyright 2016 Robin.Rowe@CinePaint.org
// License open source MIT

#ifndef strings_h
#define strings_h

#include "../portable/stub.h"

#ifdef __cplusplus
extern "C" {
#else
#define inline __inline
#endif

int ffs(int i)
{   STUB_NEG(ffs);
}

int ffsl(long int i)
{   STUB_NEG(ffsl);
}

int ffsll(long long int i)
{   STUB_NEG(ffsll);
}

#ifdef __cplusplus
}
#endif

#endif
