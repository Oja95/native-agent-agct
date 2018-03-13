#ifndef NATIVE_AGENT_AGCT_MAIN_OWN_H
#define NATIVE_AGENT_AGCT_MAIN_OWN_H

#include "jvmti.h"
#include "stacktraces.h"
#include "thread_map.h"
#include <iostream>
#include <cstring>


typedef struct {
    /* JVMTI Environment */
    jvmtiEnv *jvmti;
    jboolean vm_is_started;
    /* Data access Lock */
    jrawMonitorID lock;
    /* Ascgt function */
    ASGCTType ascgt;
} GlobalAgentData;

static const int MAX_FRAMES_TO_CAPTURE = 2048;
static GlobalAgentData *gdata;

jint init(JavaVM *jvm, char *options);

JVMPI_CallTrace callAsgct(JNIEnv *jni_env/*, void* ucontext*/) ;

void check_jvmti_error(jvmtiEnv *jvmti, jvmtiError errnum, const std::string &error_message);

static void JNICALL callbackVMInit(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread);

static void JNICALL callbackVMDeath(jvmtiEnv *jvmti_env, JNIEnv *jni_env);

static void JNICALL callbackOnClassLoad(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread, jclass klass);

static void JNICALL callbackOnThreadEnd(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread);

static void JNICALL callbackOnThreadStart(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread);

#endif //NATIVE_AGENT_AGCT_MAIN_OWN_H
