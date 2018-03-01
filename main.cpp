#include "main.h"

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

  jvmtiEventCallbacks callbacks = jvmtiEventCallbacks();
  callbacks.VMInit = &callbackVMInit;
  callbacks.VMDeath = &callbackVMDeath;
  callbacks.VMObjectAlloc = &callbackVMObjectAlloc;
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
  // TODO: Store reference to thread
}

static void JNICALL callbackOnThreadStart(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread) {
  // TODO: Remove thread reference
}


static void JNICALL
callbackVMObjectAlloc(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread, jobject object, jclass object_klass,
                      jlong size) {

  // Getting the name of the class loaded: https://stackoverflow.com/a/12730789
  // First get the class object
  jmethodID mid = jni_env->GetMethodID(object_klass, "getClass", "()Ljava/lang/Class;");
  jobject clsObj = jni_env->CallObjectMethod(object, mid);

// Now get the class object's class descriptor
  object_klass = jni_env->GetObjectClass(clsObj);

// Find the getName() method on the class object
  mid = jni_env->GetMethodID(object_klass, "getName", "()Ljava/lang/String;");

// Call the getName() to get a jstring object back
  auto strObj = (jstring) jni_env->CallObjectMethod(clsObj, mid);

// Now get the c string from the java jstring object
  const char *str = jni_env->GetStringUTFChars(strObj, nullptr);

  std::cout << "VM Object allocated callback! Loaded class: " << str << std::endl;
  jni_env->ReleaseStringUTFChars(strObj, str);
}

static void JNICALL callbackVMDeath(jvmtiEnv *jvmti_env, JNIEnv *jni_env) {
    /* The VM has died. */
    std::cout << "VM Death callback!" << std::endl;
}

static void JNICALL callbackVMInit(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread) {
  std::cout << "VM Init callback!" << std::endl;
  ASGCTType asgct = gdata->ascgt;

  // Call asgct once for testing purposes.
  // TODO: Extract max frames as constant.
  JVMPI_CallFrame frames[2048];
  JVMPI_CallTrace trace;
  trace.frames = frames;

  if (jni_env != nullptr) {
    trace.env_id = jni_env;

    std::cout << "Calling AsyncGetCallTrace" << std::endl;
    (*asgct)(&trace, 2048, nullptr);
    std::cout << "Num frames received: " << trace.num_frames << std::endl;
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