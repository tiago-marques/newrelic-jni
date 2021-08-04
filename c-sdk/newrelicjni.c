#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <hashmap.c>
#include <string.h>
/*
 * A standalone example that demonstrates to users how to
 * configure logging, configure an app, connect an app,
 * start a transaction and a segment, and cleanly destroy
 * everything.
 */
#include "libnewrelic.h"
#include "newrelicjni.h"

struct transaction_t {
  char* id;
  newrelic_segment_t* seg;
  newrelic_txn_t* txn
};

int transaction_compare(const void* a, const void* b, void* udata) {
  const struct transaction_t* ua = a;
  const struct transaction_t* ub = b;
  return strcmp(ua->id, ub->id);
}

uint64_t transaction_hash(const void* item, uint64_t seed0, uint64_t seed1) {
  const struct transaction_t* user = item;
  return hashmap_sip(user->id, strlen(user->id), seed0, seed1);
}

struct hashmap* transactionmap;

newrelic_app_t* app;

JNIEXPORT void JNICALL Java_com_dlocal_NewRelicJNI_init(JNIEnv* env,
                                                        jobject obj,
                                                        jstring appName,
                                                        jstring accountId) {
  transactionmap = hashmap_new(sizeof(struct transaction_t), 0, 0, 0,
                               transaction_hash, transaction_compare, NULL);

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

  hashmap_set(transactionmap, &(struct transaction_t){
                                  .id = transactionId, .txn = txn, .seg = seg});

  return id;
}

JNIEXPORT void JNICALL
Java_com_dlocal_NewRelicJNI_endWebTransaction(JNIEnv* env,
                                              jobject obj,
                                              jstring id) {
  newrelic_txn_t* txn;
  newrelic_segment_t* seg;
  jboolean iscopy;

  const char* transactionId;
  transactionId = (*env)->GetStringUTFChars(env, id, &iscopy);
  // TODO
  // Recover from List of Transactions by id
  struct transaction_t* transaction;
  transaction = hashmap_delete(transactionmap,
                               &(struct transaction_t){.id = transactionId});
  // TODO: remove dashes below
  if (transaction) {
    txn = transaction->txn;
    seg = transaction->seg;

    newrelic_end_segment(txn, &seg);
    newrelic_end_transaction(&txn);
  }
}

JNIEXPORT void JNICALL Java_com_dlocal_NewRelicJNI_destroy(JNIEnv* env,
                                                           jobject obj) {
  newrelic_destroy_app(&app);
  hashmap_free(transactionmap);
}
