/* Tests for impKnot: its constructor's defaults, the coil/twist setters, and
 * the value()/center()/addCrawlPoint() functions built on top of them.
 *
 * Like the other impShape subclasses (see test_imp_primitives.cpp), invtrmat
 * is left uninitialised by impShape's constructor and is only filled in by
 * setMatrix(), so every value() call below is preceded by an explicit
 * identity setMatrix().
 *
 * value() reads invtrmat and calls the *approximate* rsSqrtf/rsCosf/rsSinf/
 * rsAtan2f from rsMath, not the standard library ones -- addCrawlPoint(), by
 * contrast, uses plain cosf/sinf directly. The expected constants below were
 * derived analytically and cross-checked by calling the real rsMath
 * functions offline; the approximation's own error at the chosen points was
 * measured at under 1e-4, so the tolerances here have generous headroom
 * without being wide enough to hide a dropped term or a wrong sign.
 */

#include "harness.h"

#include "Implicit/impKnot.h"

namespace {

const float kTol = 1e-4f;

void makeIdentity(float *m)
{
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

}  // namespace

/* --- impKnot() --------------------------------------------------------------
 *
 * The constructor's initializer-list arithmetic (coilsf, twistsOverCoils,
 * lat_offset) has no direct getter, but its inputs -- radius1, radius2,
 * coils, twists -- are all readable and are what value()/center() actually
 * depend on. */

TEST(knot_constructor_sets_the_documented_defaults)
{
    impKnot k;
    CHECK_NEAR(k.getRadius1(), 1.0f, kTol);
    CHECK_NEAR(k.getRadius2(), 0.5f, kTol);
    CHECK(k.getNumCoils() == 3);
    CHECK(k.getNumTwists() == 2);
}

/* --- impKnot::setNumCoils ----------------------------------------------------
 *
 * "coils must be greater than 1" is what the comment claims, but the clamp
 * (c<1 ? 1 : c) actually only enforces coils >= 1 -- setNumCoils(1) is
 * accepted, not rejected. Characterizing the code as it stands, not the
 * comment's stronger claim. */

TEST(knot_setNumCoils_clamps_zero_and_negative_up_to_one)
{
    impKnot k;

    k.setNumCoils(0);
    CHECK(k.getNumCoils() == 1);

    k.setNumCoils(-5);
    CHECK(k.getNumCoils() == 1);
}

TEST(knot_setNumCoils_accepts_values_of_one_and_above)
{
    impKnot k;

    k.setNumCoils(1);
    CHECK(k.getNumCoils() == 1);

    k.setNumCoils(7);
    CHECK(k.getNumCoils() == 7);
}

/* --- impKnot::setNumTwists ---------------------------------------------------
 *
 * Unlike setNumCoils, there is no clamp at all here: twists is stored
 * exactly as given, including zero and negative values. */

TEST(knot_setNumTwists_stores_the_value_unclamped)
{
    impKnot k;

    k.setNumTwists(5);
    CHECK(k.getNumTwists() == 5);

    k.setNumTwists(0);
    CHECK(k.getNumTwists() == 0);

    k.setNumTwists(-3);
    CHECK(k.getNumTwists() == -3);
}

/* --- impKnot::value ----------------------------------------------------------
 *
 * value() sums one inverse-square falloff term per coil, each centered on a
 * ring displaced around the knot's tube. With the default matrix (identity)
 * and a point on the x axis at x = radius1 + radius2, y = z = 0: atan2(0, x)
 * is ~0, so lat ~0 and the i=0 term's lon is ~0 too, putting hor = temp -
 * cos(0)*radius2 = (radius2 - radius1... ) at exactly zero and ver at zero --
 * that term is dominated by IMP_MIN_DIVISOR and swamps the other two coils'
 * off-ring contributions. */

TEST(knot_value_peaks_on_the_first_coil_ring)
{
    impKnot k;
    float m[16];
    makeIdentity(m);
    k.setMatrix(m);

    /* radius1=1, radius2=0.5 (defaults): x = 1.5 sits exactly on the i=0
     * coil ring. Two other coils (default coils=3) each contribute a small
     * off-ring term; the total was cross-checked at 100.026672 by calling
     * the real rsSqrtf/rsCosf/rsSinf/rsAtan2f offline. */
    float onRing[3] = {1.5f, 0.0f, 0.0f};
    CHECK_NEAR(k.value(onRing), 100.026672f, 0.01f);
}

TEST(knot_value_falls_off_away_from_the_tube)
{
    impKnot k;
    float m[16];
    makeIdentity(m);
    k.setMatrix(m);

    /* Far past the ring (x=5 vs. the ring at x=1.5): every coil's hor term
     * is large, so no single term dominates and the total is small.
     * Cross-checked offline at 0.001912. */
    float farOff[3] = {5.0f, 0.0f, 0.0f};
    CHECK_NEAR(k.value(farOff), 0.001912f, 1e-4f);
}

TEST(knot_value_sums_exactly_one_term_per_coil)
{
    impKnot k;
    float m[16];
    makeIdentity(m);
    k.setMatrix(m);

    /* Same on-ring point as above, but with setNumCoils(1) there is only
     * one term in the sum -- no off-ring coils to add their small
     * contribution, so the total is exactly the IMP_MIN_DIVISOR-dominated
     * term itself, not 100.0267 as with three coils. This is what actually
     * pins the loop to `coils` rather than a hardcoded count. */
    k.setNumCoils(1);
    float onRing[3] = {1.5f, 0.0f, 0.0f};
    CHECK_NEAR(k.value(onRing), 100.0f, 0.01f);
}

/* --- impKnot::center ----------------------------------------------------------
 *
 * center() only reads mat, not invtrmat, so no setMatrix() is required --
 * same as impTorus::center in test_imp_primitives.cpp. */

TEST(knot_center_combines_the_radius_sum_along_local_x_with_position)
{
    impKnot k;

    /* Default radius1=1, radius2=0.5, identity matrix: center is one
     * (radius1+radius2) out along local x from the origin. */
    float c[3];
    k.center(c);
    CHECK_NEAR(c[0], 1.5f, kTol);
    CHECK_NEAR(c[1], 0.0f, kTol);
    CHECK_NEAR(c[2], 0.0f, kTol);

    /* Changing both radii and moving the shape both have to show up. */
    k.setRadius1(2.0f);
    k.setRadius2(1.0f);
    k.setPosition(10.0f, 20.0f, 30.0f);
    k.center(c);
    CHECK_NEAR(c[0], 13.0f, kTol);
    CHECK_NEAR(c[1], 20.0f, kTol);
    CHECK_NEAR(c[2], 30.0f, kTol);
}

/* --- impKnot::addCrawlPoint ---------------------------------------------------
 *
 * Places one crawl point per coil, evenly spaced around the tube's local
 * x-z circle of radius `radius2`, offset by `radius1` along x -- using plain
 * cosf/sinf, not the rsMath approximations, so these are exact to float
 * precision. */

TEST(knot_addCrawlPoint_places_one_point_per_coil_around_the_xz_circle)
{
    impKnot k;
    k.setNumCoils(4);

    impCrawlPointVector cpv;
    k.addCrawlPoint(cpv);

    CHECK(cpv.size() == 4);

    /* i=0: angle 0 -> (radius1 + radius2, 0, 0) = (1.5, 0, 0). */
    CHECK_NEAR(cpv[0].position[0], 1.5f, kTol);
    CHECK_NEAR(cpv[0].position[1], 0.0f, kTol);
    CHECK_NEAR(cpv[0].position[2], 0.0f, kTol);

    /* i=1: angle pi/2 -> (radius1, 0, radius2) = (1.0, 0, 0.5). */
    CHECK_NEAR(cpv[1].position[0], 1.0f, kTol);
    CHECK_NEAR(cpv[1].position[1], 0.0f, kTol);
    CHECK_NEAR(cpv[1].position[2], 0.5f, kTol);

    /* i=2: angle pi -> (radius1 - radius2, 0, 0) = (0.5, 0, 0). */
    CHECK_NEAR(cpv[2].position[0], 0.5f, kTol);
    CHECK_NEAR(cpv[2].position[2], 0.0f, kTol);

    /* i=3: angle 3pi/2 -> (radius1, 0, -radius2) = (1.0, 0, -0.5). */
    CHECK_NEAR(cpv[3].position[0], 1.0f, kTol);
    CHECK_NEAR(cpv[3].position[2], -0.5f, kTol);
}

TEST(knot_addCrawlPoint_appends_rather_than_replaces)
{
    impKnot k;
    k.setNumCoils(3);

    impCrawlPointVector cpv;
    k.addCrawlPoint(cpv);
    CHECK(cpv.size() == 3);

    k.addCrawlPoint(cpv);
    CHECK(cpv.size() == 6);

    /* The second batch repeats the same pattern as the first. */
    CHECK_NEAR(cpv[3].position[0], cpv[0].position[0], kTol);
    CHECK_NEAR(cpv[3].position[2], cpv[0].position[2], kTol);
}
