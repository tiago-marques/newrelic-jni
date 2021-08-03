/*DESCRIPTION
Tests the Supportability metric "Supportability/DistributedTrace/AcceptPayload/ParseException"
by removing information from the payload, in this case "ty":"App".
*/

/*CONFIG
  cfg->transaction_tracer.threshold = NEWRELIC_THRESHOLD_IS_OVER_DURATION;
  cfg->transaction_tracer.duration_us = 0;
  cfg->distributed_tracing.enabled = true;
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/Action/basic"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/Action/basic"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/AcceptPayload/ParseException"},
                                                          [1, "??", "??", "??", "??", "??"]]
  ]
]
*/


#include "common.h"

RUN_NONWEB_TXN("basic") {
  char* payload = "{\"v\":[0,1],\"d\":{\"ac\":\"432507\",\"ap\":\"4741547\",\"id\":\"3925aa3552e648dd\",\"tr\":\"3925aa3552e648dd\",\"pr\":1.82236,\"sa\":true,\"ti\":1538512769934,\"tk\":\"310705\"}}";
  newrelic_accept_distributed_trace_payload(txn, payload, NULL);
}
