#include "main.h"

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
  std::cout << "Starting to load agent!" << std::endl;
  return init(jvm, options);
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm) {
  delete (gdata);
}

//static void enter_critical_section(jvmtiEnv *jvmti) {
//  jvmtiError error;
//  error = jvmti->RawMonitorEnter(gdata->lock);
//  check_jvmti_error(jvmti, error, "Cannot enter with raw monitor");
//}
//
///* Exit a critical section by doing a JVMTI Raw Monitor Exit */
//static void exit_critical_section(jvmtiEnv *jvmti) {
//  jvmtiError error;
//  error = jvmti->RawMonitorExit(gdata->lock);
//  check_jvmti_error(jvmti, error, "Cannot exit with raw monitor");
//}


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

  jvmtiEventCallbacks callbacks = jvmtiEventCallbacks();
  callbacks.VMInit = &callbackVMInit; /* JVMTI_EVENT_VM_INIT */
  callbacks.VMDeath = &callbackVMDeath; /* JVMTI_EVENT_VM_DEATH */
  callbacks.VMObjectAlloc = &callbackVMObjectAlloc;/* JVMTI_EVENT_VM_OBJECT_ALLOC */
  error = jvmti->SetEventCallbacks(&callbacks, (jint) sizeof(callbacks));
  check_jvmti_error(jvmti, error, "Cannot set JVMTI callbacks");


  /* Here we create a raw monitor for our use in this agent to protect critical sections of code.
  */
  // TODO: Raw monitor creation fails with segfault for some reason, figure out why.
//  error = jvmti->CreateRawMonitor("agent data", &(gdata->lock));
//  check_jvmti_error(jvmti, error, "Cannot create raw monitor");

  gdata = new GlobalAgentData();
  gdata->jvmti = jvmti;
  gdata->ascgt = Accessors::GetJvmFunction("AsyncGetCallTrace");

  std::cout << "Successfully loaded agent!" << std::endl;
  return JNI_OK;
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
  jstring strObj = (jstring) jni_env->CallObjectMethod(clsObj, mid);

// Now get the c string from the java jstring object
  const char *str = jni_env->GetStringUTFChars(strObj, nullptr);

  std::cout << "VM Object allocated callback! Loaded class: " << str << std::endl;
  jni_env->ReleaseStringUTFChars(strObj, str);
}

static void JNICALL callbackVMDeath(jvmtiEnv *jvmti_env, JNIEnv *jni_env) {
//  enter_critical_section(jvmti_env);
  {
    /* The VM has died. */
    std::cout << "VM Death callback!" << std::endl;

    /* The critical section here is important to hold back the VM death
     *    until all other callbacks have completed.
     */

    /* Since this critical section could be holding up other threads
     *   in other event callbacks, we need to indicate that the VM is
     *   dead so that the other callbacks can short circuit their work.
     *   We don't expect any further events after VmDeath but we do need
     *   to be careful that existing threads might be in our own agent
     *   callback code.
     */

  }
//  exit_critical_section(jvmti_env);

}

static void JNICALL callbackVMInit(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread) {
  std::cout << "VM Init callback!" << std::endl;
//  ASGCTType asgct = Asgct::GetAsgct();
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