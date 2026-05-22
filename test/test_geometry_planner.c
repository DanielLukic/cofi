#include <stdio.h>
#include <stdbool.h>
#include "../src/geometry_planner.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(desc, cond) do { \
    if (cond) { printf("PASS: %s\n", (desc)); tests_passed++; } \
    else       { printf("FAIL: %s\n", (desc)); tests_failed++; } \
} while (0)

#define ASSERT_FALSE(desc, cond) ASSERT_TRUE((desc), !(cond))

// --- helper: a baseline "normal window" state ---
static GeometryState make_state(int x, int y, int w, int h, int desk,
                                bool mv, bool mh, bool fs) {
    GeometryState s = {0};
    s.x = x; s.y = y; s.width = w; s.height = h;
    s.desktop = desk;
    s.maximized_vert = mv;
    s.maximized_horz = mh;
    s.fullscreen = fs;
    return s;
}

// ------------------------------------------------------------------ //
// All-no-op: current == target in every dimension                     //
// ------------------------------------------------------------------ //
static void test_no_op_when_already_in_target(void) {
    printf("\n--- no-op: current == target ---\n");

    GeometryState s = make_state(10, 20, 800, 600, 1, false, false, false);
    GeometryRestorePlan p = geometry_restore_plan(&s, &s);

    ASSERT_FALSE("no-op: plan.any is false",          p.any);
    ASSERT_FALSE("no-op: unset_fullscreen",           p.unset_fullscreen);
    ASSERT_FALSE("no-op: unset_max_vert",             p.unset_max_vert);
    ASSERT_FALSE("no-op: unset_max_horz",             p.unset_max_horz);
    ASSERT_FALSE("no-op: do_move",                    p.do_move);
    ASSERT_FALSE("no-op: do_desktop",                 p.do_desktop);
    ASSERT_FALSE("no-op: set_fullscreen",             p.set_fullscreen);
    ASSERT_FALSE("no-op: set_max_vert",               p.set_max_vert);
    ASSERT_FALSE("no-op: set_max_horz",               p.set_max_horz);
}

static void test_no_op_both_maximized_same_geom(void) {
    printf("\n--- no-op: both already fully maximized, same desktop ---\n");

    GeometryState s = make_state(0, 0, 1920, 1080, 0, true, true, false);
    GeometryRestorePlan p = geometry_restore_plan(&s, &s);

    ASSERT_FALSE("fully-max no-op: plan.any is false", p.any);
    ASSERT_FALSE("fully-max no-op: no set_max_vert",   p.set_max_vert);
    ASSERT_FALSE("fully-max no-op: no set_max_horz",   p.set_max_horz);
    ASSERT_FALSE("fully-max no-op: no do_move",        p.do_move);
}

// ------------------------------------------------------------------ //
// Maximize state transitions                                          //
// ------------------------------------------------------------------ //
static void test_maximized_to_normal(void) {
    printf("\n--- max → normal ---\n");

    GeometryState cur  = make_state(0, 0, 1920, 1080, 0, true,  true,  false);
    GeometryState want = make_state(100, 50, 800, 600,  0, false, false, false);
    GeometryRestorePlan p = geometry_restore_plan(&cur, &want);

    ASSERT_TRUE ("unset_max_vert",      p.unset_max_vert);
    ASSERT_TRUE ("unset_max_horz",      p.unset_max_horz);
    ASSERT_FALSE("no unset_fullscreen", p.unset_fullscreen);
    ASSERT_TRUE ("do_move",             p.do_move);
    ASSERT_FALSE("no do_desktop",       p.do_desktop);
    ASSERT_FALSE("no set_max_vert",     p.set_max_vert);
    ASSERT_FALSE("no set_max_horz",     p.set_max_horz);
    ASSERT_TRUE ("plan.any",            p.any);
}

static void test_normal_to_both_maximized(void) {
    printf("\n--- normal → both max ---\n");

    GeometryState cur  = make_state(100, 50, 800, 600, 0, false, false, false);
    GeometryState want = make_state(  0,  0, 1920, 1080, 0, true,  true,  false);
    GeometryRestorePlan p = geometry_restore_plan(&cur, &want);

    ASSERT_FALSE("no unset", p.unset_max_vert);
    ASSERT_FALSE("no unset", p.unset_max_horz);
    // target is fully maximized → skip move even though geom differs
    ASSERT_FALSE("no do_move (target fully max)", p.do_move);
    ASSERT_TRUE ("set_max_vert",  p.set_max_vert);
    ASSERT_TRUE ("set_max_horz",  p.set_max_horz);
    ASSERT_TRUE ("plan.any",      p.any);
}

static void test_partial_max_swap(void) {
    printf("\n--- vert-max → horz-max only ---\n");

    // Currently max_vert; want max_horz (not both)
    GeometryState cur  = make_state(0, 0, 800, 1080, 0, true,  false, false);
    GeometryState want = make_state(0, 0, 1920, 600, 0, false, true,  false);
    GeometryRestorePlan p = geometry_restore_plan(&cur, &want);

    ASSERT_TRUE ("unset_max_vert",  p.unset_max_vert);
    ASSERT_FALSE("no unset_horz",   p.unset_max_horz);
    ASSERT_TRUE ("set_max_horz",    p.set_max_horz);
    ASSERT_FALSE("no set_max_vert", p.set_max_vert);
    // target is not BOTH maximized, so move is allowed
    ASSERT_TRUE ("do_move (target not both-max)", p.do_move);
    ASSERT_TRUE ("plan.any", p.any);
}

// ------------------------------------------------------------------ //
// Fullscreen transitions                                              //
// ------------------------------------------------------------------ //
static void test_fullscreen_to_normal(void) {
    printf("\n--- fullscreen → normal ---\n");

    GeometryState cur  = make_state(0, 0, 1920, 1080, 0, false, false, true);
    GeometryState want = make_state(100, 50, 800, 600, 0, false, false, false);
    GeometryRestorePlan p = geometry_restore_plan(&cur, &want);

    ASSERT_TRUE ("unset_fullscreen", p.unset_fullscreen);
    ASSERT_TRUE ("do_move",          p.do_move);
    ASSERT_FALSE("no set_fullscreen", p.set_fullscreen);
    ASSERT_TRUE ("plan.any",          p.any);
}

static void test_normal_to_fullscreen_skips_move(void) {
    printf("\n--- normal → fullscreen (move skipped) ---\n");

    GeometryState cur  = make_state(100, 50, 800, 600, 0, false, false, false);
    GeometryState want = make_state(  0,  0, 1920, 1080, 0, false, false, true);
    GeometryRestorePlan p = geometry_restore_plan(&cur, &want);

    ASSERT_TRUE ("set_fullscreen",            p.set_fullscreen);
    ASSERT_FALSE("do_move skipped (target fullscreen)", p.do_move);
    ASSERT_FALSE("no unset_fullscreen",       p.unset_fullscreen);
    ASSERT_TRUE ("plan.any",                  p.any);
}

static void test_already_fullscreen_stays_fullscreen(void) {
    printf("\n--- fullscreen stays fullscreen, only geom changes ignored ---\n");

    GeometryState cur  = make_state(0, 0, 1920, 1080, 0, false, false, true);
    GeometryState want = make_state(0, 0, 1920, 1080, 0, false, false, true);
    GeometryRestorePlan p = geometry_restore_plan(&cur, &want);

    ASSERT_FALSE("no unset",  p.unset_fullscreen);
    ASSERT_FALSE("no set",    p.set_fullscreen);
    ASSERT_FALSE("no move",   p.do_move);
    ASSERT_FALSE("plan.any",  p.any);
}

// ------------------------------------------------------------------ //
// Desktop transitions                                                 //
// ------------------------------------------------------------------ //
static void test_same_desktop_no_op(void) {
    printf("\n--- desktop same → no do_desktop ---\n");

    GeometryState cur  = make_state(10, 10, 800, 600, 2, false, false, false);
    GeometryState want = make_state(10, 10, 800, 600, 2, false, false, false);
    GeometryRestorePlan p = geometry_restore_plan(&cur, &want);

    ASSERT_FALSE("do_desktop is false", p.do_desktop);
    ASSERT_FALSE("plan.any is false",   p.any);
}

static void test_different_desktop_triggers_move(void) {
    printf("\n--- desktop differs → do_desktop ---\n");

    GeometryState cur  = make_state(10, 10, 800, 600, 0, false, false, false);
    GeometryState want = make_state(10, 10, 800, 600, 3, false, false, false);
    GeometryRestorePlan p = geometry_restore_plan(&cur, &want);

    ASSERT_TRUE ("do_desktop",  p.do_desktop);
    ASSERT_FALSE("no do_move",  p.do_move);   // geom identical
    ASSERT_TRUE ("plan.any",    p.any);
}

static void test_invalid_desktop_no_move(void) {
    printf("\n--- invalid desktop (-1) → no do_desktop ---\n");

    GeometryState cur  = make_state(10, 10, 800, 600, 0, false, false, false);
    GeometryState want = make_state(10, 10, 800, 600, -1, false, false, false);
    GeometryRestorePlan p = geometry_restore_plan(&cur, &want);

    ASSERT_FALSE("no do_desktop for desktop=-1", p.do_desktop);
    ASSERT_FALSE("plan.any",                     p.any);
}

// ------------------------------------------------------------------ //
// Geometry-only changes                                               //
// ------------------------------------------------------------------ //
static void test_geom_same_no_move(void) {
    printf("\n--- geometry same → no do_move ---\n");

    GeometryState cur  = make_state(10, 20, 800, 600, 0, false, false, false);
    GeometryState want = make_state(10, 20, 800, 600, 0, false, false, false);
    GeometryRestorePlan p = geometry_restore_plan(&cur, &want);

    ASSERT_FALSE("no do_move", p.do_move);
    ASSERT_FALSE("plan.any",   p.any);
}

static void test_geom_differs_triggers_move(void) {
    printf("\n--- geometry differs (not max) → do_move ---\n");

    GeometryState cur  = make_state(10, 20, 800, 600, 0, false, false, false);
    GeometryState want = make_state(50, 60, 900, 700, 0, false, false, false);
    GeometryRestorePlan p = geometry_restore_plan(&cur, &want);

    ASSERT_TRUE ("do_move",   p.do_move);
    ASSERT_FALSE("no desktop", p.do_desktop);
    ASSERT_TRUE ("plan.any",  p.any);
}

static void test_geom_differs_but_target_both_max_no_move(void) {
    printf("\n--- geom differs but target both-max → skip move ---\n");

    GeometryState cur  = make_state(100, 50, 800, 600,  0, false, false, false);
    GeometryState want = make_state(  0,  0, 1920, 1080, 0, true,  true,  false);
    GeometryRestorePlan p = geometry_restore_plan(&cur, &want);

    ASSERT_FALSE("no do_move when target fully maximized", p.do_move);
    ASSERT_TRUE ("set_max_vert",  p.set_max_vert);
    ASSERT_TRUE ("set_max_horz",  p.set_max_horz);
    ASSERT_TRUE ("plan.any",      p.any);
}

// ------------------------------------------------------------------ //
// null safety                                                         //
// ------------------------------------------------------------------ //
static void test_null_inputs_return_empty_plan(void) {
    printf("\n--- null inputs ---\n");

    GeometryState s = make_state(0, 0, 800, 600, 0, false, false, false);
    GeometryRestorePlan p1 = geometry_restore_plan(NULL, &s);
    GeometryRestorePlan p2 = geometry_restore_plan(&s, NULL);
    GeometryRestorePlan p3 = geometry_restore_plan(NULL, NULL);

    ASSERT_FALSE("null current → no ops", p1.any);
    ASSERT_FALSE("null target  → no ops", p2.any);
    ASSERT_FALSE("both null    → no ops", p3.any);
}

int main(void) {
    printf("Geometry planner tests\n");
    printf("======================\n");

    test_no_op_when_already_in_target();
    test_no_op_both_maximized_same_geom();

    test_maximized_to_normal();
    test_normal_to_both_maximized();
    test_partial_max_swap();

    test_fullscreen_to_normal();
    test_normal_to_fullscreen_skips_move();
    test_already_fullscreen_stays_fullscreen();

    test_same_desktop_no_op();
    test_different_desktop_triggers_move();
    test_invalid_desktop_no_move();

    test_geom_same_no_move();
    test_geom_differs_triggers_move();
    test_geom_differs_but_target_both_max_no_move();

    test_null_inputs_return_empty_plan();

    printf("\n=====================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
