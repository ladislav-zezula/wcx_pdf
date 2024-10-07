#define NO_DUMMY_DECL
#define NO_GZIP
#pragma warning(disable: 4090)          // '=' : different 'const' qualifiers
#pragma warning(disable: 4127)          // conditional expression is constant
#pragma warning(disable: 4131)          // 'adler32' : uses old-style declarator
#pragma warning(disable: 4242)          // '=' : conversion from 'unsigned int' to 'Bytef', possible loss of data
#pragma warning(disable: 4244)          // '=' : conversion from 'unsigned int' to 'Bytef', possible loss of data

#include "./libs/zlib/adler32.c"
#undef DO1
#undef DO8
#include "./libs/zlib/trees.c"
#include "./libs/zlib/zutil.c"

#undef COPY                             // Conflicting definition
#include "./libs/zlib/inflate.c"
#include "./libs/zlib/inffast.c"
#include "./libs/zlib/inftrees.c"
