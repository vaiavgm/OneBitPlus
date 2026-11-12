#include "IPlugPaths.h"
BEGIN_IPLUG_NAMESPACE
void AppSupportPath(WDL_String& path, bool isSystem) { path.Set(isSystem ? "/tmp/obp_headless_shared" : "/tmp/obp_headless_user"); }
END_IPLUG_NAMESPACE
