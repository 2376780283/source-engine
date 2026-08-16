#ifndef SOURCEAPP_ANDROID_H
#define SOURCEAPP_ANDROID_H

#include <jni.h>
#include "SourceApp/sourceapp_userstats.h"

void SourceApp_InitAchievementBridge(JNIEnv* env, jclass gameBridgeClass);

// Global interface
CSourceAppApicontext* SourceApp_CreateAndroidContext();

#endif
