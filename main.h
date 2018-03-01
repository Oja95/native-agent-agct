#ifndef NATIVE_AGENT_AGCT_MAIN_OWN_H
#define NATIVE_AGENT_AGCT_MAIN_OWN_H

#include "jvmti.h"
#include "stacktraces.h"
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

static GlobalAgentData *gdata;

jint init(JavaVM *jvm, char *options);

void check_jvmti_error(jvmtiEnv *jvmti, jvmtiError errnum, const std::string &error_message);

static void JNICALL callbackVMInit(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread);

static void JNICALL callbackVMDeath(jvmtiEnv *jvmti_env, JNIEnv *jni_env);

static void JNICALL callbackVMObjectAlloc(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread, jobject object, jclass object_klass,
                           jlong size);


#endif //NATIVE_AGENT_AGCT_MAIN_OWN_H
