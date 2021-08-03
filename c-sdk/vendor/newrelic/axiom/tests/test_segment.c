#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

#include "nr_segment_private.h"
#include "nr_segment.h"
#include "test_segment_helpers.h"
#include "util_memory.h"

#include "tlib_main.h"

/* A simple list type to affirm that traversing the tree of segments in
 * prefix order is correct.  This type is for test purposes only.
 */
#define NR_TEST_LIST_CAPACITY 10
typedef struct _nr_test_list_t {
  size_t capacity;

  int used;
  nr_segment_t* elements[NR_TEST_LIST_CAPACITY];

  nr_segment_post_iter_t post_callback;
  int post_used;
  nr_segment_t* post_elements[NR_TEST_LIST_CAPACITY];
} nr_test_list_t;

static void test_iterator_post_callback(nr_segment_t* segment,
                                        nr_test_list_t* list) {
  tlib_pass_if_not_null("post iterator must receive a valid segment", segment);
  tlib_pass_if_not_null("post iterator must receive a valid userdata", list);

  list->post_elements[list->post_used] = segment;
  list->post_used += 1;
}

static nr_segment_iter_return_t test_iterator_callback(nr_segment_t* segment,
                                                       void* userdata) {
  nr_test_list_t* list;

  if (nrunlikely(NULL == segment || NULL == userdata)) {
    return NR_SEGMENT_NO_POST_ITERATION_CALLBACK;
  }

  list = (nr_test_list_t*)userdata;
  list->elements[list->used] = segment;
  list->used = list->used + 1;

  if (list->post_callback) {
    return ((nr_segment_iter_return_t){.post_callback = list->post_callback,
                                       .userdata = userdata});
  }

  return NR_SEGMENT_NO_POST_ITERATION_CALLBACK;
}

static int test_segment_priority_comparator(const void* ptr1,
                                            const void* ptr2,
                                            void* userdata NRUNUSED) {
  const nr_segment_t* seg1 = (const nr_segment_t*)ptr1;
  const nr_segment_t* seg2 = (const nr_segment_t*)ptr2;

  if (seg1->priority > seg2->priority) {
    return 1;
  } else if (seg1->priority < seg2->priority) {
    return -1;
  } else {
    return 0;
  }
}

static void test_segment_new_txn_with_segment_root(void) {
  nrtxn_t* txn = new_txn(0);

  /*
   * Test : Normal operation. When a new transaction is started, affirm
   * that it has all the necessary initialization for maintaining
   * a tree of segments.
   */
  tlib_pass_if_not_null("A new transaction must have a segment root",
                        txn->segment_root);

  tlib_pass_if_size_t_equal(
      "A new transaction's segment root must initialise its children", 0,
      nr_segment_children_size(&txn->segment_root->children));

  tlib_pass_if_size_t_equal("A new transaction must have a segment count of 0",
                            txn->segment_count, 0);

  tlib_pass_if_ptr_equal(
      "A new transaction's current parent must be initialized to its segment "
      "root",
      txn->segment_root, nr_txn_get_current_segment(txn, NULL));

  tlib_pass_if_true(
      "A new transaction's segment root must have its start time initialized",
      0 != txn->abs_start_time, "Expected true");

  /* Force the call to nr_txn_end() to be successful */
  txn->status.path_is_frozen = 1;
  nr_txn_end(txn);

  tlib_pass_if_true(
      "An ended transaction's segment root must have its stop time initialized",
      0 != txn->segment_root->stop_time, "Expected true");

  tlib_pass_if_str_equal(
      "An ended transaction's segment root must have the same name as the root "
      "node",
      txn->name, nr_string_get(txn->trace_strings, txn->segment_root->name));

  nr_txn_destroy(&txn);
}

static void test_segment_start(void) {
  nr_segment_t* s = NULL;
  nr_segment_t* t = NULL;
  nr_segment_t* u = NULL;

  nr_segment_t* prev_parent = NULL;

  /* Use the helper function to leverage nr_txn_begin(), install a segment_root
   * in the transaction and set a start time */
  nrtxn_t* txn = new_txn(0);

  txn->status.recording = 1;

  /*
   * Test : Bad parameters.
   */
  s = nr_segment_start(NULL, NULL, NULL);
  tlib_pass_if_null("Starting a segment on a NULL txn must not succeed", s);

  /*
   * Test : Normal operation.
   *
   * Starting a segment with a NULL, or implicit, parent. The parenting
   * information for the segment must be supplied by the parent_stack on the
   * transaction. Let's start and end three segments and make sure that the
   * family of segments is well-formed.  Affirm that the parent, child, and
   * sibling relationships are all as expected.
   */

  /* Start a segment and affirm that it is well-formed */
  s = nr_segment_start(txn, NULL, NULL);
  tlib_pass_if_not_null("Starting a segment on a valid txn must succeed", s);

  tlib_pass_if_ptr_equal(
      "The most-recently started segment must be the transaction's current "
      "segment",
      nr_txn_get_current_segment(txn, NULL), s);

  tlib_pass_if_not_null(
      "Starting a segment on a valid txn must allocate space for children",
      &s->children);

  tlib_pass_if_uint64_t_equal("A started segment has default color WHITE",
                              s->color, NR_SEGMENT_WHITE);
  tlib_pass_if_uint64_t_equal("A started segment has default type CUSTOM",
                              s->type, NR_SEGMENT_CUSTOM);
  tlib_pass_if_ptr_equal("A started segment must save its transaction", s->txn,
                         txn);
  tlib_fail_if_uint64_t_equal("A started segment has an initialized start time",
                              s->start_time, 0);
  tlib_pass_if_null("A started segment has a NULL hash for user attributes",
                    s->user_attributes);
  tlib_pass_if_ptr_equal(
      "A segment started with an implicit parent must have the transaction's"
      " segment_root as parent",
      s->parent, txn->segment_root);

  /* Start and end a second segment, t */
  prev_parent = nr_txn_get_current_segment(txn, NULL);
  t = nr_segment_start(txn, NULL, NULL);
  tlib_pass_if_not_null("Starting a segment on a valid txn must succeed", t);
  tlib_pass_if_ptr_equal(
      "The most recently started segment must be the transaction's current "
      "segment",
      nr_txn_get_current_segment(txn, NULL), t);

  tlib_pass_if_ptr_equal(
      "A segment started with an implicit parent must have the previously "
      "current segment as parent",
      t->parent, prev_parent);

  tlib_pass_if_true("Ending a well-formed segment must succeed",
                    nr_segment_end(t), "Expected true");

  tlib_pass_if_ptr_equal(
      "The most recently started segment has ended; the current segment must "
      "be its parent",
      nr_txn_get_current_segment(txn, NULL), s);

  /* Start a third segment.  Its sibling should be the second segment, t */
  prev_parent = nr_txn_get_current_segment(txn, NULL);
  u = nr_segment_start(txn, NULL, NULL);
  tlib_pass_if_not_null("Starting a segment on a valid txn must succeed", u);
  tlib_pass_if_ptr_equal(
      "The most recently started segment must be the transaction's current "
      "segment",
      nr_txn_get_current_segment(txn, NULL), u);

  tlib_pass_if_ptr_equal(
      "A segment started with an implicit parent must have the previously "
      "current segment as parent",
      u->parent, prev_parent);

  tlib_pass_if_ptr_equal(
      "A segment started with a NULL parent must have the expected "
      "previous siblings",
      nr_segment_children_get_prev(&s->children, u), t);

  tlib_pass_if_null(
      "A segment started with a NULL parent must have the expected "
      "next siblings",
      nr_segment_children_get_next(&s->children, u));

  tlib_pass_if_true("Ending a well-formed segment must succeed",
                    nr_segment_end(u), "Expected true");

  /* Let the matriarch sleep */
  tlib_pass_if_true("Ending a well-formed segment must succeed",
                    nr_segment_end(s), "Expected true");

  /* Clean up */
  nr_txn_destroy(&txn);
}

static void test_segment_start_async(void) {
  nr_segment_t* mother;
  nr_segment_t* first_born;
  nr_segment_t* third_born;
  nr_segment_t* first_stepchild;
  nr_segment_t* first_grandchild;
  nr_segment_t* great_grandchild;

  /* Use the helper function to leverage nr_txn_begin(), install a segment_root
   * in the transaction and set a start time */
  nrtxn_t* txn = new_txn(0);

  txn->status.recording = 1;

  /* Build out a small tree of segments to test upon */
  mother = nr_segment_start(txn, NULL, NULL);
  first_born = nr_segment_start(txn, NULL, NULL);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("Starting a segment on a NULL txn must not succeed",
                    nr_segment_start(NULL, mother, "async_context"));

  /*
   * Test : Async operation. Starting a segment with an explicit parent,
   *        supplied as a parameter to nr_segment_start() has the expected
   *        impact on parent and sibling relationships.
   */
  first_stepchild = nr_segment_start(txn, mother, "async_context");
  tlib_pass_if_not_null(
      "Starting a segment on a valid txn and an explicit parent must succeed",
      first_stepchild);

  tlib_pass_if_ptr_equal(
      "The most recently started, explicitly-parented segment must not alter "
      "the NULL context's current segment",
      nr_txn_get_current_segment(txn, NULL), first_born);

  tlib_pass_if_not_null(
      "Starting a segment on a valid txn must allocate space for children",
      &first_stepchild->children);
  tlib_pass_if_uint64_t_equal("A started segment has default type CUSTOM",
                              first_stepchild->type, NR_SEGMENT_CUSTOM);
  tlib_pass_if_ptr_equal("A started segment must save its transaction",
                         first_stepchild->txn, txn);
  tlib_fail_if_uint64_t_equal("A started segment has an initialized start time",
                              first_stepchild->start_time, 0);
  tlib_pass_if_null("A started segment has a NULL hash for user attributes",
                    first_stepchild->user_attributes);
  tlib_pass_if_int_equal(
      "A started segment has an initialized async context",
      first_stepchild->async_context,
      nr_string_find(first_stepchild->txn->trace_strings, "async_context"));

  tlib_pass_if_ptr_equal(
      "A segment started with an explicit parent must have the explicit "
      "parent",
      first_stepchild->parent, mother);

  tlib_pass_if_ptr_equal(
      "A segment started with an explicit parent must have the explicit "
      "previous siblings",
      nr_segment_children_get_prev(&(mother->children), first_stepchild),
      first_born);

  /*
   * Test : Async operation. Starting a segment with no parent and a new
   *        context supplied as a parameter to nr_segment_start() has the
   *        expected impact on parent and sibling relationships.
   */
  first_grandchild = nr_segment_start(txn, NULL, "another_async");
  tlib_pass_if_not_null(
      "Starting a segment on a valid txn and an implicit parent must succeed",
      first_grandchild);

  tlib_pass_if_ptr_equal(
      "The most recently started, implicitly-parented segment must not alter "
      "the NULL context's current segment",
      nr_txn_get_current_segment(txn, NULL), first_born);

  tlib_pass_if_ptr_equal(
      "The most recently started, implicitly-parented segment must set the "
      "current segment for the new context",
      nr_txn_get_current_segment(txn, "another_async"), first_grandchild);

  tlib_pass_if_uint64_t_equal("A started segment has default type CUSTOM",
                              first_grandchild->type, NR_SEGMENT_CUSTOM);
  tlib_pass_if_ptr_equal("A started segment must save its transaction",
                         first_grandchild->txn, txn);
  tlib_fail_if_uint64_t_equal("A started segment has an initialized start time",
                              first_grandchild->start_time, 0);
  tlib_pass_if_null("A started segment has a NULL hash for user attributes",
                    first_grandchild->user_attributes);
  tlib_pass_if_int_equal(
      "A started segment has an initialized async context",
      first_grandchild->async_context,
      nr_string_find(first_grandchild->txn->trace_strings, "another_async"));

  tlib_pass_if_ptr_equal(
      "A segment started with an implicit parent must have the implied parent "
      "on the main context",
      first_grandchild->parent, first_born);

  tlib_pass_if_ptr_equal(
      "A segment started with an implicit parent must be a child of that "
      "parent",
      nr_segment_children_get(&first_born->children, 0), first_grandchild);

  /*
   * Test : Async operation. Starting a segment with no parent on the same
   *        context as first_grandchild should make it a child of that segment.
   */
  great_grandchild = nr_segment_start(txn, NULL, "another_async");
  tlib_pass_if_not_null(
      "Starting a segment on a valid txn and an implicit parent must succeed",
      great_grandchild);

  tlib_pass_if_ptr_equal(
      "The most recently started, implicitly-parented segment must not alter "
      "the NULL context's current segment",
      nr_txn_get_current_segment(txn, NULL), first_born);

  tlib_pass_if_ptr_equal(
      "The most recently started, implicitly-parented segment must set the "
      "current segment for the new context",
      nr_txn_get_current_segment(txn, "another_async"), great_grandchild);

  tlib_pass_if_uint64_t_equal("A started segment has default type CUSTOM",
                              great_grandchild->type, NR_SEGMENT_CUSTOM);
  tlib_pass_if_ptr_equal("A started segment must save its transaction",
                         great_grandchild->txn, txn);
  tlib_fail_if_uint64_t_equal("A started segment has an initialized start time",
                              great_grandchild->start_time, 0);
  tlib_pass_if_null("A started segment has a NULL hash for user attributes",
                    great_grandchild->user_attributes);
  tlib_pass_if_int_equal(
      "A started segment has an initialized async context",
      great_grandchild->async_context,
      nr_string_find(great_grandchild->txn->trace_strings, "another_async"));

  tlib_pass_if_ptr_equal(
      "A segment started with an implicit parent must have the implied parent "
      "on the same async context",
      great_grandchild->parent, first_grandchild);

  tlib_pass_if_ptr_equal(
      "A segment started with an implicit parent must be a child of that "
      "parent",
      nr_segment_children_get(&first_grandchild->children, 0),
      great_grandchild);

  /*
   * Test : Async operation. Starting a segment with an explicit parent,
   *        supplied as a parameter to nr_segment_start() has the expected
   *        impact on subsequent sibling relationships.
   */
  nr_segment_end(first_born);
  third_born = nr_segment_start(txn, NULL, NULL);
  tlib_pass_if_ptr_equal(
      "A segment started with an explicit parent must have the explicit "
      "next siblings",
      nr_segment_children_get_next(&(mother->children), first_stepchild),
      third_born);

  /* Clean up */
  nr_txn_destroy(&txn);
}

static void test_set_name(void) {
  nrtxn_t txnv = {0};
  nr_segment_t segment
      = {.type = NR_SEGMENT_CUSTOM, .txn = &txnv, .parent = NULL};

  /* Mock up transaction */
  txnv.status.recording = 1;
  txnv.trace_strings = nr_string_pool_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false("Setting a name on a NULL segment must not succeed",
                     nr_segment_set_name(NULL, "name"), "Expected false");

  tlib_pass_if_false("Setting a NULL name on a segment must not succeed",
                     nr_segment_set_name(&segment, NULL), "Expected false");

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_true("Setting a name on a segment must succeed",
                    nr_segment_set_name(&segment, "name"), "Expected true");

  tlib_pass_if_int_equal("A named segment has the expected name", segment.name,
                         nr_string_find(segment.txn->trace_strings, "name"));

  /* Clean up */
  nr_string_pool_destroy(&txnv.trace_strings);
}

static void test_add_child(void) {
  nr_segment_t mother = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};
  nr_segment_t segment = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false("Adding a NULL child to a parent must not succeed",
                     nr_segment_add_child(&mother, NULL), "Expected false");

  tlib_pass_if_false("Adding a child to a NULL parent must not succeed",
                     nr_segment_add_child(NULL, &segment), "Expected false");
}

static void test_add_metric(void) {
  nr_segment_t segment
      = {.type = NR_SEGMENT_CUSTOM, .parent = NULL, .metrics = NULL};
  nr_vector_t* vec;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal("Adding a metric to a NULL segment must not succeed",
                          false,
                          nr_segment_add_metric(NULL, "Dead Disco", false));
  tlib_pass_if_bool_equal(
      "Adding a NULL metric name to a segment must not succeed", false,
      nr_segment_add_metric(&segment, NULL, false));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_bool_equal(
      "Adding a scoped metric to a segment must succeed", true,
      nr_segment_add_metric(&segment, "Help I'm Alive", true));
  tlib_pass_if_not_null(
      "Adding a metric to a segment without an initialised segment vector must "
      "create a vector to store the segments",
      segment.metrics);
  tlib_pass_if_size_t_equal("Adding a metric to a segment must save the metric",
                            1, nr_vector_size(segment.metrics));
  tlib_pass_if_str_equal(
      "Adding a metric to a segment must save the name", "Help I'm Alive",
      ((nr_segment_metric_t*)nr_vector_get(segment.metrics, 0))->name);
  tlib_pass_if_bool_equal(
      "Adding a metric to a segment must save the scoping flag", true,
      ((nr_segment_metric_t*)nr_vector_get(segment.metrics, 0))->scoped);

  vec = segment.metrics;

  tlib_pass_if_bool_equal(
      "Adding an unscoped metric to a segment must succeed", true,
      nr_segment_add_metric(&segment, "Gimme Sympathy", false));
  tlib_pass_if_ptr_equal(
      "Adding a metric to a segment with an initialised segment vector must "
      "use the same vector",
      vec, segment.metrics);
  tlib_pass_if_size_t_equal("Adding a metric to a segment must save the metric",
                            2, nr_vector_size(segment.metrics));
  tlib_pass_if_str_equal(
      "Adding a metric to a segment must save the name", "Gimme Sympathy",
      ((nr_segment_metric_t*)nr_vector_get(segment.metrics, 1))->name);
  tlib_pass_if_bool_equal(
      "Adding a metric to a segment must save the scoping flag", false,
      ((nr_segment_metric_t*)nr_vector_get(segment.metrics, 1))->scoped);

  nr_vector_destroy(&segment.metrics);
}

static void test_set_parent_to_same(void) {
  nr_segment_t mother = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false("Setting a parent on a NULL segment must not succeed",
                     nr_segment_set_parent(NULL, &mother), "Expected false");

  /*
   *  Test : Normal operation.
   */
  tlib_pass_if_true(
      "Setting a well-formed segment with the same parent must succeed",
      nr_segment_set_parent(&mother, NULL), "Expected true");

  tlib_pass_if_null(
      "Setting a well-formed segment with a NULL parent means the segment must "
      "have a NULL parent",
      mother.parent);
}

static void test_set_null_parent(void) {
  nr_segment_t thing_one = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};
  nr_segment_t thing_two = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};

  nr_segment_t mother = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};
  nr_segment_t segment = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};

  /* Build mock segments, each with an array of children */
  nr_segment_children_init(&mother.children);
  nr_segment_add_child(&mother, &thing_one);

  nr_segment_children_init(&segment.children);
  nr_segment_add_child(&segment, &thing_two);

  tlib_pass_if_ptr_equal("Affirm my nuclear family is well-formed",
                         thing_one.parent, &mother);

  tlib_pass_if_ptr_equal("Affirm my nuclear family is well-formed",
                         thing_two.parent, &segment);

  /*
   *  Test : Normal operation.  Reparent a segment with a NULL parent.
   */
  tlib_pass_if_true(
      "Setting a well-formed segment with a new parent must succeed",
      nr_segment_set_parent(&segment, &mother), "Expected true");

  tlib_pass_if_ptr_equal(
      "Setting a well-formed segment with a new parent means the segment must "
      "have that new parent",
      segment.parent, &mother);

  tlib_pass_if_ptr_equal(
      "Setting a well-formed segment with a new parent means the segment must "
      "have expected prev siblings",
      nr_segment_children_get_prev(&mother.children, &segment), &thing_one);

  tlib_pass_if_null(
      "Setting a well-formed segment with a new parent means the segment must "
      "have expected next siblings",
      nr_segment_children_get_next(&mother.children, &segment));

  /* Clean up */
  nr_segment_children_deinit(&mother.children);
  nr_segment_destroy_fields(&mother);

  nr_segment_children_deinit(&segment.children);
  nr_segment_destroy_fields(&segment);
}

static void test_set_non_null_parent(void) {
  nr_segment_t thing_one = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};
  nr_segment_t thing_two = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};

  nr_segment_t mother = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};
  nr_segment_t segment = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};

  /* Build mock segments, each with an array of children */
  nr_segment_children_init(&segment.children);
  nr_segment_add_child(&segment, &thing_two);

  nr_segment_children_init(&mother.children);
  nr_segment_add_child(&mother, &thing_one);
  nr_segment_add_child(&mother, &segment);

  tlib_pass_if_ptr_equal("Affirm my nuclear family is well-formed",
                         thing_one.parent, &mother);

  tlib_pass_if_ptr_equal("Affirm my nuclear family is well-formed",
                         segment.parent, &mother);
  /*
   *  Test : Normal operation.  Re-parent a segment with a non-NULL parent.
   */
  tlib_pass_if_true(
      "Setting a well-formed segment with a new parent must succeed",
      nr_segment_set_parent(&thing_one, &segment), "Expected true");

  tlib_pass_if_ptr_equal(
      "Setting a well-formed segment with a new parent means the segment must "
      "have that new parent",
      thing_one.parent, &segment);

  tlib_pass_if_ptr_equal(
      "Setting a well-formed segment with a new parent means the segment must "
      "have expected prev siblings",
      nr_segment_children_get_prev(&segment.children, &thing_one), &thing_two);

  tlib_pass_if_null(
      "Setting a well-formed segment with a new parent means the segment must "
      "have expected next siblings",
      nr_segment_children_get_next(&segment.children, &thing_one));

  tlib_pass_if_ptr_equal(
      "Setting a well-formed segment with a new parent means the old parent "
      "must "
      "have a new first child",
      nr_segment_children_get(&mother.children, 0), &segment);

  tlib_fail_if_ptr_equal(
      "Setting a well-formed segment with a new parent means the segment must "
      "not be a child of its old parent",
      nr_segment_children_get(&mother.children, 0), &thing_one);

  /* Clean up */
  nr_segment_children_deinit(&mother.children);
  nr_segment_destroy_fields(&mother);

  nr_segment_children_deinit(&segment.children);
  nr_segment_destroy_fields(&segment);
}

static void test_set_parent_different_txn(void) {
  nrtxn_t txn_one, txn_two;
  nr_segment_t thing_one = {.type = NR_SEGMENT_CUSTOM, .txn = &txn_one};
  nr_segment_t thing_two = {.type = NR_SEGMENT_CUSTOM, .txn = &txn_two};

  tlib_pass_if_bool_equal(
      "A segment cannot be parented to a segment on a different transaction",
      false, nr_segment_set_parent(&thing_one, &thing_two));
  tlib_pass_if_bool_equal(
      "A segment cannot be parented to a segment on a different transaction",
      false, nr_segment_set_parent(&thing_two, &thing_one));

  tlib_pass_if_ptr_equal(
      "A failed nr_segment_set_parent() call must not change the parent",
      &txn_one, thing_one.txn);
  tlib_pass_if_ptr_equal(
      "A failed nr_segment_set_parent() call must not change the parent",
      &txn_two, thing_two.txn);
}

static void test_set_timing(void) {
  nr_segment_t segment = {.start_time = 1234, .stop_time = 5678};

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false("Setting timing on a NULL segment must not succeed",
                     nr_segment_set_timing(NULL, 0, 0), "Expected false");

  /*
   *  Test : Normal operation.
   */
  tlib_pass_if_true("Setting timing on a well-formed segment must succeed",
                    nr_segment_set_timing(&segment, 2000, 5), "Expected false");

  tlib_pass_if_true(
      "Setting timing on a well-formed segment must alter the start time",
      2000 == segment.start_time, "Expected true");

  tlib_pass_if_true(
      "Setting timing on a well-formed segment must alter the stop time",
      2005 == segment.stop_time, "Expected true");
}

static void test_end_segment(void) {
  nrtxn_t txnv = {.segment_count = 0, .parent_stacks = nr_hashmap_create(NULL)};
  nr_segment_t s = {.start_time = 1234, .stop_time = 0, .txn = &txnv};
  nr_segment_t t = {.start_time = 1234, .stop_time = 5678, .txn = &txnv};
  nr_segment_t u = {.txn = 0};

  /* Mock up the parent stacks used by the txn */
  nr_stack_t parent_stack;

  nr_stack_init(&parent_stack, 32);
  nr_hashmap_index_set(txnv.parent_stacks, 0, (void*)&parent_stack);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false("Ending a NULL segment must not succeed",
                     nr_segment_end(NULL), "Expected false");

  tlib_pass_if_false("Ending a segment with a NULL txn must not succeed",
                     nr_segment_end(&u), "Expected false");

  tlib_pass_if_true(
      "Ending ill-formed segments must not alter the transaction's segment "
      "count",
      0 == txnv.segment_count, "Expected true");

  /*
   * Test : Normal operation. Ending a segment with
   * stop_time = 0.
   */
  tlib_pass_if_true("Ending a well-formed segment must succeed",
                    nr_segment_end(&s), "Expected true");

  tlib_pass_if_true(
      "Ending a well-formed segment with a zero-value stop "
      "time must alter the stop time",
      0 != t.stop_time, "Expected true");

  tlib_pass_if_true(
      "Ending a well-formed segment must increment the "
      "transaction's segment count by 1",
      1 == txnv.segment_count, "Expected true");

  /*
   * Test : Normal operation. Ending a segment with stop_time != 0.
   */
  tlib_pass_if_true("Ending a well-formed segment must succeed",
                    nr_segment_end(&t), "Expected true");

  tlib_pass_if_true(
      "Ending a well-formed segment with a non-zero stop "
      "time must not alter the stop time",
      5678 == t.stop_time, "Expected true");

  tlib_pass_if_true(
      "Ending a well-formed segment must increment the transaction's segment "
      "count by 1",
      2 == txnv.segment_count, "Expected true");

  /* Clean up */
  nr_hashmap_destroy(&txnv.parent_stacks);
  nr_stack_destroy_fields(&parent_stack);
}

static void test_end_segment_async(void) {
  nr_segment_t* aa;
  nr_segment_t* ab;
  nr_segment_t* ba;
  nr_segment_t* bb;
  nrtxn_t* txn = new_txn(0);

  txn->status.recording = 1;

  /*
   * Test : Ending a segment on an async context should only affect that stack.
   */
  aa = nr_segment_start(txn, NULL, "a");

  tlib_pass_if_ptr_equal(
      "Segment aa should have the transaction's segment root as its parent",
      txn->segment_root, aa->parent);

  tlib_pass_if_size_t_equal(
      "Context a should have exactly one element in its parent stack", 1,
      nr_vector_size((nr_stack_t*)nr_hashmap_index_get(
          txn->parent_stacks, (uint64_t)aa->async_context)));

  tlib_pass_if_ptr_equal(
      "Context a should have aa as the only element in its parent stack", aa,
      nr_txn_get_current_segment(txn, "a"));

  tlib_pass_if_ptr_equal(
      "The main context should have the transaction's segment root as its "
      "current segment",
      txn->segment_root, nr_txn_get_current_segment(txn, NULL));

  ab = nr_segment_start(txn, NULL, "a");

  tlib_pass_if_ptr_equal("Segment ab should have aa as its parent", aa,
                         ab->parent);

  tlib_pass_if_size_t_equal(
      "Context a should have exactly two elements in its parent stack", 2,
      nr_vector_size((nr_stack_t*)nr_hashmap_index_get(
          txn->parent_stacks, (uint64_t)aa->async_context)));

  tlib_pass_if_ptr_equal(
      "Context a should have ab as the current element in its parent stack", ab,
      nr_txn_get_current_segment(txn, "a"));

  tlib_pass_if_ptr_equal(
      "The main context should have the transaction's segment root as its "
      "current segment",
      txn->segment_root, nr_txn_get_current_segment(txn, NULL));

  nr_segment_end(ab);

  tlib_pass_if_size_t_equal(
      "Context a should have exactly one element in its parent stack", 1,
      nr_vector_size((nr_stack_t*)nr_hashmap_index_get(
          txn->parent_stacks, (uint64_t)aa->async_context)));

  tlib_pass_if_ptr_equal(
      "Context a should have aa as the only element in its parent stack", aa,
      nr_txn_get_current_segment(txn, "a"));

  tlib_pass_if_ptr_equal(
      "The main context should have the transaction's segment root as its "
      "current segment",
      txn->segment_root, nr_txn_get_current_segment(txn, NULL));

  /*
   * Test : As above, but when the parent segment is ended first, only the child
   *        should remain in the stack.
   */
  ba = nr_segment_start(txn, NULL, "b");

  tlib_pass_if_ptr_equal(
      "Segment ba should have the transaction's segment root as its parent",
      txn->segment_root, ba->parent);

  tlib_pass_if_size_t_equal(
      "Context b should have exactly one element in its parent stack", 1,
      nr_vector_size((nr_stack_t*)nr_hashmap_index_get(
          txn->parent_stacks, (uint64_t)ba->async_context)));

  tlib_pass_if_ptr_equal(
      "Context b should have ba as the only element in its parent stack", ba,
      nr_txn_get_current_segment(txn, "b"));

  tlib_pass_if_ptr_equal(
      "The main context should have the transaction's segment root as its "
      "current segment",
      txn->segment_root, nr_txn_get_current_segment(txn, NULL));

  bb = nr_segment_start(txn, NULL, "b");

  tlib_pass_if_ptr_equal("Segment bb should have ba as its parent", ba,
                         bb->parent);

  tlib_pass_if_size_t_equal(
      "Context b should have exactly two elements in its parent stack", 2,
      nr_vector_size((nr_stack_t*)nr_hashmap_index_get(
          txn->parent_stacks, (uint64_t)bb->async_context)));

  tlib_pass_if_ptr_equal(
      "Context b should have bb as the current element in its parent stack", bb,
      nr_txn_get_current_segment(txn, "b"));

  tlib_pass_if_ptr_equal(
      "The main context should have the transaction's segment root as its "
      "current segment",
      txn->segment_root, nr_txn_get_current_segment(txn, NULL));

  nr_segment_end(ba);

  tlib_pass_if_size_t_equal(
      "Context b should have exactly one element in its parent stack", 1,
      nr_vector_size((nr_stack_t*)nr_hashmap_index_get(
          txn->parent_stacks, (uint64_t)bb->async_context)));

  tlib_pass_if_ptr_equal(
      "Context b should have bb as the only element in its parent stack", bb,
      nr_txn_get_current_segment(txn, "b"));

  tlib_pass_if_ptr_equal(
      "The main context should have the transaction's segment root as its "
      "current segment",
      txn->segment_root, nr_txn_get_current_segment(txn, NULL));

  nr_txn_destroy(&txn);
}

static void test_segment_iterate_nulls(void) {
  nr_segment_t segment = {.type = NR_SEGMENT_CUSTOM, .name = 1};
  nr_test_list_t list = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};

  /*
   * Test : Bad parameters.
   */
  nr_segment_iterate(NULL, (nr_segment_iter_t)test_iterator_callback, &list);
  nr_segment_iterate(&segment, (nr_segment_iter_t)NULL, &list);
  nr_segment_iterate(&segment, (nr_segment_iter_t)test_iterator_callback, NULL);

  tlib_pass_if_int_equal(
      "Traversing with bad parameters must result in an empty list", list.used,
      0);
}

static void test_segment_iterate_bachelor(void) {
  nr_segment_t bachelor_1 = {.type = NR_SEGMENT_CUSTOM, .name = 1};
  nr_segment_t bachelor_2 = {.type = NR_SEGMENT_CUSTOM, .name = 2};

  nr_test_list_t list_1 = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};
  nr_test_list_t list_2 = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};

  /* Bachelor 1 has no room for children; Bachelor 2 does.  Each
   * bachelor needs be regarded as a leaf node.
   */
  nr_segment_children_init(&bachelor_2.children);

  /*
   * Test : Normal operation.  Traversing a tree of 1.
   */
  nr_segment_iterate(&bachelor_1, (nr_segment_iter_t)test_iterator_callback,
                     &list_1);

  tlib_pass_if_int_equal(
      "Traversing a tree of one node must create a one-node list",
      list_1.elements[0]->name, bachelor_1.name);

  /*
   * Test : Normal operation.  Traversing a tree of 1, where
   *        the node has allocated room for children.
   */
  nr_segment_iterate(&bachelor_2, (nr_segment_iter_t)test_iterator_callback,
                     &list_2);

  tlib_pass_if_int_equal(
      "Traversing a tree of one node must create a one-node list",
      list_2.elements[0]->name, bachelor_2.name);

  /* Clean up */
  nr_segment_children_deinit(&bachelor_2.children);
}

static void test_segment_iterate(void) {
  int i;
  nr_test_list_t list = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};

  /* Declare eight segments; give them .name values in pre-order */
  nr_segment_t grandmother = {.type = NR_SEGMENT_CUSTOM, .name = 0};

  nr_segment_t grown_child_1 = {.type = NR_SEGMENT_CUSTOM, .name = 1};
  nr_segment_t grown_child_2 = {.type = NR_SEGMENT_CUSTOM, .name = 3};
  nr_segment_t grown_child_3 = {.type = NR_SEGMENT_CUSTOM, .name = 7};

  nr_segment_t child_1 = {.type = NR_SEGMENT_CUSTOM, .name = 2};
  nr_segment_t child_2 = {.type = NR_SEGMENT_CUSTOM, .name = 4};
  nr_segment_t child_3 = {.type = NR_SEGMENT_CUSTOM, .name = 5};
  nr_segment_t child_4 = {.type = NR_SEGMENT_CUSTOM, .name = 6};

  /* Build a mock tree of segments */
  nr_segment_children_init(&grandmother.children);
  nr_segment_add_child(&grandmother, &grown_child_1);
  nr_segment_add_child(&grandmother, &grown_child_2);
  nr_segment_add_child(&grandmother, &grown_child_3);

  nr_segment_children_init(&grown_child_1.children);
  nr_segment_add_child(&grown_child_1, &child_1);

  nr_segment_children_init(&grown_child_2.children);
  nr_segment_add_child(&grown_child_2, &child_2);
  nr_segment_add_child(&grown_child_2, &child_3);
  nr_segment_add_child(&grown_child_2, &child_4);

  /*
   * The mock tree looks like this:
   *
   *               --------(0)grandmother---------
   *                /             |              \
   *    (1)grown_child_1   (3)grown_child_2    (7)grown_child_3
   *       /                /      |      \
   * (2)child_1    (4)child_2  (5)child_3  (6)child_4
   *
   *
   * In pre-order, that's: 0 1 2 3 4 5 6 7
   */

  nr_segment_iterate(&grandmother, (nr_segment_iter_t)test_iterator_callback,
                     &list);

  tlib_pass_if_uint64_t_equal("The subsequent list has eight elements", 8,
                              list.used);

  for (i = 0; i < list.used; i++) {
    tlib_pass_if_int_equal("A tree must be traversed pre-order",
                           list.elements[i]->name, i);
    tlib_pass_if_uint64_t_equal("A traversed tree must supply grey nodes",
                                list.elements[i]->color, NR_SEGMENT_GREY);
  }

  /* Clean up */
  nr_segment_children_deinit(&grandmother.children);
  nr_segment_children_deinit(&grown_child_1.children);
  nr_segment_children_deinit(&grown_child_2.children);
}

/* The C Agent API gives customers the ability to arbitrarily parent a segment
 * with any other segment.  It is  possible that one could make a mistake in
 * manually parenting segments and introduce a cycle into the tree.  Test that
 * tree iteration is hardened against this possibility.
 */
static void test_segment_iterate_cycle_one(void) {
  int i;
  nr_test_list_t list = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};

  /* Declare three segments; give them .name values in pre-order */
  nr_segment_t grandmother = {.type = NR_SEGMENT_CUSTOM, .name = 0};
  nr_segment_t grown_child = {.type = NR_SEGMENT_CUSTOM, .name = 1};
  nr_segment_t child = {.type = NR_SEGMENT_CUSTOM, .name = 2};

  /* Build a mock ill-formed tree of segments; the tree shall have a cycle. */
  nr_segment_children_init(&grandmother.children);
  nr_segment_add_child(&grandmother, &grown_child);

  nr_segment_children_init(&grown_child.children);
  nr_segment_add_child(&grown_child, &child);

  nr_segment_children_init(&child.children);
  nr_segment_add_child(&child, &grandmother);

  /*
   * The ill-formed tree looks like this:
   *
   *                      +-----<------+
   *                      |            |
   *               (0)grandmother      |
   *                      |            |
   *                (1)grown_child     |
   *                      |            |
   *                  (2)child         |
   *                      |            |
   *                      +----->------+
   *
   * In pre-order, that's: 0 1 2
   *     but oooooh, there's a cycle.  That's not a tree, it's a graph!
   */
  nr_segment_iterate(&grandmother, (nr_segment_iter_t)test_iterator_callback,
                     &list);

  tlib_pass_if_uint64_t_equal("The subsequent list has three elements", 3,
                              list.used);

  for (i = 0; i < list.used; i++) {
    tlib_pass_if_int_equal("A tree must be traversed pre-order",
                           list.elements[i]->name, i);
    tlib_pass_if_uint64_t_equal(
        "A one-time traversed tree must supply grey nodes",
        list.elements[i]->color, NR_SEGMENT_GREY);
  }

  /* Clean up */
  nr_segment_children_deinit(&grandmother.children);
  nr_segment_children_deinit(&grown_child.children);
  nr_segment_children_deinit(&child.children);
}

static void test_segment_iterate_cycle_two(void) {
  int i;
  nr_test_list_t list_1 = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};
  nr_test_list_t list_2 = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};

  /* Declare three segments; give them .name values in pre-order */
  nr_segment_t grandmother = {.type = NR_SEGMENT_CUSTOM, .name = 0};
  nr_segment_t grown_child_1 = {.type = NR_SEGMENT_CUSTOM, .name = 1};
  nr_segment_t grown_child_2 = {.type = NR_SEGMENT_CUSTOM, .name = 2};
  nr_segment_t child = {.type = NR_SEGMENT_CUSTOM, .name = 3};

  /* Build a mock ill-formed tree of segments; the tree shall have a cycle. */
  nr_segment_children_init(&grandmother.children);
  nr_segment_add_child(&grandmother, &grown_child_1);
  nr_segment_add_child(&grandmother, &grown_child_2);

  nr_segment_children_init(&grown_child_1.children);
  nr_segment_add_child(&grown_child_1, &grandmother);

  nr_segment_children_init(&grown_child_2.children);
  nr_segment_add_child(&grown_child_2, &child);

  /*
   * The ill-formed tree looks like this:
   *
   *  +---------->----------+
   *  |                      |
   *  |               (0)grandmother
   *  |                 /       \
   *  |    (1)grown_child_1    (2)grown_child_2
   *  |            |            /
   *  +------------+     (3)child
   *
   *
   * In pre-order, that's: 0 1 2 3
   *     but oooooh, there's a cycle.  That's not a tree, it's a graph!
   */
  nr_segment_iterate(&grandmother, (nr_segment_iter_t)test_iterator_callback,
                     &list_1);

  tlib_pass_if_uint64_t_equal("The subsequent list has four elements", 4,
                              list_1.used);

  for (i = 0; i < list_1.used; i++) {
    tlib_pass_if_int_equal("A tree must be traversed pre-order",
                           list_1.elements[i]->name, i);
    tlib_pass_if_uint64_t_equal(
        "A one-time traversed tree must supply grey nodes",
        list_1.elements[i]->color, NR_SEGMENT_GREY);
  }

  nr_segment_iterate(&grandmother, (nr_segment_iter_t)test_iterator_callback,
                     &list_2);

  tlib_pass_if_uint64_t_equal("The subsequent list has four elements", 4,
                              list_2.used);

  for (i = 0; i < list_2.used; i++) {
    tlib_pass_if_int_equal("A tree must be traversed pre-order",
                           list_2.elements[i]->name, i);
    tlib_pass_if_uint64_t_equal(
        "A two-time traversed tree must supply white nodes",
        list_2.elements[i]->color, NR_SEGMENT_WHITE);
  }

  /* Clean up */
  nr_segment_children_deinit(&grandmother.children);
  nr_segment_children_deinit(&grown_child_1.children);
  nr_segment_children_deinit(&grown_child_2.children);
}

static void test_segment_iterate_with_amputation(void) {
  int i;
  nr_test_list_t list = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};

  /* Declare seven segments; give them .name values in pre-order */
  nr_segment_t grandmother = {.type = NR_SEGMENT_CUSTOM, .name = 0};

  nr_segment_t grown_child_1 = {.type = NR_SEGMENT_CUSTOM, .name = 1};
  nr_segment_t grown_child_2 = {.type = NR_SEGMENT_CUSTOM, .name = 3};

  nr_segment_t child_1 = {.type = NR_SEGMENT_CUSTOM, .name = 2};

  /* Build a mock tree of segments */
  nr_segment_children_init(&grandmother.children);
  nr_segment_add_child(&grandmother, &grown_child_1);
  nr_segment_add_child(&grandmother, &grown_child_1);
  nr_segment_add_child(&grandmother, &grown_child_2);

  nr_segment_children_init(&grown_child_1.children);
  nr_segment_add_child(&grown_child_1, &child_1);

  /*
   * The mock tree looks like this:
   *
   *               --------(0)grandmother---------
   *                /             |              \
   *    (1)grown_child_1   (1)grown_child_1    (3)grown_child_2
   *       |                  |
   * (2)child_1          (2)child_1
   *
   *
   * In pre-order, that's: 0 1 2 1 2 3
   *   Except!  Segment 1 "grown_child_1" appears twice in the tree.  The
   * implementation of nr_segment_iterate() is such that every unique segment is
   * traversed only once. This means that the second child of the grandmother,
   * and all of its children, will be amputated from the subsequent trace.
   *
   * So the expected traversal is: 0 1 2 3
   */
  nr_segment_iterate(&grandmother, (nr_segment_iter_t)test_iterator_callback,
                     &list);

  tlib_pass_if_uint64_t_equal("The subsequent list has four elements", 4,
                              list.used);

  for (i = 0; i < list.used; i++) {
    tlib_pass_if_int_equal("A tree must be traversed pre-order",
                           list.elements[i]->name, i);
    tlib_pass_if_uint64_t_equal(
        "A one-time traversed tree must supply grey nodes",
        list.elements[i]->color, NR_SEGMENT_GREY);
  }

  /* Clean up */
  nr_segment_children_deinit(&grandmother.children);
  nr_segment_children_deinit(&grown_child_1.children);
}

static void test_segment_iterate_with_post_callback(void) {
  int i;
  nr_test_list_t list
      = {.capacity = NR_TEST_LIST_CAPACITY,
         .used = 0,
         .post_callback = (nr_segment_post_iter_t)test_iterator_post_callback,
         .post_used = 0};

  /* Declare eight segments; give them .name values in post-order */
  nr_segment_t grandmother = {.type = NR_SEGMENT_CUSTOM, .name = 7};

  nr_segment_t grown_child_1 = {.type = NR_SEGMENT_CUSTOM, .name = 1};
  nr_segment_t grown_child_2 = {.type = NR_SEGMENT_CUSTOM, .name = 5};
  nr_segment_t grown_child_3 = {.type = NR_SEGMENT_CUSTOM, .name = 6};

  nr_segment_t child_1 = {.type = NR_SEGMENT_CUSTOM, .name = 0};
  nr_segment_t child_2 = {.type = NR_SEGMENT_CUSTOM, .name = 2};
  nr_segment_t child_3 = {.type = NR_SEGMENT_CUSTOM, .name = 3};
  nr_segment_t child_4 = {.type = NR_SEGMENT_CUSTOM, .name = 4};

  /* Build a mock tree of segments */
  nr_segment_children_init(&grandmother.children);
  nr_segment_add_child(&grandmother, &grown_child_1);
  nr_segment_add_child(&grandmother, &grown_child_2);
  nr_segment_add_child(&grandmother, &grown_child_3);

  nr_segment_children_init(&grown_child_1.children);
  nr_segment_add_child(&grown_child_1, &child_1);

  nr_segment_children_init(&grown_child_2.children);
  nr_segment_add_child(&grown_child_2, &child_2);
  nr_segment_add_child(&grown_child_2, &child_3);
  nr_segment_add_child(&grown_child_2, &child_4);

  /*
   * The mock tree looks like this:
   *
   *               --------(7)grandmother---------
   *                /             |              \
   *    (1)grown_child_1   (5)grown_child_2    (6)grown_child_3
   *       /                /      |      \
   * (0)child_1    (2)child_2  (3)child_3  (4)child_4
   *
   *
   * In post-order, that's: 0 1 2 3 4 5 6 7
   */

  nr_segment_iterate(&grandmother, (nr_segment_iter_t)test_iterator_callback,
                     &list);

  tlib_pass_if_uint64_t_equal("The subsequent list has eight elements", 8,
                              list.used);
  tlib_pass_if_uint64_t_equal("The post callback was invoked eight times", 8,
                              list.post_used);

  for (i = 0; i < list.post_used; i++) {
    tlib_pass_if_int_equal(
        "A tree must be traversed post-order by post-callbacks",
        list.post_elements[i]->name, i);
  }

  /* Clean up */
  nr_segment_children_deinit(&grandmother.children);
  nr_segment_children_deinit(&grown_child_1.children);
  nr_segment_children_deinit(&grown_child_2.children);
}

static void test_segment_destroy(void) {
  nr_segment_t* bachelor_1 = nr_zalloc(sizeof(nr_segment_t));
  nr_segment_t* bachelor_2 = nr_zalloc(sizeof(nr_segment_t));

  /* Bachelor 1 has no room for children; Bachelor 2 does.  Each
   * bachelor needs be regarded as a leaf node.
   */
  nr_segment_children_init(&bachelor_2->children);

  /*
   * Test : Bad parameters.
   */
  nr_segment_destroy(NULL);

  /*
   * Test : Normal operation.  Free a tree of one segment that has
   * no room for children.  i.e., segment.children->children is
   * NULL.
   */
  nr_segment_destroy(bachelor_1);

  /*
   * Test : Normal operation.  Free a tree of one segment that has
   * room for children but no children.  i.e., segment.children->children is
   * is not NULL.
   */
  nr_segment_destroy(bachelor_2);

  nr_free(bachelor_1);
  nr_free(bachelor_2);
}

static void test_segment_destroy_tree(void) {
  int i;
  nr_slab_t* slab = nr_slab_create(sizeof(nr_segment_t), 0);
  char* test_string = "0123456789";
  nr_test_list_t list = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};

  /* Declare eight segments; give them .name values in pre-order */
  nr_segment_t* grandmother = nr_slab_next(slab);

  nr_segment_t* grown_child_1 = nr_slab_next(slab);
  nr_segment_t* grown_child_2 = nr_slab_next(slab);
  nr_segment_t* grown_child_3 = nr_slab_next(slab);

  nr_segment_t* child_1 = nr_slab_next(slab);
  nr_segment_t* child_2 = nr_slab_next(slab);
  nr_segment_t* child_3 = nr_slab_next(slab);
  nr_segment_t* child_4 = nr_slab_next(slab);

  grown_child_1->name = 1;
  grown_child_2->name = 3;
  grown_child_3->name = 7;

  child_1->name = 2;
  child_2->name = 4;
  child_3->name = 5;
  child_4->name = 6;

  /*
   * Test : Normal operation.  Mock up a dynamically-allocated
   * tree and affirm that using nr_segment_destroy results in
   * 0 leaks.
   */

  /* Build a mock tree of segments */
  nr_segment_children_init(&grandmother->children);
  nr_segment_add_child(grandmother, grown_child_1);
  nr_segment_add_child(grandmother, grown_child_2);
  nr_segment_add_child(grandmother, grown_child_3);

  nr_segment_children_init(&grown_child_1->children);
  nr_segment_add_child(grown_child_1, child_1);

  nr_segment_children_init(&grown_child_2->children);
  nr_segment_add_child(grown_child_2, child_2);
  nr_segment_add_child(grown_child_2, child_3);
  nr_segment_add_child(grown_child_2, child_4);

  /* Make a couple of nodes external and datastore, so we know those
   * attributes are getting destroyed
   */
  child_1->type = NR_SEGMENT_EXTERNAL;
  child_1->typed_attributes.external.transaction_guid = nr_strdup(test_string);
  child_1->typed_attributes.external.uri = nr_strdup(test_string);
  child_1->typed_attributes.external.library = nr_strdup(test_string);
  child_1->typed_attributes.external.procedure = nr_strdup(test_string);

  // clang-format off
  grown_child_2->type = NR_SEGMENT_DATASTORE;
  grown_child_2->typed_attributes.datastore.component = nr_strdup(test_string);
  grown_child_2->typed_attributes.datastore.sql = nr_strdup(test_string);
  grown_child_2->typed_attributes.datastore.sql_obfuscated = nr_strdup(test_string);
  grown_child_2->typed_attributes.datastore.input_query_json = nr_strdup(test_string);
  grown_child_2->typed_attributes.datastore.backtrace_json = nr_strdup(test_string);
  grown_child_2->typed_attributes.datastore.explain_plan_json = nr_strdup(test_string);
  grown_child_2->typed_attributes.datastore.instance.host = nr_strdup(test_string);
  grown_child_2->typed_attributes.datastore.instance.port_path_or_id
      = nr_strdup(test_string);
  grown_child_2->typed_attributes.datastore.instance.database_name = nr_strdup(test_string);
  // clang-format on

  /*
   * The mock tree looks like this:
   *
   *               --------(0)grandmother---------
   *                /             |              \
   *    (1)grown_child_1   (3)grown_child_2    (7)grown_child_3
   *       /                /      |      \
   * (2)child_1    (4)child_2  (5)child_3  (6)child_4
   *
   *
   * In pre-order, that's: 0 1 2 3 4 5 6 7
   */

  nr_segment_iterate(grandmother, (nr_segment_iter_t)test_iterator_callback,
                     &list);

  tlib_pass_if_uint64_t_equal("The subsequent list has eight elements", 8,
                              list.used);

  /* Valgrind will check against memory leaks, but it's nice to affirm that
   * every node in the tree was visited exactly once. */
  for (i = 0; i < list.used; i++) {
    tlib_pass_if_int_equal("A tree must be traversed pre-order",
                           list.elements[i]->name, i);
    tlib_pass_if_uint64_t_equal("A traversed tree must supply grey nodes",
                                list.elements[i]->color, NR_SEGMENT_GREY);
  }

  /* Affirm that we can free an entire, dynamically-allocated tree
   * of segments.  The valgrind check will affirm nothing is faulted or
   * leaked. */
  nr_segment_destroy(grandmother);
  nr_slab_destroy(&slab);
}

static void test_segment_discard(void) {
  nrtxn_t txn = {0};
  nr_segment_t* A;
  nr_segment_t* B;
  nr_segment_t* C;
  nr_segment_t* D;

  txn.status.recording = 1;
  txn.segment_slab = nr_slab_create(sizeof(nr_segment_t), 0);

  /* Bad parameters. */
  tlib_pass_if_false("NULL address", nr_segment_discard(NULL),
                     "expected false");
  tlib_pass_if_false("NULL segment", nr_segment_discard(&txn.segment_root),
                     "expected false");

  txn.segment_root = nr_slab_next(txn.segment_slab);

  tlib_pass_if_false("NULL segment pointer to txn",
                     nr_segment_discard(&txn.segment_root), "expected false");

  txn.segment_root->txn = &txn;
  txn.segment_count = 1;

  /* Build a mock tree of segments
   *
   *          A
   *          |
   *          B
   *         / \
   *        C   D
   */

  A = txn.segment_root;
  B = nr_segment_start(&txn, A, NULL);
  C = nr_segment_start(&txn, B, NULL);
  nr_segment_end(C);
  D = nr_segment_start(&txn, B, NULL);
  nr_segment_end(D);
  nr_segment_end(B);

  tlib_pass_if_size_t_equal("segment count", 4, txn.segment_count);

  /* Allocate some fields, so we know those are getting destroyed. */
  A->id = nr_strdup("A");
  B->id = nr_strdup("B");
  C->id = nr_strdup("C");
  D->id = nr_strdup("D");

  /* Deleting the root node of a tree must not work.
   *
   * delete -> A              A
   *           |              |
   *           B      =>      B
   *          / \            / \
   *         C   D          C   D
   */
  tlib_pass_if_false("Don't discard root node", nr_segment_discard(&A),
                     "expected false");
  tlib_pass_if_size_t_equal("segment count", 4, txn.segment_count);

  /* Deleting B must reparent C and D.
   *
   *           A
   *           |              A
   * delete -> B      =>     / \
   *          / \           C   D
   *         C   D
   */
  tlib_pass_if_true("delete node with kids", nr_segment_discard(&B),
                    "expected true");
  tlib_pass_if_size_t_equal("A has two children", 2,
                            nr_segment_children_size(&A->children));
  tlib_pass_if_ptr_equal("A is C's parent", C->parent, A);
  tlib_pass_if_ptr_equal("A is D's parent", D->parent, A);
  tlib_pass_if_ptr_equal("B is NULL", B, NULL);
  tlib_pass_if_size_t_equal("segment count", 3, txn.segment_count);

  /* Deleting  a leaf node.
   *
   *              A           A
   *             / \    =>    |
   * delete ->  C   D         D
   */
  tlib_pass_if_true("delete leaf node", nr_segment_discard(&C),
                    "expected true");
  tlib_pass_if_size_t_equal("A has one child", 1,
                            nr_segment_children_size(&A->children));
  tlib_pass_if_ptr_equal("A is D's parent", D->parent, A);
  tlib_pass_if_ptr_equal("C is NULL", B, NULL);
  tlib_pass_if_size_t_equal("segment count", 2, txn.segment_count);

  nr_segment_destroy(A);
  nr_slab_destroy(&txn.segment_slab);
}

static void test_segment_tree_to_heap(void) {
  nr_minmax_heap_t* heap;
  nr_segment_tree_to_heap_metadata_t heaps
      = {.trace_heap = NULL, .span_heap = NULL};

  nr_segment_t* root = nr_zalloc(sizeof(nr_segment_t));
  nr_segment_t* mini = nr_zalloc(sizeof(nr_segment_t));
  nr_segment_t* midi = nr_zalloc(sizeof(nr_segment_t));
  nr_segment_t* maxi = nr_zalloc(sizeof(nr_segment_t));

  root->start_time = 100;
  root->stop_time = 10000;

  mini->start_time = 100;
  mini->stop_time = 200;

  midi->start_time = 100;
  midi->stop_time = 300;

  maxi->start_time = 100;
  maxi->stop_time = 400;

  /* Build a mock tree of segments */
  nr_segment_children_init(&root->children);
  nr_segment_add_child(root, mini);
  nr_segment_add_child(root, midi);
  nr_segment_add_child(root, maxi);

  /*
   * Test : Normal operation.  Insert multiple segments directly into a
   * two-slot heap and affirm that the expected pair are the min and
   * max members of the heap.  It's an indirect way of testing that
   * the supplied comparator is working, but I want to affirm all the
   * right pieces are in place for a heap of segments.
   */
  heap = nr_segment_heap_create(2, nr_segment_wrapped_duration_comparator);

  nr_minmax_heap_insert(heap, (void*)mini);
  nr_minmax_heap_insert(heap, (void*)midi);
  nr_minmax_heap_insert(heap, (void*)maxi);
  nr_minmax_heap_insert(heap, (void*)root);

  tlib_pass_if_ptr_equal(
      "After inserting the maxi segment, it must be the min value in the "
      "heap",
      nr_minmax_heap_peek_min(heap), maxi);

  tlib_pass_if_ptr_equal(
      "After inserting the root segment, it must be the max value in the "
      "heap",
      nr_minmax_heap_peek_max(heap), root);
  nr_minmax_heap_destroy(&heap);

  /*
   * Bad input
   */

  // Test : No heaps should not blow up
  nr_segment_tree_to_heap(root, NULL);

  // Test : No root should not blow up
  nr_segment_tree_to_heap(NULL, &heaps);

  /*
   * Test : Normal operation.  Iterate over a tree and make a heap.
   */
  heaps.trace_heap
      = nr_segment_heap_create(2, nr_segment_wrapped_duration_comparator);
  heaps.span_heap
      = nr_segment_heap_create(2, nr_segment_wrapped_duration_comparator);
  nr_segment_tree_to_heap(root, &heaps);

  tlib_pass_if_ptr_equal(
      "After inserting the maxi segment, it must be the min value in the "
      "trace heap",
      nr_minmax_heap_peek_min(heaps.trace_heap), maxi);
  tlib_pass_if_ptr_equal(
      "After inserting the maxi segment, it must be the min value in the "
      "span heap",
      nr_minmax_heap_peek_min(heaps.span_heap), maxi);

  tlib_pass_if_ptr_equal(
      "After inserting the root segment, it must be the max value in the "
      "trace heap",
      nr_minmax_heap_peek_max(heaps.trace_heap), root);
  tlib_pass_if_ptr_equal(
      "After inserting the root segment, it must be the max value in the "
      "span heap",
      nr_minmax_heap_peek_max(heaps.span_heap), root);

  /* Clean up */
  nr_minmax_heap_destroy(&heap);
  nr_minmax_heap_destroy(&heaps.trace_heap);
  nr_minmax_heap_destroy(&heaps.span_heap);
  nr_segment_destroy(root);
  nr_free(root);
  nr_free(mini);
  nr_free(midi);
  nr_free(maxi);
}

static void test_segment_set(void) {
  nr_set_t* set;

  nr_segment_t* root = nr_zalloc(sizeof(nr_segment_t));
  nr_segment_t* mini = nr_zalloc(sizeof(nr_segment_t));

  /* Build a mock tree of segments */
  nr_segment_children_init(&root->children);
  nr_segment_add_child(root, mini);

  /* Prepare a set for population */
  set = nr_set_create();

  nr_set_insert(set, root);
  nr_set_insert(set, mini);

  tlib_pass_if_true("The root segment is a member of the set",
                    nr_set_contains(set, root), "Expected true");
  tlib_pass_if_true("The mini segment is a member of the set",
                    nr_set_contains(set, mini), "Expected true");

  nr_set_destroy(&set);
  nr_segment_destroy(root);
  nr_free(root);
  nr_free(mini);
}

static void test_segment_heap_to_set(void) {
  nr_set_t* set;
  nr_segment_tree_to_heap_metadata_t heaps
      = {.trace_heap = NULL, .span_heap = NULL};

  nr_segment_t* root = nr_zalloc(sizeof(nr_segment_t));
  nr_segment_t* mini = nr_zalloc(sizeof(nr_segment_t));
  nr_segment_t* midi = nr_zalloc(sizeof(nr_segment_t));
  nr_segment_t* maxi = nr_zalloc(sizeof(nr_segment_t));

  root->start_time = 100;
  root->stop_time = 10000;

  mini->start_time = 100;
  mini->stop_time = 200;

  midi->start_time = 100;
  midi->stop_time = 300;

  maxi->start_time = 100;
  maxi->stop_time = 400;

  /* Build a mock tree of segments */
  nr_segment_children_init(&root->children);
  nr_segment_add_child(root, mini);
  nr_segment_add_child(root, midi);
  nr_segment_add_child(root, maxi);

  /* Build a heap */
  heaps.trace_heap
      = nr_segment_heap_create(4, nr_segment_wrapped_duration_comparator);
  nr_segment_tree_to_heap(root, &heaps);

  /* Prepare a set for population */
  set = nr_set_create();

  /* Test : Bad parameters */
  nr_segment_heap_to_set(heaps.trace_heap, NULL);
  nr_segment_heap_to_set(NULL, set);
  tlib_pass_if_true("Converting a NULL heap to a set must yield an empty set",
                    nr_set_size(set) == 0, "Expected true");
  nr_set_destroy(&set);

  /* Test : Normal operation. */
  set = nr_set_create();
  nr_segment_heap_to_set(heaps.trace_heap, set);

  tlib_pass_if_not_null(
      "Converting a well-formed heap to a set must yield a non-empty set", set);

  /* Affirm membership */
  tlib_pass_if_true("The longest segment is a member of the set",
                    nr_set_contains(set, root), "Expected true");
  tlib_pass_if_true("The second-longest segment is a member of the set",
                    nr_set_contains(set, maxi), "Expected true");
  tlib_pass_if_true("The third-longest segment is a member of the set",
                    nr_set_contains(set, midi), "Expected true");
  tlib_pass_if_true("The shortest segment is a member of the set",
                    nr_set_contains(set, mini), "Expected true");
  nr_set_destroy(&set);

  /* Clean up */
  nr_minmax_heap_destroy(&heaps.trace_heap);
  nr_segment_destroy(root);
  nr_free(root);
  nr_free(mini);
  nr_free(midi);
  nr_free(maxi);
}

static void test_segment_set_parent_cycle(void) {
  // clang-format off
  nr_segment_t root = {.start_time = 1000, .stop_time = 10000};
  nr_segment_t A = {.start_time = 2000, .stop_time = 7000};
  nr_segment_t B = {.start_time = 3000, .stop_time = 6000};
  nr_segment_t C = {.start_time = 4000, .stop_time = 5000};
  nr_segment_t D = {.start_time = 5000, .stop_time = 7000};
  nr_segment_t E = {.start_time = 6000, .stop_time = 8000};

  /*
   * The mock tree looks like this:
   *
   *              ---------root--------
   *               /                  \
   *           ---A---              ---D---
   *          /       \             /
   *      ---B---   ---C---     ---E---
   *
   */
  // clang-format on

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&A.children);
  nr_segment_children_init(&B.children);
  nr_segment_children_init(&C.children);
  nr_segment_children_init(&D.children);
  nr_segment_children_init(&E.children);

  tlib_pass_if_true("root -> A", nr_segment_set_parent(&A, &root),
                    "expected true");
  tlib_pass_if_true("A -> B", nr_segment_set_parent(&B, &A), "expected true");
  tlib_pass_if_true("A -> C", nr_segment_set_parent(&C, &A), "expected true");
  tlib_pass_if_true("root -> D", nr_segment_set_parent(&D, &root),
                    "expected true");
  tlib_pass_if_true("D -> E", nr_segment_set_parent(&E, &D), "expected true");

  tlib_pass_if_false("Cycle must not succeed E->Root",
                     nr_segment_set_parent(&root, &E), "expected false");
  tlib_pass_if_null("Root should not have a parent", root.parent);

  tlib_pass_if_false("Cycle must not succeed B->A",
                     nr_segment_set_parent(&A, &B), "expected false");
  tlib_pass_if_ptr_equal("A should still be B's Parent", B.parent, &A);

  tlib_pass_if_false("Cycle must not succeed C->A",
                     nr_segment_set_parent(&A, &C), "expected false");
  tlib_pass_if_ptr_equal("A should still be C's parent", C.parent, &A);

  tlib_pass_if_false("Cycle must not succeed C->Root",
                     nr_segment_set_parent(&root, &C), "expected false");
  tlib_pass_if_null("Root should not have a parent", root.parent);

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&A.children);
  nr_segment_children_deinit(&B.children);
  nr_segment_children_deinit(&C.children);
  nr_segment_children_deinit(&D.children);
  nr_segment_children_deinit(&E.children);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&C);
  nr_segment_destroy_fields(&D);
  nr_segment_destroy_fields(&E);
}

static void test_segment_no_recording(void) {
  nrtxn_t txn = {0};
  nr_segment_t* seg;

  txn.status.recording = 0;

  /* Recording is off, no segment is started. */
  seg = nr_segment_start(&txn, NULL, NULL);
  tlib_pass_if_null("recording off, no segment", seg);

  /* Test that other segment functions don't crash at NULL inputs. */
  nr_segment_destroy_fields(seg);
  nr_segment_set_custom(seg);
  nr_segment_set_datastore(seg, &(nr_segment_datastore_t){0});
  nr_segment_set_external(seg, &(nr_segment_external_t){0});
  nr_segment_add_child(seg, &(nr_segment_t){0});
  nr_segment_add_metric(seg, "metric", false);
  nr_segment_set_name(seg, "name");
  nr_segment_set_parent(seg, &(nr_segment_t){0});
  nr_segment_set_timing(seg, 1, 2);
  nr_segment_end(seg);
  nr_segment_destroy(seg);
}

static void test_segment_span_comparator_null(void) {
  nr_segment_t seg = {0};
  nr_vector_t* segments = nr_vector_create(2, NULL, NULL);

  /*
   * Verify the comparator doesn't crash on NULL elements.
   */
  nr_vector_push_back(segments, &seg);
  nr_vector_push_back(segments, NULL);

  nr_vector_sort(segments, nr_segment_wrapped_span_priority_comparator, NULL);

  tlib_pass_if_ptr_equal("valid segment after NULL", nr_vector_get(segments, 0),
                         NULL);
  tlib_pass_if_ptr_equal("valid segment after NULL", nr_vector_get(segments, 1),
                         &seg);

  nr_vector_destroy(&segments);
}

static void test_segment_span_comparator(void) {
  nr_segment_t root = {.parent = NULL, .priority = NR_SEGMENT_PRIORITY_ROOT};
  nr_segment_t external = {.parent = &root,
                           .start_time = 0,
                           .stop_time = 10,
                           .type = NR_SEGMENT_EXTERNAL};
  nr_segment_t external_dt = {.parent = &root,
                              .start_time = 0,
                              .stop_time = 10,
                              .type = NR_SEGMENT_EXTERNAL,
                              .priority = NR_SEGMENT_PRIORITY_DT,
                              .id = "id1"};
  nr_segment_t external_dt_long = {.parent = &root,
                                   .start_time = 10,
                                   .stop_time = 30,
                                   .type = NR_SEGMENT_EXTERNAL,
                                   .priority = NR_SEGMENT_PRIORITY_DT,
                                   .id = "id2"};
  nr_segment_t external_dt_log
      = {.parent = &root,
         .start_time = 0,
         .stop_time = 10,
         .type = NR_SEGMENT_EXTERNAL,
         .priority = NR_SEGMENT_PRIORITY_DT | NR_SEGMENT_PRIORITY_LOG,
         .id = "id3"};
  nr_segment_t custom = {.parent = &root, .start_time = 0, .stop_time = 20};
  nr_segment_t custom_long
      = {.parent = &root, .start_time = 0, .stop_time = 1000};
  nr_segment_t custom_log = {.parent = &root,
                             .start_time = 0,
                             .stop_time = 10,
                             .id = "id4",
                             .priority = NR_SEGMENT_PRIORITY_LOG};
  nr_segment_t custom_log_long = {.parent = &root,
                                  .start_time = 10,
                                  .stop_time = 30,
                                  .id = "id5",
                                  .priority = NR_SEGMENT_PRIORITY_LOG};
  nr_vector_t* segments = nr_vector_create(12, NULL, NULL);

  /*
   * The comparator function is tested by using it to sort a vector of
   * segments. In this way, all necessary test cases are covered.
   *
   * The comparator first compares a segment's priority, which is a bit
   * field with bits set according to NR_SEGEMENT_PRIORITY_* flags. The
   * priority with the higher numerical value is considered higher.
   *
   * If the priorities of two segments are the same, the comparator
   * compares the segments' duration. The longer duration is considered
   * higher.
   *
   * The table below shows the final ordering and the respective values
   * that are considered by the comparator function.
   *
   * Position | Segment          | Priority            | Duration
   * ---------+------------------+---------------------+----------
   * 8        | root             | 0b10000000000000000 | 10
   * 7        | external_dt_log  | 0b01100000000000000 | 10
   * 6        | external_dt_long | 0b01000000000000000 | 20
   * 5        | external_dt      | 0b01000000000000000 | 10
   * 4        | custom_log_long  | 0b00100000000000000 | 20
   * 3        | custom_log       | 0b00100000000000000 | 10
   * 2        | custom_long      | 0b00000000000000000 | 1000
   * 1        | custom           | 0b00000000000000000 | 20
   * 0        | external         | 0b00000000000000000 | 10
   */

  nr_vector_push_back(segments, &root);
  nr_vector_push_back(segments, &external);
  nr_vector_push_back(segments, &external_dt);
  nr_vector_push_back(segments, &external_dt_long);
  nr_vector_push_back(segments, &external_dt_log);
  nr_vector_push_back(segments, &custom);
  nr_vector_push_back(segments, &custom_long);
  nr_vector_push_back(segments, &custom_log);
  nr_vector_push_back(segments, &custom_log_long);

  nr_vector_sort(segments, nr_segment_wrapped_span_priority_comparator, NULL);

  tlib_pass_if_ptr_equal("1. root segment", nr_vector_get(segments, 8), &root);
  tlib_pass_if_ptr_equal("2. external DT and logs", nr_vector_get(segments, 7),
                         &external_dt_log);
  tlib_pass_if_ptr_equal("3. external DT long", nr_vector_get(segments, 6),
                         &external_dt_long);
  tlib_pass_if_ptr_equal("4. external DT", nr_vector_get(segments, 5),
                         &external_dt);
  tlib_pass_if_ptr_equal("5. custom long and logs", nr_vector_get(segments, 4),
                         &custom_log_long);
  tlib_pass_if_ptr_equal("6. custom log", nr_vector_get(segments, 3),
                         &custom_log);
  tlib_pass_if_ptr_equal("7. custom long", nr_vector_get(segments, 2),
                         &custom_long);
  tlib_pass_if_ptr_equal("8. custom", nr_vector_get(segments, 1), &custom);
  tlib_pass_if_ptr_equal("9. external", nr_vector_get(segments, 0), &external);

  nr_vector_destroy(&segments);
}

static void test_segment_set_priority_flag(void) {
  nr_segment_t no_priority = {0};
  nr_segment_t root = {0};
  nr_segment_t dt = {0};
  nr_segment_t log = {0};
  nr_segment_t dt_log = {0};

  nr_vector_t* segments = nr_vector_create(5, NULL, NULL);

  /*
   * Don't blow up when passed a NULL segment.
   */
  nr_segment_set_priority_flag(NULL, NR_SEGMENT_PRIORITY_ROOT);

  nr_segment_set_priority_flag(&root, NR_SEGMENT_PRIORITY_ROOT);
  nr_segment_set_priority_flag(&dt, NR_SEGMENT_PRIORITY_DT);
  nr_segment_set_priority_flag(&log, NR_SEGMENT_PRIORITY_LOG);
  nr_segment_set_priority_flag(
      &dt_log, NR_SEGMENT_PRIORITY_DT | NR_SEGMENT_PRIORITY_LOG);

  /*
   * The impact of different priority flags is tested by sorting a
   * vector of segments according to their priority values. In this way,
   * all necessary test cases are covered.
   *
   * The helper function test_segment_priority_comparator is used to
   * sort segments according to the numeric value of the priority field.
   * This should test the correct relation of the priority flags to each
   * other.
   *
   * The table below shows the segments with their priority flags and
   * the resulting values of the priority field.
   *
   * Position | Segment     | Priority            | NR_SEGMENT_PRIORITY_*
   * ---------+-------------+---------------------+-----------------------
   * 4        | root        | 0b10000000000000000 | ROOT
   * 3        | dt_log      | 0b01100000000000000 | DT | LOG
   * 2        | dt          | 0b01000000000000000 | DT
   * 1        | log         | 0b00100000000000000 | LOG
   * 0        | no priority | 0b00000000000000000 |
   */

  nr_vector_push_back(segments, &no_priority);
  nr_vector_push_back(segments, &root);
  nr_vector_push_back(segments, &dt);
  nr_vector_push_back(segments, &log);
  nr_vector_push_back(segments, &dt_log);

  nr_vector_sort(segments, test_segment_priority_comparator, NULL);

  tlib_pass_if_ptr_equal("1. root", nr_vector_get(segments, 4), &root);
  tlib_pass_if_ptr_equal("2. dt with log", nr_vector_get(segments, 3), &dt_log);
  tlib_pass_if_ptr_equal("3. dt", nr_vector_get(segments, 2), &dt);
  tlib_pass_if_ptr_equal("4. log", nr_vector_get(segments, 1), &log);
  tlib_pass_if_ptr_equal("5. no priority", nr_vector_get(segments, 0),
                         &no_priority);

  nr_vector_destroy(&segments);
}

static void test_segment_init_deinit(void) {
  nrtxn_t txn = {0};
  nr_segment_t* root;
  nr_segment_t* s;

  txn.status.recording = 1;
  txn.segment_slab = nr_slab_create(sizeof(nr_segment_t), 0);

  txn.segment_root = nr_slab_next(txn.segment_slab);
  txn.segment_root->txn = &txn;
  txn.segment_count = 1;

  root = txn.segment_root;

  /* start a new child segment */
  s = nr_segment_start(&txn, root, NULL);
  nr_segment_end(s);
  tlib_pass_if_size_t_equal("one child", 1,
                            nr_segment_children_size(&root->children));

  /* de-initialize the child segment */
  nr_segment_deinit(s);
  tlib_pass_if_size_t_equal("no child", 0,
                            nr_segment_children_size(&root->children));

  /* re-initialize the child segment */
  nr_segment_init(s, &txn, root, NULL);
  tlib_pass_if_true("stop time must be set to 0 on initialization",
                    0 == s->stop_time, "Expected true");
  tlib_pass_if_size_t_equal("one child", 1,
                            nr_segment_children_size(&root->children));

  /* reparent child segments on de-initialization */
  nr_segment_end(nr_segment_start(&txn, s, NULL));
  nr_segment_end(nr_segment_start(&txn, s, NULL));
  nr_segment_end(nr_segment_start(&txn, s, NULL));
  nr_segment_end(s);
  tlib_pass_if_size_t_equal("3 children", 3,
                            nr_segment_children_size(&s->children));
  nr_segment_deinit(s);
  tlib_pass_if_size_t_equal("3 children", 3,
                            nr_segment_children_size(&root->children));

  nr_segment_destroy(s);
  nr_segment_destroy(root);
  nr_slab_destroy(&txn.segment_slab);
}

static void test_segment_ensure_id(void) {
  nrapp_t app = {
      .state = NR_APP_OK,
      .limits = {
        .analytics_events = NR_MAX_ANALYTIC_EVENTS,
        .span_events = NR_MAX_SPAN_EVENTS,
      },
  };
  nrtxnopt_t opts;
  nr_segment_t* segment;
  char* segment_id;
  nrtxn_t* txn;

  /* start txn and segment */
  nr_memset(&opts, 0, sizeof(opts));
  opts.distributed_tracing_enabled = 1;
  opts.span_events_enabled = 1;
  txn = nr_txn_begin(&app, &opts, NULL);
  segment = nr_segment_start(txn, txn->segment_root, NULL);
  nr_distributed_trace_set_sampled(txn->distributed_trace, true);

  /*
   * Test : Bad parameters
   */
  tlib_pass_if_null("null txn and segment", nr_segment_ensure_id(NULL, NULL));
  tlib_pass_if_null("null txn", nr_segment_ensure_id(segment, NULL));
  tlib_pass_if_null("null segment", nr_segment_ensure_id(NULL, txn));

  /*
   * Test : segment id is created
   */
  segment_id = nr_segment_ensure_id(segment, txn);
  tlib_fail_if_null("segment id is created", segment_id);

  /*
   * Test : correct id is returned for the segment
   */
  tlib_pass_if_str_equal("correct id is returned for the segment", segment_id,
                         nr_segment_ensure_id(segment, txn));
  nr_free(segment->id);

  /*
   * Test : NULL segment id when DT is disabled
   */
  txn->options.distributed_tracing_enabled = 0;
  tlib_pass_if_null("no segment id when DT is disabled",
                    nr_segment_ensure_id(segment, txn));
  txn->options.distributed_tracing_enabled = 1;

  /*
   * Test : NULL segment id when span events are disabled
   */
  txn->options.span_events_enabled = 0;
  tlib_pass_if_null("no segment id when span events are disabled",
                    nr_segment_ensure_id(segment, txn));
  txn->options.span_events_enabled = 1;

  /*
   * Test : NULL segment id when DT is not sampled
   */
  nr_distributed_trace_set_sampled(txn->distributed_trace, false);
  tlib_pass_if_null("no segment id when DT is sampled",
                    nr_segment_ensure_id(segment, txn));
  nr_distributed_trace_set_sampled(txn->distributed_trace, true);

  nr_segment_destroy(segment);
  nr_txn_destroy(&txn);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_segment_new_txn_with_segment_root();
  test_segment_start();
  test_segment_start_async();
  test_set_name();
  test_add_child();
  test_add_metric();
  test_set_parent_to_same();
  test_set_null_parent();
  test_set_non_null_parent();
  test_set_parent_different_txn();
  test_set_timing();
  test_end_segment();
  test_end_segment_async();
  test_segment_iterate_bachelor();
  test_segment_iterate_nulls();
  test_segment_iterate();
  test_segment_iterate_cycle_one();
  test_segment_iterate_cycle_two();
  test_segment_iterate_with_amputation();
  test_segment_iterate_with_post_callback();
  test_segment_destroy();
  test_segment_destroy_tree();
  test_segment_discard();
  test_segment_tree_to_heap();
  test_segment_set();
  test_segment_heap_to_set();
  test_segment_set_parent_cycle();
  test_segment_no_recording();
  test_segment_span_comparator();
  test_segment_span_comparator_null();
  test_segment_set_priority_flag();
  test_segment_init_deinit();
  test_segment_ensure_id();
}
