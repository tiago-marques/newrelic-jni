/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class src_NewRelicJNI */

#ifndef _Included_com_dlocal_NewRelicJNI
#define _Included_com_dlocal_NewRelicJNI
#ifdef __cplusplus
extern "C" {
#endif

typedef struct _transaction_t {
  char *id;
  newrelic_segment_t *seg;
  newrelic_txn_t *txn
} transaction_t;

/*
 * Class:     src_NewRelicJNI
 * Method:    init
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_dlocal_NewRelicJNI_init(JNIEnv*,
                                                        jobject,
                                                        jstring,
                                                        jstring);

/*
 * Class:     src_NewRelicJNI
 * Method:    startWebTransaction
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT jstring JNICALL
Java_com_dlocal_NewRelicJNI_startWebTransaction(JNIEnv*,
                                                jobject,
                                                jstring,
                                                jstring,
                                                jstring,
                                                jstring);

/*
 * Class:     src_NewRelicJNI
 * Method:    endWebTransaction
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_dlocal_NewRelicJNI_endWebTransaction(JNIEnv*,
                                                                     jobject,
                                                                     jstring);

/*
 * Class:     src_NewRelicJNI
 * Method:    destroy
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_dlocal_NewRelicJNI_destroy(JNIEnv*, jobject);

#ifdef __cplusplus
}
#endif
#endif
