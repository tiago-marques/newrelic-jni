/*DESCRIPTION
Tests the Supportability metric "Supportability/DistributedTrace/AcceptPayload/Ignored/Multiple"
by calling newrelic_accept_distributed_trace_payload twice.
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
    [{"name":"DurationByCaller/App/432507/4741547/Unknown/all"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/App/432507/4741547/Unknown/allOther"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/Action/basic"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/Action/basic"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/AcceptPayload/Success"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/AcceptPayload/Ignored/Multiple"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"TransportDuration/App/432507/4741547/Unknown/all"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"TransportDuration/App/432507/4741547/Unknown/allOther"},
                                                          [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

#include "common.h"

RUN_NONWEB_TXN("basic") {
  char* payload = "{\"v\":[0,1],\"d\":{\"ty\":\"App\",\"ac\":\"432507\",\"ap\":\"4741547\",\"id\":\"3925aa3552e648dd\",\"tr\":\"3925aa3552e648dd\",\"pr\":1.82236,\"sa\":true,\"ti\":1538512769934,\"tk\":\"310705\"}}";
  newrelic_accept_distributed_trace_payload(txn, payload, NULL);
  newrelic_accept_distributed_trace_payload(txn, payload, NULL);
}
