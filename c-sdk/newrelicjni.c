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

JNIEXPORT void JNICALL Java_src_NewRelicJNI_init(JNIEnv* env,
                                                 jobject obj,
                                                 jstring appName,
                                                 jstring accountId) {
  jint i;
  jboolean iscopy;
  const char* app;
  const char* account;
  app = (*env)->GetStringUTFChars(env, appName, &iscopy);
  account = (*env)->GetStringUTFChars(env, accountId, &iscopy);
  printf("App %s\n", app);
  printf("Account %s\n", account);
}

int main(void) {
  newrelic_app_t* app;
  newrelic_txn_t* txn;
  newrelic_app_config_t* config;
  newrelic_segment_t* seg;

  config = newrelic_create_app_config(
      "native", "a61337f3c5409a5bb31f95e90417b5b26221NRAL");

  if (!newrelic_configure_log("./c_sdk.log", NEWRELIC_LOG_INFO)) {
    printf("Error configuring logging.\n");
    return -1;
  }

  if (!newrelic_init(NULL, 0)) {
    printf("Error connecting to daemon.\n");
    return -1;
  }

  /* Wait up to 10 seconds for the SDK to connect to the daemon */
  app = newrelic_create_app(config, 10000);
  newrelic_destroy_app_config(&config);

  /* Start a web transaction and a segment */
  txn = newrelic_start_web_transaction(app, "Transaction name");
  seg = newrelic_start_segment(txn, "Segment name", "Custom");

  /* Interesting application code happens here */
  sleep(2);

  /* End the segment and web transaction */
  newrelic_end_segment(txn, &seg);
  newrelic_end_transaction(&txn);
  newrelic_destroy_app(&app);

  return 0;
}