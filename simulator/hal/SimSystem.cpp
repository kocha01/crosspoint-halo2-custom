// Desktop HalSystem stubs
#include <HalSystem.h>

namespace HalSystem {
void begin() {}
void checkPanic() {}
void clearPanic() {}
std::string getPanicInfo(bool) { return ""; }
bool isRebootFromPanic() { return false; }
}
