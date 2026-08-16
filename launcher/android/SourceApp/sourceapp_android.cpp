#include "sourceapp_android.h"
#include <SDL_system.h>
#include "tier0/dbg.h"

static jclass g_AchievementClass = nullptr;
static jmethodID g_OnAchievementUnlocked = nullptr;

// --------------------------------------------------------------------------------------------
// purpose: 成就统计实现：通过 JNI 回调到 Java 通知系统
// --------------------------------------------------------------------------------------------
class CAndroidUserStats : public ISourceAppUserStats {
public:
    void SetAchievement(const char* name) override;
};

void CAndroidUserStats::SetAchievement(const char* name) {
    if (!g_AchievementClass || !g_OnAchievementUnlocked) {
        Msg("[SourceApp]: bridge not initialized yet\n");
        return;
    }

    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (!env) {
        Msg("[SourceApp]: getJniEnv failed\n");
        return;
    }

    JavaVM* javaVM = nullptr;
    env->GetJavaVM(&javaVM);

    JNIEnv* threadEnv = nullptr;
    bool needDetach = false;
    if (javaVM->GetEnv((void**)&threadEnv, JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (javaVM->AttachCurrentThread(&threadEnv, nullptr) != JNI_OK) {
            Msg("[SourceApp]: AttachCurrentThread failed\n");
            return;
        }
        needDetach = true;
    }

    jstring jname = threadEnv->NewStringUTF(name);
    threadEnv->CallStaticVoidMethod(g_AchievementClass, g_OnAchievementUnlocked, jname);
    threadEnv->DeleteLocalRef(jname);

    if (needDetach) {
        javaVM->DetachCurrentThread();
    }
#ifdef DEBUG
    Msg("[SourceApp]: CAndroidUserStats::SetAchievement called for: %s\n", name);
#endif
}

// --------------------------------------------------------------------------------------------
// purpose: Android 平台 API 上下文
// --------------------------------------------------------------------------------------------
class CAndroidSourceAppApicontext : public CSourceAppApicontext {
public:
    ISourceAppUserStats* SourceAppUserStats() override { return &m_UserStats; }
private:
    CAndroidUserStats m_UserStats;
};

CSourceAppApicontext* SourceApp_CreateAndroidContext() {
    return new CAndroidSourceAppApicontext();
}

// --------------------------------------------------------------------------------------------
// JNI 入口：由 GameBridge.initNatives() 在主线程调用
// --------------------------------------------------------------------------------------------
void SourceApp_InitAchievementBridge(JNIEnv* env, jclass gameBridgeClass) {
    g_AchievementClass = (jclass)env->NewGlobalRef(gameBridgeClass);
    g_OnAchievementUnlocked = env->GetStaticMethodID(
        g_AchievementClass,
        "onAchievementUnlocked",
        "(Ljava/lang/String;)V"
    );
    Msg("Achievement JNI bridge initialized, method=%p\n", g_OnAchievementUnlocked);
}
