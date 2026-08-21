/* Tests for impShape itself -- the base class every implicit primitive
 * derives from. impShape has no pure virtual methods, so it is instantiated
 * directly here rather than through a subclass: every assertion below reads
 * impShape's own public members (mat/invmat/invtrmat/thickness/
 * thicknessSquared), so an effect that only showed up incidentally through a
 * subclass's own logic would not be enough to pass these -- the state
 * asserted on is exactly the state each function under test writes.
 *
 * Matrices are column-major (mat[12..14] is the translation column), matching
 * the convention noted in test_rsmath.cpp and test_imp_primitives.cpp.
 */

#include "harness.h"

#include "Implicit/impShape.h"

namespace {

const float kTol = 1e-4f;

void makeIdentity(float *m)
{
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

}  // namespace

/* --- impShape::impShape() ----------------------------------------------------
 *
 * The constructor sets mat and invmat to identity (invtrmat is left
 * uninitialised -- only setMatrix() fills it in) and gives thickness its
 * documented default of 0.1, with thicknessSquared precomputed from it. */

TEST(constructor_sets_mat_and_invmat_to_identity)
{
    impShape s;
    for (int i = 0; i < 16; i++) {
        const float expected = (i == 0 || i == 5 || i == 10 || i == 15) ? 1.0f : 0.0f;
        CHECK_NEAR(s.mat[i], expected, kTol);
        CHECK_NEAR(s.invmat[i], expected, kTol);
    }
}

TEST(constructor_sets_default_thickness_and_its_square)
{
    impShape s;
    CHECK_NEAR(s.thickness, 0.1f, kTol);
    CHECK_NEAR(s.thicknessSquared, 0.01f, kTol);
}

/* --- impShape::setThickness --------------------------------------------------
 *
 * Stores thickness as given and precomputes thicknessSquared as its square,
 * not a copy -- a no-op or a mis-derived thicknessSquared both show up here. */

TEST(setThickness_stores_the_value_and_squares_it_separately)
{
    impShape s;
    s.setThickness(3.0f);
    CHECK_NEAR(s.thickness, 3.0f, kTol);
    CHECK_NEAR(s.thicknessSquared, 9.0f, kTol);
    CHECK(s.getThickness() == 3.0f);
}

/* --- impShape::setPosition(float, float, float) ------------------------------
 *
 * Writes the translation column of mat, the negated translation column of
 * invmat, and the negated translation entries of invtrmat (indices 3, 7, 11 --
 * the last column of each row in the transposed-inverse layout). setMatrix()
 * is called first only to give invtrmat defined values to overwrite; the
 * assertions below are on the entries setPosition itself is documented to
 * touch. */

TEST(setPosition_xyz_sets_the_translation_column_and_its_negated_inverses)
{
    impShape s;
    float identity[16];
    makeIdentity(identity);
    s.setMatrix(identity);

    s.setPosition(1.0f, 2.0f, 3.0f);

    CHECK_NEAR(s.mat[12], 1.0f, kTol);
    CHECK_NEAR(s.mat[13], 2.0f, kTol);
    CHECK_NEAR(s.mat[14], 3.0f, kTol);

    CHECK_NEAR(s.invmat[12], -1.0f, kTol);
    CHECK_NEAR(s.invmat[13], -2.0f, kTol);
    CHECK_NEAR(s.invmat[14], -3.0f, kTol);

    CHECK_NEAR(s.invtrmat[3], -1.0f, kTol);
    CHECK_NEAR(s.invtrmat[7], -2.0f, kTol);
    CHECK_NEAR(s.invtrmat[11], -3.0f, kTol);
}

/* --- impShape::setPosition(float*) --------------------------------------------
 *
 * The pointer overload is a thin forward to the three-float version; the
 * point of this test is that the array's elements land in the same x/y/z
 * slots, not just that some position got set. */

TEST(setPosition_array_overload_forwards_elements_in_order)
{
    impShape s;
    float pos[3] = {4.0f, 5.0f, 6.0f};

    s.setPosition(pos);

    CHECK_NEAR(s.mat[12], 4.0f, kTol);
    CHECK_NEAR(s.mat[13], 5.0f, kTol);
    CHECK_NEAR(s.mat[14], 6.0f, kTol);
}

/* --- impShape::determinant3 ---------------------------------------------------
 *
 * Plain 3x3 determinant, called with (row-major) aa..cc. */

TEST(determinant3_computes_the_determinant_of_a_nonsingular_matrix)
{
    impShape s;

    /* | 1 2 3 |
     * | 0 1 4 |
     * | 5 6 0 |   -> classic textbook example, det = 1. */
    const float det = s.determinant3(1.0f, 2.0f, 3.0f,
                                      0.0f, 1.0f, 4.0f,
                                      5.0f, 6.0f, 0.0f);
    CHECK_NEAR(det, 1.0f, kTol);
}

TEST(determinant3_is_zero_for_linearly_dependent_rows)
{
    impShape s;

    /* Row 2 is exactly twice row 1, so the matrix is singular. */
    const float det = s.determinant3(1.0f, 2.0f, 3.0f,
                                      2.0f, 4.0f, 6.0f,
                                      0.0f, 0.0f, 1.0f);
    CHECK_NEAR(det, 0.0f, kTol);
}

/* --- impShape::invertMatrix ---------------------------------------------------
 *
 * Reads mat, writes invmat, and reports whether mat was invertible. Exercised
 * directly (not just through setMatrix) so failure is attributable to
 * invertMatrix's own arithmetic rather than setMatrix's forwarding of it. */

TEST(invertMatrix_inverts_a_diagonal_scale_matrix)
{
    impShape s;
    for (int i = 0; i < 16; i++) s.mat[i] = 0.0f;
    s.mat[0] = 2.0f;
    s.mat[5] = 4.0f;
    s.mat[10] = 5.0f;
    s.mat[15] = 1.0f;

    const bool ok = s.invertMatrix();

    CHECK(ok);
    CHECK_NEAR(s.invmat[0], 0.5f, kTol);
    CHECK_NEAR(s.invmat[5], 0.25f, kTol);
    CHECK_NEAR(s.invmat[10], 0.2f, kTol);
    CHECK_NEAR(s.invmat[15], 1.0f, kTol);
    /* Off-diagonal entries of a diagonal matrix's inverse stay zero. */
    CHECK_NEAR(s.invmat[1], 0.0f, kTol);
    CHECK_NEAR(s.invmat[12], 0.0f, kTol);
}

TEST(invertMatrix_inverts_a_pure_translation_by_negating_it)
{
    impShape s;
    makeIdentity(s.mat);
    s.mat[12] = 10.0f;
    s.mat[13] = 20.0f;
    s.mat[14] = 30.0f;

    const bool ok = s.invertMatrix();

    CHECK(ok);
    CHECK_NEAR(s.invmat[12], -10.0f, kTol);
    CHECK_NEAR(s.invmat[13], -20.0f, kTol);
    CHECK_NEAR(s.invmat[14], -30.0f, kTol);
    /* The linear part of a pure translation is untouched by inversion. */
    CHECK_NEAR(s.invmat[0], 1.0f, kTol);
    CHECK_NEAR(s.invmat[5], 1.0f, kTol);
    CHECK_NEAR(s.invmat[10], 1.0f, kTol);
}

TEST(invertMatrix_returns_false_for_a_singular_matrix)
{
    impShape s;
    for (int i = 0; i < 16; i++) s.mat[i] = 0.0f;

    const bool ok = s.invertMatrix();

    CHECK(!ok);
}

/* --- impShape::setMatrix -------------------------------------------------------
 *
 * Copies m into mat, inverts it into invmat, then builds invtrmat as the
 * transpose of invmat. The shear matrix below has a distinctive off-diagonal
 * entry so the test can tell a genuine transpose apart from a bug that copied
 * invmat into invtrmat verbatim: a verbatim copy would leave the entry at
 * invtrmat[4], not move it to invtrmat[1]. */

TEST(setMatrix_copies_mat_and_builds_invtrmat_as_the_transpose_of_invmat)
{
    impShape s;
    float m[16];
    makeIdentity(m);
    /* Row 0, column 1 (mat[4]) = 2: shear x' = x + 2y. Upper-triangular unit
     * matrix, so its inverse is the same shear negated: invmat[4] = -2. */
    m[4] = 2.0f;

    s.setMatrix(m);

    /* mat is copied verbatim. */
    CHECK_NEAR(s.mat[4], 2.0f, kTol);

    /* invertMatrix's own arithmetic is covered separately above; here it is
     * only the anchor value that setMatrix's transpose step has to move. */
    CHECK_NEAR(s.invmat[4], -2.0f, kTol);

    /* The transpose moves that value from invmat[4] (row 0, col 1) to
     * invtrmat[1] (row 1, col 0) -- and invtrmat[4] must NOT still hold it. */
    CHECK_NEAR(s.invtrmat[1], -2.0f, kTol);
    CHECK_NEAR(s.invtrmat[4], 0.0f, kTol);
}

/* --- impShape::addCrawlPoint ---------------------------------------------------
 *
 * Appends exactly one crawl point at the shape's translation (mat[12..14]). */

TEST(addCrawlPoint_appends_one_point_at_the_shapes_position)
{
    impShape s;
    s.setPosition(1.0f, 2.0f, 3.0f);

    impCrawlPointVector cpv;
    s.addCrawlPoint(cpv);

    CHECK(cpv.size() == 1);
    CHECK_NEAR(cpv[0].position[0], 1.0f, kTol);
    CHECK_NEAR(cpv[0].position[1], 2.0f, kTol);
    CHECK_NEAR(cpv[0].position[2], 3.0f, kTol);
}

TEST(addCrawlPoint_appends_rather_than_replaces)
{
    impShape s;

    impCrawlPointVector cpv;
    s.addCrawlPoint(cpv);
    CHECK(cpv.size() == 1);

    s.addCrawlPoint(cpv);
    CHECK(cpv.size() == 2);
}
