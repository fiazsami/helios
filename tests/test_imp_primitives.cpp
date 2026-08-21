/* Tests for the implicit primitive shapes' value() functions and the small
 * torus helpers built on top of them.
 *
 * impShape's constructor sets mat and invmat to identity but leaves invtrmat
 * uninitialised -- it is only filled in by setMatrix(). Every primitive below
 * whose value() reads invtrmat (all but impSphere, which reads invmat
 * directly) is given an explicit matrix via setMatrix() before value() is
 * called, so no test reads uninitialised memory.
 *
 * Matrices are column-major (mat[12..14] is the translation column), matching
 * the convention noted in test_rsmath.cpp.
 */

#include "harness.h"

#include "Implicit/impCapsule.h"
#include "Implicit/impEllipsoid.h"
#include "Implicit/impHexahedron.h"
#include "Implicit/impRoundedHexahedron.h"
#include "Implicit/impSphere.h"
#include "Implicit/impTorus.h"

namespace {

const float kTol = 1e-4f;

/* IMP_MIN_DIVISOR from impShape.h -- every value() formula below adds it to
 * the falloff denominator, so exact-center points still divide by a nonzero
 * number instead of the position term's own zero. */
const float kMinDivisor = 0.0001f;

void makeIdentity(float *m)
{
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void makeTranslation(float *m, float tx, float ty, float tz)
{
    makeIdentity(m);
    m[12] = tx;
    m[13] = ty;
    m[14] = tz;
}

}  // namespace

/* --- impHexahedron::value ---------------------------------------------------
 *
 * value() is the minimum of three per-axis inverse-square falloffs, computed
 * with two comparisons (xx<yy, then xx<zz or yy<zz) that together always pick
 * the smallest of the three -- so every combination of comparison outcomes
 * still has to return the true minimum. The four cases below drive both
 * branches of both comparisons. */

TEST(hexahedron_value_picks_x_when_x_is_farthest_on_both_branches)
{
    impHexahedron h;
    float m[16];
    makeIdentity(m);
    h.setMatrix(m);

    /* |x|=3 > |y|=2 > |z|=1, so xx < yy and xx < zz: both comparisons take
     * their true branch and the outer/inner true path returns xx. */
    float pos[3] = {3.0f, 2.0f, 1.0f};
    const float xx = 1.0f / (3.0f * 3.0f + kMinDivisor);
    CHECK_NEAR(h.value(pos), xx, kTol);
}

TEST(hexahedron_value_picks_z_when_xy_branch_is_true_but_z_is_smaller)
{
    impHexahedron h;
    float m[16];
    makeIdentity(m);
    h.setMatrix(m);

    /* |x|=2 > |y|=1 (xx < yy, true) but |z|=3 makes zz the true minimum, so
     * the true/false path (xx < yy but not xx < zz) has to fall through to
     * zz rather than returning xx early. */
    float pos[3] = {2.0f, 1.0f, 3.0f};
    const float zz = 1.0f / (3.0f * 3.0f + kMinDivisor);
    CHECK_NEAR(h.value(pos), zz, kTol);
}

TEST(hexahedron_value_picks_y_when_xy_branch_is_false_and_y_is_smaller)
{
    impHexahedron h;
    float m[16];
    makeIdentity(m);
    h.setMatrix(m);

    /* |y|=3 is largest so xx < yy is false; within the else, yy < zz is true
     * because |y| > |z|. False/true path returns yy. */
    float pos[3] = {1.0f, 3.0f, 2.0f};
    const float yy = 1.0f / (3.0f * 3.0f + kMinDivisor);
    CHECK_NEAR(h.value(pos), yy, kTol);
}

TEST(hexahedron_value_picks_z_when_xy_branch_is_false_and_z_is_smaller)
{
    impHexahedron h;
    float m[16];
    makeIdentity(m);
    h.setMatrix(m);

    /* |z|=3 is the largest of all three: xx < yy is false (|x|<|y|), and
     * within the else, yy < zz is also false. False/false path returns zz. */
    float pos[3] = {1.0f, 2.0f, 3.0f};
    const float zz = 1.0f / (3.0f * 3.0f + kMinDivisor);
    CHECK_NEAR(h.value(pos), zz, kTol);
}

TEST(hexahedron_value_at_center_uses_min_divisor_on_every_axis)
{
    impHexahedron h;
    float m[16];
    makeIdentity(m);
    h.setMatrix(m);

    /* All three transformed coordinates are exactly zero, so all three
     * per-axis terms are equal (1/kMinDivisor) and the tie has to resolve to
     * one of them rather than dividing by zero anywhere. */
    float pos[3] = {0.0f, 0.0f, 0.0f};
    const float expected = 1.0f / kMinDivisor;
    CHECK_NEAR(h.value(pos), expected, 1.0f);
}

/* --- impEllipsoid::value ---------------------------------------------------- */

TEST(ellipsoid_value_is_max_at_center_and_falls_off_with_squared_distance)
{
    impEllipsoid e;
    float m[16];
    makeIdentity(m);
    e.setMatrix(m);

    /* Default thickness is 0.1 (impShape's constructor), so thicknessSquared
     * is 0.01. */
    float center[3] = {0.0f, 0.0f, 0.0f};
    CHECK_NEAR(e.value(center), 0.01f / kMinDivisor, 1.0f);

    float off[3] = {3.0f, 4.0f, 0.0f};
    CHECK_NEAR(e.value(off), 0.01f / (25.0f + kMinDivisor), 1e-6f);
}

TEST(ellipsoid_value_follows_the_matrix_translation)
{
    impEllipsoid e;
    float m[16];
    makeTranslation(m, 5.0f, 0.0f, 0.0f);
    e.setMatrix(m);

    /* A point at the shape's new center should score exactly like an
     * untranslated shape evaluated at the origin -- proves invtrmat's
     * translation column is applied, not just its rotation/scale part. */
    float atNewCenter[3] = {5.0f, 0.0f, 0.0f};
    CHECK_NEAR(e.value(atNewCenter), 0.01f / kMinDivisor, 1.0f);
}

/* --- impSphere::value -------------------------------------------------------
 *
 * Unlike the others, impSphere reads invmat directly rather than invtrmat, so
 * it is well-defined straight from impShape's constructor (identity) without
 * a setMatrix() call. */

TEST(sphere_value_is_max_at_center_and_falls_off_with_squared_distance)
{
    impSphere s;

    float center[3] = {0.0f, 0.0f, 0.0f};
    CHECK_NEAR(s.value(center), 0.01f / kMinDivisor, 1.0f);

    float off[3] = {3.0f, 4.0f, 0.0f};
    CHECK_NEAR(s.value(off), 0.01f / (25.0f + kMinDivisor), 1e-6f);
}

TEST(sphere_value_follows_setPosition)
{
    impSphere s;
    s.setPosition(5.0f, 0.0f, 0.0f);

    float atNewCenter[3] = {5.0f, 0.0f, 0.0f};
    CHECK_NEAR(s.value(atNewCenter), 0.01f / kMinDivisor, 1.0f);
}

/* --- impTorus::value --------------------------------------------------------- */

TEST(torus_value_peaks_on_the_ring_and_falls_off_radially)
{
    impTorus t;
    float m[16];
    makeIdentity(m);
    t.setMatrix(m);

    /* Default radius is 1.0 (impTorus's constructor). A point exactly on the
     * ring, in-plane, drives temp = sqrt(1) - 1 = 0. */
    float onRing[3] = {1.0f, 0.0f, 0.0f};
    CHECK_NEAR(t.value(onRing), 0.01f / kMinDivisor, 1.0f);

    /* Off the ring in-plane: temp = sqrt(9) - 1 = 2. */
    float offRing[3] = {3.0f, 0.0f, 0.0f};
    CHECK_NEAR(t.value(offRing), 0.01f / (4.0f + kMinDivisor), 1e-6f);
}

TEST(torus_value_falls_off_along_the_tube_axis)
{
    impTorus t;
    float m[16];
    makeIdentity(m);
    t.setMatrix(m);

    /* Directly above the ring center (0,0), off the ring plane by 1: temp =
     * sqrt(0) - 1 = -1, so temp*temp still contributes 1, plus tz*tz = 1. */
    float aboveCenter[3] = {0.0f, 0.0f, 1.0f};
    CHECK_NEAR(t.value(aboveCenter), 0.01f / (2.0f + kMinDivisor), 1e-6f);
}

/* --- impTorus::center -------------------------------------------------------- */

TEST(torus_center_combines_radius_along_local_x_with_position)
{
    impTorus t;

    /* Default radius 1, default (identity) matrix: mat[0]=1, mat[12..14]=0,
     * so center is the point one radius out along local x. */
    float c[3];
    t.center(c);
    CHECK_NEAR(c[0], 1.0f, kTol);
    CHECK_NEAR(c[1], 0.0f, kTol);
    CHECK_NEAR(c[2], 0.0f, kTol);

    /* Moving the shape and changing the radius both have to show up: radius
     * 2 along local x, plus the new translation. */
    t.setRadius(2.0f);
    t.setPosition(10.0f, 20.0f, 30.0f);
    t.center(c);
    CHECK_NEAR(c[0], 12.0f, kTol);
    CHECK_NEAR(c[1], 20.0f, kTol);
    CHECK_NEAR(c[2], 30.0f, kTol);
}

/* --- impTorus::addCrawlPoint -------------------------------------------------- */

TEST(torus_addCrawlPoint_appends_the_same_point_as_center)
{
    impTorus t;
    t.setRadius(2.0f);
    t.setPosition(10.0f, 20.0f, 30.0f);

    impCrawlPointVector cpv;
    t.addCrawlPoint(cpv);

    CHECK(cpv.size() == 1);
    CHECK_NEAR(cpv[0].position[0], 12.0f, kTol);
    CHECK_NEAR(cpv[0].position[1], 20.0f, kTol);
    CHECK_NEAR(cpv[0].position[2], 30.0f, kTol);

    /* Appends rather than replaces. */
    t.addCrawlPoint(cpv);
    CHECK(cpv.size() == 2);
}

/* --- impCapsule::value -------------------------------------------------------
 *
 * The along-axis term is clamped to zero inside the capsule's length and only
 * grows past its ends: sz = zz * (zz > 0.0f) where zz = fabsf(tz) - length. */

TEST(capsule_value_ignores_axial_distance_within_its_length)
{
    impCapsule c;
    float m[16];
    makeIdentity(m);
    c.setMatrix(m);

    /* Default length is 1.0. z=0 is well inside it, so the length term
     * (zz negative) is clamped to zero and only tx, ty contribute. */
    float onAxis[3] = {0.0f, 0.0f, 0.0f};
    CHECK_NEAR(c.value(onAxis), 0.01f / kMinDivisor, 1.0f);

    float offAxisWithinLength[3] = {1.0f, 1.0f, 0.0f};
    CHECK_NEAR(c.value(offAxisWithinLength), 0.01f / (2.0f + kMinDivisor), 1e-6f);
}

TEST(capsule_value_falls_off_past_its_length)
{
    impCapsule c;
    float m[16];
    makeIdentity(m);
    c.setMatrix(m);

    /* z=3 is past the default length of 1, so zz = 3 - 1 = 2 is positive and
     * unclamped: sz*sz = 4. */
    float pastEnd[3] = {0.0f, 0.0f, 3.0f};
    CHECK_NEAR(c.value(pastEnd), 0.01f / (4.0f + kMinDivisor), 1e-6f);
}

/* --- impRoundedHexahedron::value and impRoundedHexahedron() -----------------
 *
 * Each axis clamps the same way the capsule's length axis does: sx = xx *
 * (xx > 0.0f) where xx = fabsf(tx) - width, and likewise for height/length.
 * Constructing the shape exercises its constructor (default width = height =
 * length = 1.0), and every value() call below relies on those defaults. */

TEST(roundedHexahedron_value_is_zero_falloff_inside_the_box)
{
    impRoundedHexahedron rh;
    float m[16];
    makeIdentity(m);
    rh.setMatrix(m);

    float center[3] = {0.0f, 0.0f, 0.0f};
    CHECK_NEAR(rh.value(center), 0.01f / kMinDivisor, 1.0f);
}

TEST(roundedHexahedron_value_only_the_axis_past_the_box_contributes)
{
    impRoundedHexahedron rh;
    float m[16];
    makeIdentity(m);
    rh.setMatrix(m);

    /* x=2 is past the default width of 1 (xx = 2 - 1 = 1); y and z are at 0,
     * inside their default height/length of 1, so they clamp to zero. */
    float pastX[3] = {2.0f, 0.0f, 0.0f};
    CHECK_NEAR(rh.value(pastX), 0.01f / (1.0f + kMinDivisor), 1e-6f);
}

TEST(roundedHexahedron_value_sums_all_three_axes_past_the_box)
{
    impRoundedHexahedron rh;
    float m[16];
    makeIdentity(m);
    rh.setMatrix(m);

    /* Past the box on all three axes by 1 each (2 - 1): 1+1+1 = 3. */
    float pastAll[3] = {2.0f, 2.0f, 2.0f};
    CHECK_NEAR(rh.value(pastAll), 0.01f / (3.0f + kMinDivisor), 1e-6f);
}
