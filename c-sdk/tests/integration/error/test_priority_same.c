/*DESCRIPTION
The SDK should report the last error when newrelic_notice_error is called
multiple times with the same error priority.
*/

/*CONFIG
  cfg->transaction_tracer.threshold = NEWRELIC_THRESHOLD_IS_OVER_DURATION;
  cfg->transaction_tracer.duration_us = 1;
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/Action/basic",
      "Last",
      "Exception",
      {
        "stack_trace": "??",
        "agentAttributes": "??",
        "intrinsics": "??"
      }
    ]
  ]
]
*/

/*EXPECT_ERROR_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": 1
  },
  [
    [
      {
        "type": "TransactionError",
        "timestamp": "??",
        "error.class": "Exception",
        "error.message": "Last",
        "transactionName": "OtherTransaction/Action/basic",
        "duration": "??",
        "nr.transactionGuid": "??"
      },
      {},
      {}
    ]
  ]
]
*/

#include "common.h"

RUN_NONWEB_TXN("basic") {
  newrelic_segment_t* s = newrelic_start_segment(txn, "s1", "other");

  newrelic_notice_error(txn, 3, "First", "Exception");
  newrelic_notice_error(txn, 3, "Last", "Exception");

  newrelic_end_segment(txn, &s);
}
