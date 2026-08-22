/* Tests that impSurface and impCubeVolume can be built with no GL context.
 *
 * This file exists to hold open a door rather than to characterize behaviour.
 * impSurface's constructor used to call glGenBuffers, which needs a context a
 * constructor has no way to require, so `new impSurface` exited 139 in any
 * test binary. impCubeVolume's constructor does `surface = new impSurface`, so
 * the whole marching-cubes pipeline -- the largest testable surface in this
 * submodule and the code the Metal port is about to rewrite -- inherited that
 * and sat in triage bucket B, unreachable. ss-aio deferred the two
 * glGenBuffers calls to the first draw(); these cases are what stops them
 * being moved back.
 *
 * They fail loudly if that happens: reverting the fix makes the constructor
 * segfault, which takes the binary down with exit 139 and no coverage report
 * at all. That is a blunter signal than a CHECK failure, but it is not a
 * silent one, and it is the only signal available -- the thing under test is
 * whether a constructor returns.
 *
 * What is deliberately NOT here:
 *
 *   impSurface's own mutators (addVertex, addIndex, addTriStripLength) now
 *   run, but every byte they write goes into private members with no accessor
 *   and no observer short of draw(), which still needs a context. A test that
 *   called them and asserted nothing would be one more undiscriminating path
 *   of the kind this project already has twenty of, so they are left uncovered
 *   and honest.
 *
 *   The geometry itself -- makeSurface, polygonize, crawl_sort and the rest.
 *   That is ss-or3, and it is a much larger job than this one: it needs a
 *   field function, an expected mesh and a tolerance argued for in writing.
 *   This file only establishes that ss-or3 is now possible.
 *
 *   The trivial accessors (getSurface, setSurfaceValue and friends), which
 *   triage classifies C. They are exercised incidentally below where a case
 *   needs them to observe something; none is tested for its own sake.
 */

#include "harness.h"

#include "Implicit/impCubeVolume.h"
#include "Implicit/impSurface.h"

/* The bare fact ss-aio turned from false to true. Before the fix this line
 * did not return. */
TEST(impsurface_constructs_without_a_gl_context)
{
    impSurface *surface = new impSurface();
    CHECK(surface != 0);
    delete surface;
}

/* Stack construction as well as heap: the destructor runs here at scope exit
 * rather than on an explicit delete, and it is the destructor that holds the
 * other half of the fix -- it must not call glDeleteBuffers on a surface whose
 * buffer names were never generated. */
TEST(impsurface_destructs_without_a_gl_context)
{
    impSurface surface;
    CHECK(true);  /* reaching the closing brace is the assertion */
}

/* impCubeVolume was never the thing calling GL. It was blocked transitively,
 * through the impSurface its constructor allocates, which is why triage read
 * its functions as testable when they were not -- the classifier reads each
 * function's own source text and none of them names a GL call.
 *
 * getSurface() is the observation rather than the subject: a non-null surface
 * proves the constructor ran past `surface = new impSurface` to completion,
 * which a bare "it did not crash" would not distinguish from a constructor
 * that had stopped allocating one. */
TEST(impcubevolume_constructs_and_owns_a_surface)
{
    impCubeVolume volume;
    CHECK(volume.getSurface() != 0);
}

/* init() allocates the cube grid and builds the crawl tables, and at 10
 * uncovered regions it is the largest single function this fix unblocks.
 * Calling it here is not a characterization of what it computes -- that is
 * ss-or3 -- but it does prove the allocation path survives outside a
 * renderer, which is what ss-or3 will be standing on.
 *
 * 4x4x4 rather than the 40-odd cubes Helios.cpp uses: large enough that
 * w_1xh_1xl_1 is not degenerate, small enough to stay instant. */
TEST(impcubevolume_init_allocates_without_a_gl_context)
{
    impCubeVolume volume;
    volume.init(4, 4, 4, 0.25f);
    CHECK(volume.getSurface() != 0);
}

/* The surface value is the one piece of impCubeVolume's state that is both
 * settable and readable, so it is the only round-trip available to show the
 * object is functional after construction rather than merely allocated.
 * 0.42f is arbitrary but not 0.0f or 1.0f, either of which could be matched by
 * a stub returning a default. */
TEST(impcubevolume_state_survives_construction)
{
    impCubeVolume volume;
    volume.setSurfaceValue(0.42f);
    CHECK_NEAR(volume.getSurfaceValue(), 0.42f, 1e-6);
}
