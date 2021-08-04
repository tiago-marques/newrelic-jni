#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/*
 * A standalone example that demonstrates to users how to
 * configure logging, configure an app, connect an app,
 * start a transaction and a segment, and cleanly destroy
 * everything.
 */
#include "libnewrelic.h"
#include "newrelicjni.h"


JNIEXPORT jobject JNICALL Java_com_dlocal_NewRelicJNI_init(JNIEnv* env,
                                                        jobject obj,
                                                        jstring appName,
                                                        jstring accountId) {

  newrelic_app_t* app;

  jboolean iscopy;
  const char* appN;
  const char* accountN;
  appN = (*env)->GetStringUTFChars(env, appName, &iscopy);
  accountN = (*env)->GetStringUTFChars(env, accountId, &iscopy);
  printf("App %s\n", appN);
  printf("Account %s\n", accountN);


  newrelic_app_config_t* config;

  config = newrelic_create_app_config(appN, accountN);

  if (!newrelic_configure_log("./c_sdk.log", NEWRELIC_LOG_INFO)) {
    printf("Error configuring logging.\n");
    return NULL;
  }

  if (!newrelic_init(NULL, 0)) {
    printf("Error connecting to daemon.\n");
    return NULL;
  }

  /* Wait up to 10 seconds for the SDK to connect to the daemon (Why 10 sec????) */
  app = newrelic_create_app(config, 10000);
  newrelic_destroy_app_config(&config);

  return (*env)->AllocObject(env, app);
}


JNIEXPORT jobject JNICALL Java_com_dlocal_NewRelicJNI_startWebTransaction(JNIEnv* env,
                                                        jobject obj,
                                                        jstring transactionName,
                                                        jobject appJava) {
  
  newrelic_txn_t* txn;
  newrelic_segment_t* seg;
  jboolean iscopy;
  const char* appN;
  const char* transactionN;

  newrelic_app_t* app = (newrelic_app_t*) (*env)->GetObjectRefType(env, appJava);
  transactionN = (*env)->GetStringUTFChars(env, transactionName, &iscopy);
  printf("Transaction Name %s\n", transactionN);
  txn = newrelic_start_web_transaction(app, transactionN);

  // Needs converts
  return (*env)->AllocObject(env, txn);
  
}

JNIEXPORT jobject JNICALL Java_com_dlocal_NewRelicJNI_startSegment(JNIEnv* env,
                                                        jobject obj,
                                                        jobject txnJava,
                                                        jstring segmentName,
                                                        jstring segmentCustom) {
  
  newrelic_segment_t* seg;
  jboolean iscopy;
  const char* segmentN;
  const char* segmentC;
  segmentN = (*env)->GetStringUTFChars(env, segmentName, &iscopy);
  segmentC = (*env)->GetStringUTFChars(env, segmentCustom, &iscopy);
  
  //Convert (*env)->AllocObject(env, objClass);
  newrelic_txn_t* txn = (newrelic_txn_t*) (*env)->AllocObject(env, txnJava);

  printf("Segment Name %s\n", segmentN);
  printf("Segment Custom %s\n", segmentC);
  seg = newrelic_start_segment(txn, segmentN, segmentC);

  // Needs converts
  return (*env)->AllocObject(env, seg);
  
}

JNIEXPORT void JNICALL Java_com_dlocal_NewRelicJNI_endWebTransaction(JNIEnv* env,
                                                        jobject obj,
                                                        jobject txnJava,
                                                        jobject segJava) {

  //Convert
  newrelic_txn_t* txn = (newrelic_txn_t*) (*env)->AllocObject(env, txnJava);

  //Convert
  newrelic_segment_t* seg = (newrelic_segment_t*) (*env)->AllocObject(env, segJava);

  newrelic_end_segment(txn, &seg);
  newrelic_end_transaction(&txn);

}

JNIEXPORT void JNICALL Java_com_dlocal_NewRelicJNI_destroy(JNIEnv* env,
                                                        jobject obj,
                                                        jobject appJava){
  newrelic_app_t* app = (newrelic_app_t*) (*env)->AllocObject(env, appJava);
  newrelic_destroy_app(&app);
}