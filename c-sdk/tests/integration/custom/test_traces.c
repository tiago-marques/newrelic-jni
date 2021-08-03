/*DESCRIPTION
newrelic_start_segment() should record a custom segment and nest segments
correctly.
*/

/*CONFIG
  cfg->transaction_tracer.threshold = NEWRELIC_THRESHOLD_IS_OVER_DURATION;
  cfg->transaction_tracer.duration_us = 0;
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"OtherTransaction/Action/basic"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/Action/basic"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"other/s1"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"other/s2"},				  [1, "??", "??", "??", "??", "??"]],
    [{"name":"other/s3"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"other/s1",
      "scope":"OtherTransaction/Action/basic"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"other/s2",
      "scope":"OtherTransaction/Action/basic"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"other/s3",
      "scope":"OtherTransaction/Action/basic"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

/*EXPECT_TXN_TRACES
[
  "?? agent run id",
  [
    [
      "?? entry",
      "?? duration",
      "OtherTransaction/Action/basic",
      "<unknown>",
      [
        [
          0, {}, {},
          [
            "?? start time", "?? end time", "ROOT", "?? root attributes",
            [
              [
                "?? start time", "?? end time", "`0", "?? node attributes",
                [
                  [
                    "?? start time", "?? end time", "`1", "?? node attributes",
                    [
                      [
                        "?? start time", "?? end time", "`2", "?? node attributes",
                        []
                      ],
                      [
                        "?? start time", "?? end time", "`3", "?? node attributes",
                        []
                      ]
                    ]
                  ]
                ]
              ]
            ]
          ],
          {
            "intrinsics": {
              "totalTime": "??",
              "cpu_time": "??",
              "cpu_user_time": "??",
              "cpu_sys_time": "??"
            }
          }
        ],
        [
          "OtherTransaction/Action/basic",
          "other/s1",
          "other/s2",
          "other/s3"
        ]
      ],
      "?? txn guid",
      "?? reserved",
      "?? force persist",
      "?? x-ray sessions",
      null
    ]
  ]
]
*/

#include "common.h"
#include <unistd.h>

RUN_NONWEB_TXN("basic") {
  newrelic_segment_t* s1;
  newrelic_segment_t* s2;
  newrelic_segment_t* s3;

  s1 = newrelic_start_segment(txn, "s1", "other");

  s2 = newrelic_start_segment(txn, "s2", "other");
  usleep(2);
  newrelic_end_segment(txn, &s2);

  s3 = newrelic_start_segment(txn, "s3", "other");
  usleep(2);
  newrelic_end_segment(txn, &s3);

  newrelic_end_segment(txn, &s1);
}
