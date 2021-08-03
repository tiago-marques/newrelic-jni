/*
 * This file contains internal error data structures and functions.
 */
#ifndef NR_ERRORS_PRIVATE_HDR
#define NR_ERRORS_PRIVATE_HDR

#include "util_time.h"

/*
 * This is the agent's view of an error.
 *
 * It contains the error attributes, but not transaction information
 * (such as request parameters) that will be added when the error is added
 * to the harvest structure.  The char * fields are owned by the error, and
 * will be allocated and assigned when the error is created, and freed when
 * the error is destroyed.
 */
struct _nr_error_t {
  nrtime_t when;         /* When did this error occur */
  int priority;          /* Error priority - lowest to highest */
  char* message;         /* Error message */
  char* klass;           /* Error class */
  char* stacktrace_json; /* Stack trace in JSON format */
};

#endif /* NR_ERRORS_PRIVATE_HDR */
