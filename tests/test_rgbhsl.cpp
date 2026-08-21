/* Tests for Rgbhsl's colour conversions.
 *
 * Note this is not standard HSL. "Luminosity" here is max(r, g, b) -- what
 * most references call value -- and saturation is derived from the raw channel
 * rather than the normalised one. Expected values below are derived from the
 * documented contract in Rgbhsl.h (all six quantities in 0..1) and from
 * hsl2rgb's own arithmetic, not copied from a run, except where a case says
 * otherwise in a comment.
 */

#include "harness.h"

#include "Rgbhsl.h"

namespace {

/* Single-precision maths through a divide; a tolerance this tight still
 * discriminates every case here, where the smallest meaningful gap is 1/6. */
const float kTol = 1e-5f;

}  // namespace

/* --- hsl2rgb: the direction whose arithmetic is fully derivable ----------- */

TEST(hsl2rgb_hue_zero_is_pure_red)
{
    float r = -1.0f, g = -1.0f, b = -1.0f;
    hsl2rgb(0.0f, 1.0f, 1.0f, r, g, b);
    CHECK_NEAR(r, 1.0f, kTol);
    CHECK_NEAR(g, 0.0f, kTol);
    CHECK_NEAR(b, 0.0f, kTol);
}

TEST(hsl2rgb_ramps_green_across_the_first_sextant)
{
    /* h = 1/12 sits halfway into the first sextant, where g = h * 6. */
    float r = 0.0f, g = 0.0f, b = 0.0f;
    hsl2rgb(1.0f / 12.0f, 1.0f, 1.0f, r, g, b);
    CHECK_NEAR(r, 1.0f, kTol);
    CHECK_NEAR(g, 0.5f, kTol);
    CHECK_NEAR(b, 0.0f, kTol);
}

TEST(hsl2rgb_zero_saturation_is_grey_whatever_the_hue)
{
    /* s = 0 collapses every channel to 1 before luminosity scales it, so hue
     * cannot influence the result. Checked at three hues that land in three
     * different branches of the sextant tree. */
    const float hues[3] = {0.05f, 0.4f, 0.9f};
    for (int i = 0; i < 3; i++) {
        float r = 0.0f, g = 0.0f, b = 0.0f;
        hsl2rgb(hues[i], 0.0f, 0.25f, r, g, b);
        CHECK_NEAR(r, 0.25f, kTol);
        CHECK_NEAR(g, 0.25f, kTol);
        CHECK_NEAR(b, 0.25f, kTol);
    }
}

TEST(hsl2rgb_zero_luminosity_is_black)
{
    float r = 1.0f, g = 1.0f, b = 1.0f;
    hsl2rgb(0.4f, 1.0f, 0.0f, r, g, b);
    CHECK_NEAR(r, 0.0f, kTol);
    CHECK_NEAR(g, 0.0f, kTol);
    CHECK_NEAR(b, 0.0f, kTol);
}

TEST(hsl2rgb_wraps_hue_past_one)
{
    /* fmodf(h, 1) makes the wheel periodic, so h and h + 1 are the same colour.
     * This is the property the tween functions rely on when they run past 1. */
    float r0 = 0.0f, g0 = 0.0f, b0 = 0.0f;
    float r1 = 0.0f, g1 = 0.0f, b1 = 0.0f;
    hsl2rgb(0.3f, 0.8f, 0.7f, r0, g0, b0);
    hsl2rgb(1.3f, 0.8f, 0.7f, r1, g1, b1);
    CHECK_NEAR(r1, r0, kTol);
    CHECK_NEAR(g1, g0, kTol);
    CHECK_NEAR(b1, b0, kTol);
}

TEST(hsl2rgb_reaches_each_primary)
{
    float r = 0.0f, g = 0.0f, b = 0.0f;

    hsl2rgb(1.0f / 3.0f, 1.0f, 1.0f, r, g, b);   /* full green */
    CHECK_NEAR(r, 0.0f, kTol);
    CHECK_NEAR(g, 1.0f, kTol);
    CHECK_NEAR(b, 0.0f, kTol);

    hsl2rgb(2.0f / 3.0f, 1.0f, 1.0f, r, g, b);   /* full blue */
    CHECK_NEAR(r, 0.0f, kTol);
    CHECK_NEAR(g, 0.0f, kTol);
    CHECK_NEAR(b, 1.0f, kTol);
}

/* --- rgb2hsl ------------------------------------------------------------- */

TEST(rgb2hsl_black_short_circuits_to_full_saturation)
{
    /* The l == 0 early return is a documented corner: with no luminosity there
     * is no hue to report, and the function reports s = 1 rather than 0. */
    float h = -1.0f, s = -1.0f, l = -1.0f;
    rgb2hsl(0.0f, 0.0f, 0.0f, h, s, l);
    CHECK_NEAR(l, 0.0f, kTol);
    CHECK_NEAR(h, 0.0f, kTol);
    CHECK_NEAR(s, 1.0f, kTol);
}

TEST(rgb2hsl_places_each_primary_on_the_wheel)
{
    float h = 0.0f, s = 0.0f, l = 0.0f;

    rgb2hsl(1.0f, 0.0f, 0.0f, h, s, l);
    CHECK_NEAR(h, 0.0f, kTol);
    CHECK_NEAR(s, 1.0f, kTol);
    CHECK_NEAR(l, 1.0f, kTol);

    rgb2hsl(0.0f, 1.0f, 0.0f, h, s, l);
    CHECK_NEAR(h, 1.0f / 3.0f, 1e-3f);   /* the source uses 0.166667 literals */
    CHECK_NEAR(s, 1.0f, kTol);
    CHECK_NEAR(l, 1.0f, kTol);

    rgb2hsl(0.0f, 0.0f, 1.0f, h, s, l);
    CHECK_NEAR(h, 2.0f / 3.0f, 1e-3f);
    CHECK_NEAR(s, 1.0f, kTol);
    CHECK_NEAR(l, 1.0f, kTol);
}

TEST(rgb2hsl_luminosity_is_the_largest_channel_in_four_of_six_huezones)
{
    /* rgb2hsl sorts the input into one of six huezones and reads luminosity off
     * the channel that zone says is largest. It gets that right in zones 0, 1,
     * 4 and 5 -- see the characterization case below for 2 and 3. */
    float h = 0.0f, s = 0.0f, l = 0.0f;

    rgb2hsl(0.75f, 0.25f, 0.5f, h, s, l);   /* zone 0: r largest */
    CHECK_NEAR(l, 0.75f, kTol);

    rgb2hsl(0.5f, 1.0f, 0.2f, h, s, l);     /* zone 1: g largest */
    CHECK_NEAR(l, 1.0f, kTol);

    rgb2hsl(0.5f, 0.2f, 0.9f, h, s, l);     /* zone 4: b largest */
    CHECK_NEAR(l, 0.9f, kTol);

    rgb2hsl(0.9f, 0.2f, 0.5f, h, s, l);     /* zone 5: r largest */
    CHECK_NEAR(l, 0.9f, kTol);
}

TEST(rgb2hsl_luminosity_is_wrong_in_huezones_two_and_three)
{
    /* CHARACTERIZATION -- pins current behaviour, not intended behaviour.
     *
     * The luminosity switch groups the huezones as {0,5}->r, {1,2}->g,
     * {3,4}->b. Zones 2 and 3 are in the wrong groups: zone 2 is r<g with b>g,
     * where blue is the largest channel, and zone 3 is r<g with r<b<=g, where
     * green is. The correct grouping is {1,3}->g and {2,4}->b.
     *
     * Consequence: l comes back as neither the largest channel nor anything
     * else meaningful, and the rgb2hsl -> hsl2rgb round trip does not close for
     * any colour landing in those two zones.
     *
     * Left unfixed on purpose -- this commit adds tests, and changing behaviour
     * underneath them would defeat writing them first. When it is fixed, these
     * two assertions are the ones that will fail, and they should be replaced
     * with the largest-channel expectation rather than retuned. */
    float h = 0.0f, s = 0.0f, l = 0.0f;

    rgb2hsl(0.25f, 0.5f, 0.8f, h, s, l);    /* zone 2: b is largest at 0.8 */
    CHECK_NEAR(l, 0.5f, kTol);              /* reports g instead */

    rgb2hsl(0.25f, 0.9f, 0.5f, h, s, l);    /* zone 3: g is largest at 0.9 */
    CHECK_NEAR(l, 0.5f, kTol);              /* reports b instead */
}

TEST(rgb2hsl_round_trips_saturated_colours_at_full_luminosity)
{
    /* Round-tripping is the contract the two functions owe each other, and it
     * is what the tween path depends on. Restricted to fully saturated colours
     * at l = 1 deliberately: see the two characterization cases for why it does
     * not hold everywhere. */
    /* Fully saturated (min channel 0) at full luminosity (max channel 1), and
     * landing in huezones 0, 1, 4 and 5. Every one of those restrictions is
     * load-bearing -- see the characterization cases for what happens outside
     * them. */
    const float reds[5]   = {1.0f, 1.0f, 0.0f, 0.0f, 1.0f};
    const float greens[5] = {0.0f, 1.0f, 1.0f, 0.0f, 0.0f};
    const float blues[5]  = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f};

    for (int i = 0; i < 5; i++) {
        float h = 0.0f, s = 0.0f, l = 0.0f;
        float r = 0.0f, g = 0.0f, b = 0.0f;
        rgb2hsl(reds[i], greens[i], blues[i], h, s, l);
        hsl2rgb(h, s, l, r, g, b);
        CHECK_NEAR(r, reds[i], 1e-3f);
        CHECK_NEAR(g, greens[i], 1e-3f);
        CHECK_NEAR(b, blues[i], 1e-3f);
    }
}

TEST(rgb2hsl_does_not_round_trip_partially_saturated_colours)
{
    /* CHARACTERIZATION -- pins current behaviour, not intended behaviour.
     *
     * rgb2hsl normalises the channels into rr/gg/bb and then desaturates them,
     * and then computes h and s from the *raw* inputs instead and never reads
     * rr/gg/bb again. They are dead locals. Where raw and normalised coincide
     * -- fully saturated at full luminosity -- the round trip closes; anywhere
     * else the hue comes back describing a different colour.
     *
     * (0.5, 1, 0.2) is huezone 1, so it is not affected by the luminosity bug
     * above: l comes back as 1.0 correctly. It is purely the hue that is wrong,
     * which is why it is worth a separate case. */
    float h = 0.0f, s = 0.0f, l = 0.0f;
    float r = 0.0f, g = 0.0f, b = 0.0f;

    rgb2hsl(0.5f, 1.0f, 0.2f, h, s, l);
    CHECK_NEAR(l, 1.0f, kTol);          /* luminosity is right here */
    CHECK_NEAR(s, 0.8f, kTol);          /* and so is saturation, at l = 1 */

    hsl2rgb(h, s, l, r, g, b);
    CHECK_NEAR(g, 1.0f, 1e-3f);         /* green and blue survive */
    CHECK_NEAR(b, 0.2f, 1e-3f);
    CHECK_NEAR(r, 0.6f, 1e-3f);         /* red does not: 0.5 went in */
}

TEST(rgb2hsl_saturation_uses_the_raw_channel_not_the_normalised_one)
{
    /* CHARACTERIZATION -- this pins current behaviour, not intended behaviour.
     *
     * Mid grey is achromatic and should report s = 0. It reports 0.5, because
     * the saturation step computes `s = 1.0f - b` from the raw channel after
     * having normalised into rr/gg/bb; `1.0f - bb` would give 0. The same
     * substitution appears in all three huezone branches.
     *
     * This is the same root cause as the round-trip case above: rr/gg/bb are
     * computed and discarded. Left as-is on purpose -- this commit adds tests,
     * and changing behaviour under them would defeat writing them first. If it
     * is fixed, this case is the one that will fail, and it should be replaced
     * with s == 0 rather than retuned. */
    float h = 0.0f, s = 0.0f, l = 0.0f;
    rgb2hsl(0.5f, 0.5f, 0.5f, h, s, l);
    CHECK_NEAR(l, 0.5f, kTol);
    CHECK_NEAR(s, 0.5f, kTol);
}

/* --- hsl2rgb: the one remaining sextant ----------------------------------- */

TEST(hsl2rgb_ramps_green_down_in_the_blue_sextant)
{
    /* h = 0.55 sits in [0.5, 0.666667): full blue, green ramping down, red at
     * zero. Every other sextant is already exercised above; this is the one
     * left uncovered. */
    float r = -1.0f, g = -1.0f, b = -1.0f;
    hsl2rgb(0.55f, 1.0f, 1.0f, r, g, b);
    CHECK_NEAR(r, 0.0f, kTol);
    CHECK_NEAR(g, 0.7f, kTol);
    CHECK_NEAR(b, 1.0f, kTol);
}

/* --- hslTween -------------------------------------------------------------- */

TEST(hslTween_forward_takes_the_direct_arc)
{
    /* direction == 0, h2 >= h1: straight interpolation, no wraparound. */
    float outh = -1.0f, outs = -1.0f, outl = -1.0f;
    hslTween(0.2f, 0.3f, 0.4f, 0.6f, 0.8f, 0.9f, 0.5f, 0, outh, outs, outl);
    CHECK_NEAR(outh, 0.4f, kTol);
    CHECK_NEAR(outs, 0.55f, kTol);
    CHECK_NEAR(outl, 0.65f, kTol);
}

TEST(hslTween_forward_wraps_without_crossing_one)
{
    /* direction == 0, h2 < h1: goes the other way around the wheel, but the
     * result stays inside [0, 1] so the `outh > 1` correction is not taken. */
    float outh = -1.0f, outs = -1.0f, outl = -1.0f;
    hslTween(0.6f, 0.3f, 0.4f, 0.2f, 0.8f, 0.9f, 0.5f, 0, outh, outs, outl);
    CHECK_NEAR(outh, 0.9f, kTol);
}

TEST(hslTween_forward_wraps_and_corrects_past_one)
{
    /* Same branch as above, but with a tween large enough that the raw sum
     * overshoots 1 and the `outh -= 1.0f` correction fires. */
    float outh = -1.0f, outs = -1.0f, outl = -1.0f;
    hslTween(0.6f, 0.3f, 0.4f, 0.2f, 0.8f, 0.9f, 0.9f, 0, outh, outs, outl);
    CHECK_NEAR(outh, 0.14f, kTol);
    CHECK_NEAR(outs, 0.75f, kTol);
    CHECK_NEAR(outl, 0.85f, kTol);
}

TEST(hslTween_backward_takes_the_direct_arc)
{
    /* direction == 1, h1 >= h2: straight interpolation, no wraparound. */
    float outh = -1.0f, outs = -1.0f, outl = -1.0f;
    hslTween(0.6f, 0.3f, 0.4f, 0.2f, 0.8f, 0.9f, 0.5f, 1, outh, outs, outl);
    CHECK_NEAR(outh, 0.4f, kTol);
}

TEST(hslTween_backward_wraps_without_crossing_zero)
{
    /* direction == 1, h1 < h2: goes the other way around the wheel, but the
     * result stays >= 0 so the `outh += 1.0f` correction is not taken. */
    float outh = -1.0f, outs = -1.0f, outl = -1.0f;
    hslTween(0.2f, 0.3f, 0.4f, 0.6f, 0.8f, 0.9f, 0.2f, 1, outh, outs, outl);
    CHECK_NEAR(outh, 0.08f, kTol);
    CHECK_NEAR(outs, 0.4f, kTol);
    CHECK_NEAR(outl, 0.5f, kTol);
}

TEST(hslTween_backward_wraps_and_corrects_below_zero)
{
    /* Same branch as above, but with a tween large enough that the raw
     * difference goes negative and the `outh += 1.0f` correction fires. */
    float outh = -1.0f, outs = -1.0f, outl = -1.0f;
    hslTween(0.2f, 0.3f, 0.4f, 0.6f, 0.8f, 0.9f, 0.9f, 1, outh, outs, outl);
    CHECK_NEAR(outh, 0.66f, kTol);
    CHECK_NEAR(outs, 0.75f, kTol);
    CHECK_NEAR(outl, 0.85f, kTol);
}

TEST(hslTween_endpoints_at_tween_zero_and_one)
{
    /* tween == 0 reproduces color 1 exactly; tween == 1 reproduces color 2
     * exactly, regardless of direction. */
    float outh = -1.0f, outs = -1.0f, outl = -1.0f;

    hslTween(0.2f, 0.3f, 0.4f, 0.6f, 0.8f, 0.9f, 0.0f, 0, outh, outs, outl);
    CHECK_NEAR(outh, 0.2f, kTol);
    CHECK_NEAR(outs, 0.3f, kTol);
    CHECK_NEAR(outl, 0.4f, kTol);

    hslTween(0.2f, 0.3f, 0.4f, 0.6f, 0.8f, 0.9f, 1.0f, 0, outh, outs, outl);
    CHECK_NEAR(outh, 0.6f, kTol);
    CHECK_NEAR(outs, 0.8f, kTol);
    CHECK_NEAR(outl, 0.9f, kTol);
}

/* --- rgbTween: composition across every huezone ---------------------------
 *
 * rgbTween is rgb2hsl -> hslTween -> hsl2rgb. It has no branches of its own,
 * but it is the thing that actually walks colors through every huezone rgb2hsl
 * can classify, so each case below picks a first color from a different one of
 * the six zones and tweens it halfway to a fixed mid-grey second color.
 *
 * None of these round-trip cleanly -- ss-wrt means rgb2hsl's hue/saturation
 * come from the raw channels rather than the normalised ones for any color
 * that isn't fully saturated at full luminosity, which none of these are.
 * Zones 2 and 3 additionally carry ss-4z1 (wrong luminosity channel). All six
 * are characterization: they pin what the port must reproduce (or knowingly
 * change) rather than a corrected contract. Expected values are the compiled
 * algorithm's own output, cross-checked with an independent float32
 * reimplementation, not a hand-derived HSL result. */

TEST(rgbTween_huezone0_forward_half)
{
    float outr = -1.0f, outg = -1.0f, outb = -1.0f;
    rgbTween(0.9f, 0.4f, 0.2f, 0.5f, 0.5f, 0.5f, 0.5f, 0, outr, outg, outb);
    CHECK_NEAR(outr, 0.7f, 1e-3f);
    CHECK_NEAR(outg, 0.44975f, 1e-3f);
    CHECK_NEAR(outb, 0.245f, 1e-3f);
}

TEST(rgbTween_huezone1_forward_half)
{
    float outr = -1.0f, outg = -1.0f, outb = -1.0f;
    rgbTween(0.5f, 1.0f, 0.2f, 0.5f, 0.5f, 0.5f, 0.5f, 0, outr, outg, outb);
    CHECK_NEAR(outr, 0.2625f, 1e-3f);
    CHECK_NEAR(outg, 0.2625f, 1e-3f);
    CHECK_NEAR(outb, 0.75f, 1e-3f);
}

TEST(rgbTween_huezone2_forward_half)
{
    /* ss-4z1: huezone 2 reads luminosity off g instead of b.
     * ss-wrt: hue/saturation come from raw r/g/b, not the normalised values.
     * Both feed this color's HSL extraction before it is even tweened. */
    float outr = -1.0f, outg = -1.0f, outb = -1.0f;
    rgbTween(0.25f, 0.5f, 0.8f, 0.5f, 0.5f, 0.5f, 0.5f, 0, outr, outg, outb);
    CHECK_NEAR(outr, 0.390624f, 1e-3f);
    CHECK_NEAR(outg, 0.1875f, 1e-3f);
    CHECK_NEAR(outb, 0.5f, 1e-3f);
}

TEST(rgbTween_huezone3_forward_half)
{
    /* ss-4z1: huezone 3 reads luminosity off b instead of g.
     * ss-wrt: hue/saturation come from raw r/g/b, not the normalised values. */
    float outr = -1.0f, outg = -1.0f, outb = -1.0f;
    rgbTween(0.25f, 0.9f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0, outr, outg, outb);
    CHECK_NEAR(outr, 0.437499f, 1e-3f);
    CHECK_NEAR(outg, 0.1875f, 1e-3f);
    CHECK_NEAR(outb, 0.5f, 1e-3f);
}

TEST(rgbTween_huezone4_forward_half)
{
    float outr = -1.0f, outg = -1.0f, outb = -1.0f;
    rgbTween(0.5f, 0.2f, 0.9f, 0.5f, 0.5f, 0.5f, 0.5f, 0, outr, outg, outb);
    CHECK_NEAR(outr, 0.7f, 1e-3f);
    CHECK_NEAR(outg, 0.245f, 1e-3f);
    CHECK_NEAR(outb, 0.472499f, 1e-3f);
}

TEST(rgbTween_huezone5_forward_half)
{
    float outr = -1.0f, outg = -1.0f, outb = -1.0f;
    rgbTween(0.9f, 0.2f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0, outr, outg, outb);
    CHECK_NEAR(outr, 0.7f, 1e-3f);
    CHECK_NEAR(outg, 0.245f, 1e-3f);
    CHECK_NEAR(outb, 0.245f, 1e-3f);
}
