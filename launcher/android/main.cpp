/*
Copyright (C) 2022 nillerusr
*/

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <jni.h>
#include <stdlib.h>
#include <unistd.h>
#include <SDL_hints.h>
#include <SDL_system.h>
#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include "SourceApp/sourceapp_userstats.h"
#include "SourceApp/sourceapp_android.h"

char *LauncherArgv[512];
char java_args[4096];
int iLastArgs = 0;

extern void InitCrashHandler();

DLL_EXPORT int LauncherMain( int argc, char **argv );

// --------------------------------------------------------------------------------------------
// JNI: 环境变量设置
// --------------------------------------------------------------------------------------------
DLL_EXPORT int Java_zzh_source_launcher_data_jni_GameBridge_setenv(
    JNIEnv *jenv, jclass *jclass, jstring env, jstring value, jint over)
{
    const char *szEnv = jenv->GetStringUTFChars(env, NULL);
    const char *szVal = jenv->GetStringUTFChars(value, NULL);
    Msg("[SourceApp]: Java_zzh_source_launcher_data_jni_GameBridge_setenv %s=%s\n", szEnv, szVal);
    int ret = setenv(szEnv, szVal, over);
    jenv->ReleaseStringUTFChars(env, szEnv);
    jenv->ReleaseStringUTFChars(value, szVal);
    return ret;
}

DLL_EXPORT void Java_zzh_source_launcher_data_jni_GameBridge_nativeOnActivityResult()
{
}

DLL_EXPORT void Java_zzh_source_launcher_data_jni_GameBridge_setArgs(JNIEnv *env, jclass *clazz, jstring str)
{
    strncpy(java_args, env->GetStringUTFChars(str, NULL), sizeof(java_args) - 1);
    java_args[sizeof(java_args) - 1] = '\0';
}

// --------------------------------------------------------------------------------------------
// purpose: JNI SourceApp系统初始化（转发到封装层）
// --------------------------------------------------------------------------------------------
DLL_EXPORT void Java_zzh_source_launcher_data_jni_GameBridge_nativeInitAchievementBridge(JNIEnv* env, jclass clazz)
{
    SourceApp_InitAchievementBridge(env, clazz);
}

DLL_EXPORT void Java_zzh_source_launcher_data_jni_GameBridge_nativeTestAchievement(JNIEnv *jenv, jclass *jclass, jstring name)
{
    Msg("[SourceApp]: JNI Test Bridge Working! Achievement: %s\n", jenv->GetStringUTFChars(name, NULL));
}

// --------------------------------------------------------------------------------------------
// purpose: 启动参数构造
// --------------------------------------------------------------------------------------------
void SetLauncherArgs()
{
#define D(a) LauncherArgv[iLastArgs++] = (char*)a

    static char binPath[2048];
    snprintf(binPath, sizeof(binPath), "%s/hl2_linux", getenv("APP_DATA_PATH"));
    D(binPath);
    D("-nouserclip");

    char *pch = strtok(java_args, " ");
    while (pch != NULL) {
        LauncherArgv[iLastArgs++] = pch;
        pch = strtok(NULL, " ");
    }

<<<<<<< HEAD
	char *pch;

	pch = strtok (java_args," ");
	while (pch != NULL)
	{
		LauncherArgv[iLastArgs++] = pch;
		pch = strtok (NULL, " ");
	}

	D("-fullscreen");
	D("-nosteam");
	D("-insecure");

#undef A
=======
    D("-fullscreen");
    D("-nosteam");
    D("-insecure");
>>>>>>> 6acc3351 (feat(all-SourceApp-Jni): 添加类似SteamContext兼容接口（SourceApp）)
#undef D
}

// --------------------------------------------------------------------------------------------
// purpose: 工具函数
// --------------------------------------------------------------------------------------------
float GetTotalMemory()
{
    int64_t mem = 0;
    char meminfo[8196] = {0};
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return 0.f;

    size_t size = fread(meminfo, 1, sizeof(meminfo), f);
    fclose(f);
    if (!size) return 0.f;

    char *s = strstr(meminfo, "MemTotal:");
    if (!s) return 0.f;

    sscanf(s + 9, "%lld", &mem);
    return mem / 1024 / 1024.f;
}

void android_property_print(const char *name)
{
    char prop[1024];
    char strValue[64] = {0};
    snprintf(prop, sizeof(prop), "getprop %s", name);
    FILE *fp = popen(prop, "r");
    if (!fp) return;

    fgets(strValue, sizeof(strValue), fp);
    pclose(fp);

    Msg("[SourceApp]: prop %s=%s", name, strValue);
}

// --------------------------------------------------------------------------------------------
// purpose: 程序入口
// --------------------------------------------------------------------------------------------
DLL_EXPORT int LauncherMainAndroid(int argc, char **argv)
{
    // 初始化平台上下文（成就、统计等）
    SourceAppApicontext = SourceApp_CreateAndroidContext();

    InitCrashHandler();

<<<<<<< HEAD
	android_property_print("ro.build.version.sdk");
	android_property_print("ro.product.device");
	android_property_print("ro.product.manufacturer");
	android_property_print("ro.product.model");
	android_property_print("ro.product.name");
=======
    Msg("[SourceApp]: GetTotalMemory() = %.2f \n", GetTotalMemory());
>>>>>>> 6acc3351 (feat(all-SourceApp-Jni): 添加类似SteamContext兼容接口（SourceApp）)

    android_property_print("ro.build.version.sdk");
    android_property_print("ro.product.device");
    android_property_print("ro.product.manufacturer");
    android_property_print("ro.product.model");
    android_property_print("ro.product.name");
    android_property_print("Inltlized Args");

    SetLauncherArgs();

    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    DeclareCurrentThreadIsMainThread();

    return LauncherMain(iLastArgs, LauncherArgv);
}