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

newrelic_app_t* app;

JNIEXPORT void JNICALL Java_com_dlocal_NewRelicJNI_init(JNIEnv* env,
                                                        jobject obj,
                                                        jstring appName,
                                                        jstring accountId) {
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
  }

  if (!newrelic_init(NULL, 0)) {
    printf("Error connecting to daemon.\n");
  }

  /* Wait up to 0 seconds for the SDK to connect to the daemon (Why 10 sec???? -
   * ANSWER doesnt impact transactions)
   */
  app = newrelic_create_app(config, 10000);
  newrelic_destroy_app_config(&config);
}

JNIEXPORT jstring JNICALL
Java_com_dlocal_NewRelicJNI_startWebTransaction(JNIEnv* env,
                                                jobject obj,
                                                jstring id,
                                                jstring transactionName,
                                                jstring segmentName,
                                                jstring segmentCustom) {
  newrelic_txn_t* txn;
  newrelic_segment_t* seg;
  jboolean iscopy;

  const char* transactionId;
  const char* transactionN;
  const char* segmentN;
  const char* segmentC;

  transactionId = (*env)->GetStringUTFChars(env, id, &iscopy);
  transactionN = (*env)->GetStringUTFChars(env, transactionName, &iscopy);
  segmentN = (*env)->GetStringUTFChars(env, segmentName, &iscopy);
  segmentC = (*env)->GetStringUTFChars(env, segmentCustom, &iscopy);

  printf("Transaction Id %s\n", transactionId);
  printf("Transaction Name %s\n", transactionN);
  printf("Segment Name %s\n", segmentN);
  printf("Segment Custom %s\n", segmentC);

  txn = newrelic_start_web_transaction(app, transactionN);
  seg = newrelic_start_segment(txn, segmentN, segmentC);

  transaction_id transaction = {id, seg, txn};

  // TODO
  // Save on some kin of transaction list

  return id;
}

JNIEXPORT void JNICALL
Java_com_dlocal_NewRelicJNI_endWebTransaction(JNIEnv* env,
                                              jobject obj,
                                              jstring transactionId) {
  // TODO
  // Recover from List of Transactions by id
  // transactionId

  // TODO: remove dashes below
  // newrelic_end_segment(txn, &seg);
  // newrelic_end_transaction(&txn);
}

JNIEXPORT void JNICALL Java_com_dlocal_NewRelicJNI_destroy(JNIEnv* env,
                                                           jobject obj) {
  newrelic_destroy_app(&app);
}