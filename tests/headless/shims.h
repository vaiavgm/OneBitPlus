// WDL's endian header expects BSD-style bswap functions on this platform
static inline unsigned short bswap16(unsigned short x) { return __builtin_bswap16(x); }
static inline unsigned int bswap32(unsigned int x) { return __builtin_bswap32(x); }
static inline unsigned long long bswap64(unsigned long long x) { return __builtin_bswap64(x); }
// IGraphics headers want platform types and a non-standard std::fmodf
#include <cmath>
namespace std { using ::fmodf; }
