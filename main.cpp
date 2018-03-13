#include "main.h"


static sigset_t prof_signal_mask;
static ThreadMap threadMap;

jint init(JavaVM *jvm, char *options) {

  jvmtiEnv *jvmti = nullptr;
  jint result = jvm->GetEnv((void **) &jvmti, JVMTI_VERSION_1_2);
  if (result != JNI_OK) {
    std::cerr << "ERROR: Unable to create jvmtiEnv, GetEnv failed, error= " << result << std::endl;
    return JNI_ERR;
  }

  // TODO: Figure out if I need more capabilities.
  jvmtiCapabilities capa = jvmtiCapabilities();
  capa.can_signal_thread = 1;
  capa.can_get_owned_monitor_info = 1;
  capa.can_generate_method_entry_events = 1;
  capa.can_generate_vm_object_alloc_events = 1;
  capa.can_tag_objects = 1;

  jvmtiError error;
  error = jvmti->AddCapabilities(&capa);
  check_jvmti_error(jvmti, error, "Unable to get necessary JVMTI capabilities.");

  error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, (jthread) NULL);
  check_jvmti_error(jvmti, error, "Cannot set event notification");
  error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, (jthread) NULL);
  check_jvmti_error(jvmti, error, "Cannot set event notification");
  error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_OBJECT_ALLOC, (jthread) NULL);
  check_jvmti_error(jvmti, error, "Cannot set event notification");
  error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_LOAD, (jthread) NULL);
  check_jvmti_error(jvmti, error, "Cannot set event notification");
  error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_START, (jthread) NULL);
  check_jvmti_error(jvmti, error, "Cannot set event notification");
  error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_END, (jthread) NULL);
  check_jvmti_error(jvmti, error, "Cannot set event notification");


  sigemptyset(&prof_signal_mask);
  sigaddset(&prof_signal_mask, SIGALRM);

  jvmtiEventCallbacks callbacks = jvmtiEventCallbacks();
  callbacks.VMInit = &callbackVMInit;
  callbacks.VMDeath = &callbackVMDeath;
  callbacks.ThreadStart = &callbackOnThreadStart;
  callbacks.ThreadEnd = &callbackOnThreadEnd;
  callbacks.ClassLoad = &callbackOnClassLoad;

  error = jvmti->SetEventCallbacks(&callbacks, (jint) sizeof(callbacks));
  check_jvmti_error(jvmti, error, "Cannot set JVMTI callbacks");

  gdata = new GlobalAgentData();
  gdata->jvmti = jvmti;
  gdata->ascgt = Accessors::GetJvmFunction("AsyncGetCallTrace");

  std::cout << "Successfully loaded agent!" << std::endl;
  return JNI_OK;
}


JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
  std::cout << "Starting to load agent!" << std::endl;
  return init(jvm, options);
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm) {
  delete (gdata);
}

static void JNICALL callbackOnClassLoad(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread, jclass klass) {
  // Must be here since the AsyncGetCallTrace documentation says so:
  // CLASS_LOAD events have been enabled since agent startup. The enabled event will cause the jmethodIDs to be allocated at class load time.
}

static void JNICALL callbackOnThreadEnd(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread) {
  pthread_sigmask(SIG_BLOCK, &prof_signal_mask, NULL);
  threadMap.remove(jni_env);

}

static void JNICALL callbackOnThreadStart(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread) {
  jvmtiThreadInfo threadInfo{};
  jvmtiPhase jvmtiPhase;
  jvmtiError phase = jvmti_env->GetPhase(&jvmtiPhase);
  if (jvmtiPhase != JVMTI_PHASE_LIVE) {
    std::cout << "JVMTI phase: " << jvmtiPhase << " Cant get thread info!" << std::endl;
  } else {
    jvmtiError info = jvmti_env->GetThreadInfo(thread, &threadInfo);
    std::cout << "GetThreadInfo error code : " << info  << ". New thread started: " << threadInfo.name << std::endl;
    threadMap.put(jni_env, threadInfo.name);
  }
  pthread_sigmask(SIG_UNBLOCK, &prof_signal_mask, NULL);
}

static void JNICALL callbackVMDeath(jvmtiEnv *jvmti_env, JNIEnv *jni_env) {
  /* The VM has died. */
  std::cout << "VM Death callback!" << std::endl;
}

static void JNICALL callbackVMInit(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread) {
  std::cout << "VM Init callback!" << std::endl;
  int x = 1000;
  while (x--) {
    std::cout << "Called ascgt " << x << std::endl;
    callAsgct(jni_env);
  }

}


void check_jvmti_error(jvmtiEnv *jvmti, jvmtiError errnum, const std::string &error_message) {
  if (errnum != JVMTI_ERROR_NONE) {
    char *error_name = nullptr;
    jvmti->GetErrorName(errnum, &error_name);
    std::cerr << error_name << " " << error_message << std::endl;
    exit(1);
  }
}

JVMPI_CallTrace callAsgct(JNIEnv *jni_env/*, void* ucontext*/) {
  ASGCTType asgct = gdata->ascgt;

  // Call asgct once for testing purposes.
  JVMPI_CallFrame frames[MAX_FRAMES_TO_CAPTURE];
  JVMPI_CallTrace trace{};
  trace.frames = frames;

  if (jni_env != nullptr) {
    trace.env_id = jni_env;

    std::cout << "Calling AsyncGetCallTrace" << std::endl;
    (*asgct)(&trace, MAX_FRAMES_TO_CAPTURE, nullptr);
    std::cout << "Num frames received: " << trace.num_frames << std::endl;
  }
}