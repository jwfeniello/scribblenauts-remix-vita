#include "scrib.h"
#include "audio.h"
#include "utils/logger.h"
#include <falso_jni/FalsoJNI_Impl.h>
#include <stdio.h>

static void initialize(jmethodID id, va_list args) { scrib_queue_initialize(); }
static void load_joysticks(jmethodID id, va_list args) { scrib_queue_joysticks(); }
static void boot_complete(jmethodID id, va_list args) { scrib_queue_boot_complete(); }
static void show_keyboard(jmethodID id, va_list args) {
    (void)va_arg(args, int);
    scrib_keyboard_set("");
    scrib_keyboard_show();
}
static void set_keyboard(jmethodID id, va_list args) {
    jstring str = va_arg(args, jstring);
    if (!str) { scrib_keyboard_set(""); return; }
    const char *text = (*jni).GetStringUTFChars(&jni, str, NULL);
    scrib_keyboard_set(text);
    (*jni).ReleaseStringUTFChars(&jni, str, (char *)text);
}
static jobject language(jmethodID id, va_list args) { return (*jni).NewStringUTF(&jni, "en_US"); }
static jobject hide_keyboard(jmethodID id, va_list args) {
    return (*jni).NewStringUTF(&jni, scrib_keyboard_text());
}
static jboolean yes(jmethodID id, va_list args) { return JNI_TRUE; }
static jboolean no(jmethodID id, va_list args) { return JNI_FALSE; }
static void mute(jmethodID id, va_list args) { scrib_audio_mute(va_arg(args, int) != 0); }
static void play_music(jmethodID id, va_list args) {
    int track = va_arg(args, int);
    int looping = va_arg(args, int);
    scrib_audio_music(track, looping != 0);
}
static void play_sfx(jmethodID id, va_list args) { scrib_audio_sfx(va_arg(args, int)); }
static void stop_music(jmethodID id, va_list args) { scrib_audio_stop_music(); }
static void save_sound(jmethodID id, va_list args) { scrib_audio_save_enabled(va_arg(args, int) != 0); }
static void unsupported(jmethodID id, va_list args) {
#ifdef SCRIB_DIAGNOSTICS
    static unsigned char logged[128];
    unsigned n = (unsigned)(uintptr_t)id;
    if (n < sizeof(logged) && !logged[n]) {
        logged[n] = 1;
        for (unsigned i = 0; i < nameToMethodId_size() / sizeof(NameToMethodID); ++i)
            if (nameToMethodId[i].id == n)
                l_warn("JNI service inactive in diagnostic build: %s", nameToMethodId[i].name);
    }
#endif
}

NameToMethodID nameToMethodId[] = {
    {70, "exitAndPauseApplication", METHOD_TYPE_VOID},
    {71, "launchWBPage", METHOD_TYPE_VOID},
    {72, "launchWBPrivacyPolicy", METHOD_TYPE_VOID},
    {73, "promptExitGame", METHOD_TYPE_VOID},
    {74, "resizeLayout", METHOD_TYPE_VOID},
    {75, "tweetImage", METHOD_TYPE_VOID},
    {76, "tweetMessage", METHOD_TYPE_VOID},

    {1, "jni_getDeviceLanguage", METHOD_TYPE_OBJECT},
    {2, "jni_hideKeyboard", METHOD_TYPE_OBJECT},
    {3, "jni_disableOnline", METHOD_TYPE_BOOLEAN},
    {4, "jni_getStoreEnabled", METHOD_TYPE_BOOLEAN},
    {5, "jni_getGameCenterMenuOptionsEnabled", METHOD_TYPE_BOOLEAN},
    {6, "jni_getIsKFTUBuild", METHOD_TYPE_BOOLEAN},
    {7, "jni_WBID_HasPlayerSignedUp", METHOD_TYPE_BOOLEAN},
    {8, "jni_IsNewVersion", METHOD_TYPE_BOOLEAN},
    {10, "jni_initialize", METHOD_TYPE_VOID},
    {11, "jni_loadSavedJoysticksEnabled", METHOD_TYPE_VOID},
    {12, "jni_showKeyboard", METHOD_TYPE_VOID},
    {13, "jni_setKeyboardText", METHOD_TYPE_VOID},
    {14, "jni_promptForFirstBootWithDLC", METHOD_TYPE_VOID},
    {15, "jni_promptForFirstBootWithDeviceCheck", METHOD_TYPE_VOID},
    {16, "jni_promptForFirstBootWithoutDLC", METHOD_TYPE_VOID},
    {17, "jni_promptForTutorialStart", METHOD_TYPE_VOID},
    {18, "jni_mute", METHOD_TYPE_VOID},
    {19, "jni_playMusic", METHOD_TYPE_VOID},
    {20, "jni_playSFX", METHOD_TYPE_VOID},
    {21, "jni_stopMusic", METHOD_TYPE_VOID},
    {22, "jni_saveSoundEnabled", METHOD_TYPE_VOID},
    {23, "jni_saveJoysticksEnabled", METHOD_TYPE_VOID},
    {24, "jni_rateApp", METHOD_TYPE_VOID},
    {25, "jni_promptRateApp", METHOD_TYPE_VOID},
    {26, "jni_promptGoldCrown", METHOD_TYPE_VOID},
    {27, "jni_giftApp", METHOD_TYPE_VOID},
    {28, "jni_sendMessage", METHOD_TYPE_VOID},
    {29, "jni_analyticsLogEvent", METHOD_TYPE_VOID},
    {30, "jni_initFacebook", METHOD_TYPE_VOID},
    {31, "jni_postMessageToFacebookWall", METHOD_TYPE_VOID},
    {32, "jni_postImageToFacebookWall", METHOD_TYPE_VOID},
    {33, "jni_loginFacebook", METHOD_TYPE_VOID},
    {34, "jni_logoutFacebook", METHOD_TYPE_VOID},
    {35, "jni_showProfilePicAt", METHOD_TYPE_VOID},
    {36, "jni_showProfilePic", METHOD_TYPE_VOID},
    {37, "jni_setProfilePicAlpha", METHOD_TYPE_VOID},
    {38, "jni_initGameCenter", METHOD_TYPE_VOID},
    {39, "jni_authenticateLocalPlayer", METHOD_TYPE_VOID},
    {40, "jni_showAchievements", METHOD_TYPE_VOID},
    {41, "jni_showLeaderboards", METHOD_TYPE_VOID},
    {42, "jni_reportAchievementProgress", METHOD_TYPE_VOID},
    {43, "jni_reportLeaderboardScore", METHOD_TYPE_VOID},
    {44, "jni_flushLeaderboardScores", METHOD_TYPE_VOID},
    {45, "jni_resetAchievementProgress", METHOD_TYPE_VOID},
    {46, "jni_playHavenContentRequest", METHOD_TYPE_VOID},
    {47, "jni_purchase", METHOD_TYPE_VOID},
    {48, "jni_open", METHOD_TYPE_VOID},
    {49, "jni_PrintStackInfo", METHOD_TYPE_VOID},
};
MethodsObject methodsObject[] = {{1, language}, {2, hide_keyboard}};
MethodsBoolean methodsBoolean[] = {{3, yes}, {4, no}, {5, no}, {6, no}, {7, no}, {8, no}};
MethodsVoid methodsVoid[] = {
    {70, unsupported}, // exitAndPauseApplication
    {71, unsupported}, // launchWBPage
    {72, unsupported}, // launchWBPrivacyPolicy
    {73, unsupported}, // promptExitGame
    {74, unsupported}, // resizeLayout
    {75, unsupported}, // tweetImage
    {76, unsupported}, // tweetMessage

    {10, initialize}, // jni_initialize
    {11, load_joysticks}, // jni_loadSavedJoysticksEnabled
    {12, show_keyboard}, // jni_showKeyboard
    {13, set_keyboard}, // jni_setKeyboardText
    {14, boot_complete}, // jni_promptForFirstBootWithDLC
    {15, boot_complete}, // jni_promptForFirstBootWithDeviceCheck
    {16, boot_complete}, // jni_promptForFirstBootWithoutDLC
    {17, boot_complete}, // jni_promptForTutorialStart
    {18, mute}, // jni_mute
    {19, play_music}, // jni_playMusic
    {20, play_sfx}, // jni_playSFX
    {21, stop_music}, // jni_stopMusic
    {22, save_sound}, // jni_saveSoundEnabled
    {23, unsupported}, // jni_saveJoysticksEnabled
    {24, unsupported}, // jni_rateApp
    {25, unsupported}, // jni_promptRateApp
    {26, unsupported}, // jni_promptGoldCrown
    {27, unsupported}, // jni_giftApp
    {28, unsupported}, // jni_sendMessage
    {29, unsupported}, // jni_analyticsLogEvent
    {30, unsupported}, // jni_initFacebook
    {31, unsupported}, // jni_postMessageToFacebookWall
    {32, unsupported}, // jni_postImageToFacebookWall
    {33, unsupported}, // jni_loginFacebook
    {34, unsupported}, // jni_logoutFacebook
    {35, unsupported}, // jni_showProfilePicAt
    {36, unsupported}, // jni_showProfilePic
    {37, unsupported}, // jni_setProfilePicAlpha
    {38, unsupported}, // jni_initGameCenter
    {39, unsupported}, // jni_authenticateLocalPlayer
    {40, unsupported}, // jni_showAchievements
    {41, unsupported}, // jni_showLeaderboards
    {42, unsupported}, // jni_reportAchievementProgress
    {43, unsupported}, // jni_reportLeaderboardScore
    {44, unsupported}, // jni_flushLeaderboardScores
    {45, unsupported}, // jni_resetAchievementProgress
    {46, unsupported}, // jni_playHavenContentRequest
    {47, unsupported}, // jni_purchase
    {48, unsupported}, // jni_open
    {49, unsupported}, // jni_PrintStackInfo
};
MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};
MethodsFloat methodsFloat[] = {};
MethodsInt methodsInt[] = {};
MethodsLong methodsLong[] = {};
MethodsShort methodsShort[] = {};
NameToFieldID nameToFieldId[] = {};
FieldsBoolean fieldsBoolean[] = {};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = {};
FieldsInt fieldsInt[] = {};
FieldsObject fieldsObject[] = {};
FieldsLong fieldsLong[] = {};
FieldsShort fieldsShort[] = {};
__FALSOJNI_IMPL_CONTAINER_SIZES
