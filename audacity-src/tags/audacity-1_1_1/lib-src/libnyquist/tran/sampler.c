#include "stdio.h"
#ifndef mips
#include "stdlib.h"
#endif
#include "xlisp.h"
#include "sound.h"

#include "falloc.h"
#include "cext.h"
#include "sampler.h"

void sampler_free();


typedef struct sampler_susp_struct {
    snd_susp_node susp;
    boolean started;
    long terminate_cnt;
    boolean logically_stopped;
    sound_type s_fm;
    long s_fm_cnt;
    sample_block_values_type s_fm_ptr;

    /* support for interpolation of s_fm */
    sample_type s_fm_x1_sample;
    double s_fm_pHaSe;
    double s_fm_pHaSe_iNcR;

    /* support for ramp between samples of s_fm */
    double output_per_s_fm;
    long s_fm_n;

    double loop_to;
    table_type the_table;
    sample_type *table_ptr;
    double table_len;
    double phase;
    double ph_incr;
} sampler_susp_node, *sampler_susp_type;


void sampler_s_fetch(register sampler_susp_type susp, snd_list_type snd_list)
{
    int cnt = 0; /* how many samples computed */
    int togo;
    int n;
    sample_block_type out;
    register sample_block_values_type out_ptr;

    register sample_block_values_type out_ptr_reg;

    register double loop_to_reg;
    register sample_type * table_ptr_reg;
    register double table_len_reg;
    register double phase_reg;
    register double ph_incr_reg;
    register sample_type s_fm_scale_reg = susp->s_fm->scale;
    register sample_block_values_type s_fm_ptr_reg;
    falloc_sample_block(out, "sampler_s_fetch");
    out_ptr = out->samples;
    snd_list->block = out;

    while (cnt < max_sample_block_len) { /* outer loop */
    /* first compute how many samples to generate in inner loop: */
    /* don't overflow the output sample block: */
    togo = max_sample_block_len - cnt;

    /* don't run past the s_fm input sample block: */
    susp_check_term_log_samples(s_fm, s_fm_ptr, s_fm_cnt);
    togo = min(togo, susp->s_fm_cnt);

    /* don't run past terminate time */
    if (susp->terminate_cnt != UNKNOWN &&
        susp->terminate_cnt <= susp->susp.current + cnt + togo) {
        togo = susp->terminate_cnt - (susp->susp.current + cnt);
        if (togo == 0) break;
    }


    /* don't run past logical stop time */
    if (!susp->logically_stopped && susp->susp.log_stop_cnt != UNKNOWN) {
        int to_stop = susp->susp.log_stop_cnt - (susp->susp.current + cnt);
        /* break if to_stop == 0 (we're at the logical stop)
         * AND cnt > 0 (we're not at the beginning of the
         * output block).
         */
        if (to_stop < togo) {
        if (to_stop == 0) {
            if (cnt) {
            togo = 0;
            break;
            } else /* keep togo as is: since cnt == 0, we
                    * can set the logical stop flag on this
                    * output block
                    */
            susp->logically_stopped = true;
        } else /* limit togo so we can start a new
                * block at the LST
                */
            togo = to_stop;
        }
    }

    n = togo;
    loop_to_reg = susp->loop_to;
    table_ptr_reg = susp->table_ptr;
    table_len_reg = susp->table_len;
    phase_reg = susp->phase;
    ph_incr_reg = susp->ph_incr;
    s_fm_ptr_reg = susp->s_fm_ptr;
    out_ptr_reg = out_ptr;
    if (n) do { /* the inner sample computation loop */
        long table_index;
        double x1;
table_index = (long) phase_reg;
        x1 = table_ptr_reg[table_index];
        *out_ptr_reg++ = (sample_type) (x1 + (phase_reg - table_index) * 
              (table_ptr_reg[table_index + 1] - x1));
        phase_reg += ph_incr_reg + (s_fm_scale_reg * *s_fm_ptr_reg++);
        while (phase_reg > table_len_reg) phase_reg -= (table_len_reg - loop_to_reg);
        /* watch out for negative frequencies! */
        while (phase_reg < loop_to_reg) phase_reg += (table_len_reg - loop_to_reg);
    } while (--n); /* inner loop */

    susp->phase = phase_reg;
    /* using s_fm_ptr_reg is a bad idea on RS/6000: */
    susp->s_fm_ptr += togo;
    out_ptr += togo;
    susp_took(s_fm_cnt, togo);
    cnt += togo;
    } /* outer loop */

    /* test for termination */
    if (togo == 0 && cnt == 0) {
    snd_list_terminate(snd_list);
    } else {
    snd_list->block_len = cnt;
    susp->susp.current += cnt;
    }
    /* test for logical stop */
    if (susp->logically_stopped) {
    snd_list->logically_stopped = true;
    } else if (susp->susp.log_stop_cnt == susp->susp.current) {
    susp->logically_stopped = true;
    }
} /* sampler_s_fetch */


void sampler_i_fetch(register sampler_susp_type susp, snd_list_type snd_list)
{
    int cnt = 0; /* how many samples computed */
    int togo;
    int n;
    sample_block_type out;
    register sample_block_values_type out_ptr;

    register sample_block_values_type out_ptr_reg;

    register double loop_to_reg;
    register sample_type * table_ptr_reg;
    register double table_len_reg;
    register double phase_reg;
    register double ph_incr_reg;
    register double s_fm_pHaSe_iNcR_rEg = susp->s_fm_pHaSe_iNcR;
    register double s_fm_pHaSe_ReG;
    register sample_type s_fm_x1_sample_reg;
    falloc_sample_block(out, "sampler_i_fetch");
    out_ptr = out->samples;
    snd_list->block = out;

    /* make sure sounds are primed with first values */
    if (!susp->started) {
    susp->started = true;
    susp_check_term_log_samples(s_fm, s_fm_ptr, s_fm_cnt);
    susp->s_fm_x1_sample = susp_fetch_sample(s_fm, s_fm_ptr, s_fm_cnt);
    }

    while (cnt < max_sample_block_len) { /* outer loop */
    /* first compute how many samples to generate in inner loop: */
    /* don't overflow the output sample block: */
    togo = max_sample_block_len - cnt;

    /* don't run past terminate time */
    if (susp->terminate_cnt != UNKNOWN &&
        susp->terminate_cnt <= susp->susp.current + cnt + togo) {
        togo = susp->terminate_cnt - (susp->susp.current + cnt);
        if (togo == 0) break;
    }


    /* don't run past logical stop time */
    if (!susp->logically_stopped && susp->susp.log_stop_cnt != UNKNOWN) {
        int to_stop = susp->susp.log_stop_cnt - (susp->susp.current + cnt);
        /* break if to_stop == 0 (we're at the logical stop)
         * AND cnt > 0 (we're not at the beginning of the
         * output block).
         */
        if (to_stop < togo) {
        if (to_stop == 0) {
            if (cnt) {
            togo = 0;
            break;
            } else /* keep togo as is: since cnt == 0, we
                    * can set the logical stop flag on this
                    * output block
                    */
            susp->logically_stopped = true;
        } else /* limit togo so we can start a new
                * block at the LST
                */
            togo = to_stop;
        }
    }

    n = togo;
    loop_to_reg = susp->loop_to;
    table_ptr_reg = susp->table_ptr;
    table_len_reg = susp->table_len;
    phase_reg = susp->phase;
    ph_incr_reg = susp->ph_incr;
    s_fm_pHaSe_ReG = susp->s_fm_pHaSe;
    s_fm_x1_sample_reg = susp->s_fm_x1_sample;
    out_ptr_reg = out_ptr;
    if (n) do { /* the inner sample computation loop */
        long table_index;
        double x1;
        if (s_fm_pHaSe_ReG >= 1.0) {
/* fixup-depends s_fm */
        /* pick up next sample as s_fm_x1_sample: */
        susp->s_fm_ptr++;
        susp_took(s_fm_cnt, 1);
        s_fm_pHaSe_ReG -= 1.0;
        susp_check_term_log_samples_break(s_fm, s_fm_ptr, s_fm_cnt, s_fm_x1_sample_reg);
        s_fm_x1_sample_reg = susp_current_sample(s_fm, s_fm_ptr);
        }
table_index = (long) phase_reg;
        x1 = table_ptr_reg[table_index];
        *out_ptr_reg++ = (sample_type) (x1 + (phase_reg - table_index) * 
              (table_ptr_reg[table_index + 1] - x1));
        phase_reg += ph_incr_reg + s_fm_x1_sample_reg;
        while (phase_reg > table_len_reg) phase_reg -= (table_len_reg - loop_to_reg);
        /* watch out for negative frequencies! */
        while (phase_reg < loop_to_reg) phase_reg += (table_len_reg - loop_to_reg);
        s_fm_pHaSe_ReG += s_fm_pHaSe_iNcR_rEg;
    } while (--n); /* inner loop */

    togo -= n;
    susp->phase = phase_reg;
    susp->s_fm_pHaSe = s_fm_pHaSe_ReG;
    susp->s_fm_x1_sample = s_fm_x1_sample_reg;
    out_ptr += togo;
    cnt += togo;
    } /* outer loop */

    /* test for termination */
    if (togo == 0 && cnt == 0) {
    snd_list_terminate(snd_list);
    } else {
    snd_list->block_len = cnt;
    susp->susp.current += cnt;
    }
    /* test for logical stop */
    if (susp->logically_stopped) {
    snd_list->logically_stopped = true;
    } else if (susp->susp.log_stop_cnt == susp->susp.current) {
    susp->logically_stopped = true;
    }
} /* sampler_i_fetch */


void sampler_r_fetch(register sampler_susp_type susp, snd_list_type snd_list)
{
    int cnt = 0; /* how many samples computed */
    sample_type s_fm_val;
    int togo;
    int n;
    sample_block_type out;
    register sample_block_values_type out_ptr;

    register sample_block_values_type out_ptr_reg;

    register double loop_to_reg;
    register sample_type * table_ptr_reg;
    register double table_len_reg;
    register double phase_reg;
    register double ph_incr_reg;
    falloc_sample_block(out, "sampler_r_fetch");
    out_ptr = out->samples;
    snd_list->block = out;

    /* make sure sounds are primed with first values */
    if (!susp->started) {
    susp->started = true;
    susp->s_fm_pHaSe = 1.0;
    }

    susp_check_term_log_samples(s_fm, s_fm_ptr, s_fm_cnt);

    while (cnt < max_sample_block_len) { /* outer loop */
    /* first compute how many samples to generate in inner loop: */
    /* don't overflow the output sample block: */
    togo = max_sample_block_len - cnt;

    /* grab next s_fm_x1_sample when phase goes past 1.0; */
    /* use s_fm_n (computed below) to avoid roundoff errors: */
    if (susp->s_fm_n <= 0) {
        susp_check_term_log_samples(s_fm, s_fm_ptr, s_fm_cnt);
        susp->s_fm_x1_sample = susp_fetch_sample(s_fm, s_fm_ptr, s_fm_cnt);
        susp->s_fm_pHaSe -= 1.0;
        /* s_fm_n gets number of samples before phase exceeds 1.0: */
        susp->s_fm_n = (long) ((1.0 - susp->s_fm_pHaSe) *
                    susp->output_per_s_fm);
    }
    togo = min(togo, susp->s_fm_n);
    s_fm_val = susp->s_fm_x1_sample;
    /* don't run past terminate time */
    if (susp->terminate_cnt != UNKNOWN &&
        susp->terminate_cnt <= susp->susp.current + cnt + togo) {
        togo = susp->terminate_cnt - (susp->susp.current + cnt);
        if (togo == 0) break;
    }


    /* don't run past logical stop time */
    if (!susp->logically_stopped && susp->susp.log_stop_cnt != UNKNOWN) {
        int to_stop = susp->susp.log_stop_cnt - (susp->susp.current + cnt);
        /* break if to_stop == 0 (we're at the logical stop)
         * AND cnt > 0 (we're not at the beginning of the
         * output block).
         */
        if (to_stop < togo) {
        if (to_stop == 0) {
            if (cnt) {
            togo = 0;
            break;
            } else /* keep togo as is: since cnt == 0, we
                    * can set the logical stop flag on this
                    * output block
                    */
            susp->logically_stopped = true;
        } else /* limit togo so we can start a new
                * block at the LST
                */
            togo = to_stop;
        }
    }

    n = togo;
    loop_to_reg = susp->loop_to;
    table_ptr_reg = susp->table_ptr;
    table_len_reg = susp->table_len;
    phase_reg = susp->phase;
    ph_incr_reg = susp->ph_incr;
    out_ptr_reg = out_ptr;
    if (n) do { /* the inner sample computation loop */
        long table_index;
        double x1;
table_index = (long) phase_reg;
        x1 = table_ptr_reg[table_index];
        *out_ptr_reg++ = (sample_type) (x1 + (phase_reg - table_index) * 
              (table_ptr_reg[table_index + 1] - x1));
        phase_reg += ph_incr_reg + s_fm_val;
        while (phase_reg > table_len_reg) phase_reg -= (table_len_reg - loop_to_reg);
        /* watch out for negative frequencies! */
        while (phase_reg < loop_to_reg) phase_reg += (table_len_reg - loop_to_reg);
    } while (--n); /* inner loop */

    susp->phase = phase_reg;
    out_ptr += togo;
    susp->s_fm_pHaSe += togo * susp->s_fm_pHaSe_iNcR;
    susp->s_fm_n -= togo;
    cnt += togo;
    } /* outer loop */

    /* test for termination */
    if (togo == 0 && cnt == 0) {
    snd_list_terminate(snd_list);
    } else {
    snd_list->block_len = cnt;
    susp->susp.current += cnt;
    }
    /* test for logical stop */
    if (susp->logically_stopped) {
    snd_list->logically_stopped = true;
    } else if (susp->susp.log_stop_cnt == susp->susp.current) {
    susp->logically_stopped = true;
    }
} /* sampler_r_fetch */


void sampler_toss_fetch(susp, snd_list)
  register sampler_susp_type susp;
  snd_list_type snd_list;
{
    long final_count = susp->susp.toss_cnt;
    time_type final_time = susp->susp.t0;
    long n;

    /* fetch samples from s_fm up to final_time for this block of zeros */
    while ((round((final_time - susp->s_fm->t0) * susp->s_fm->sr)) >=
       susp->s_fm->current)
    susp_get_samples(s_fm, s_fm_ptr, s_fm_cnt);
    /* convert to normal processing when we hit final_count */
    /* we want each signal positioned at final_time */
    n = round((final_time - susp->s_fm->t0) * susp->s_fm->sr -
         (susp->s_fm->current - susp->s_fm_cnt));
    susp->s_fm_ptr += n;
    susp_took(s_fm_cnt, n);
    susp->susp.fetch = susp->susp.keep_fetch;
    (*(susp->susp.fetch))(susp, snd_list);
}


void sampler_mark(sampler_susp_type susp)
{
    sound_xlmark(susp->s_fm);
}


void sampler_free(sampler_susp_type susp)
{
    table_unref(susp->the_table);
    sound_unref(susp->s_fm);
    ffree_generic(susp, sizeof(sampler_susp_node), "sampler_free");
}


void sampler_print_tree(sampler_susp_type susp, int n)
{
    indent(n);
    stdputstr("s_fm:");
    sound_print_tree_1(susp->s_fm, n);
}


sound_type snd_make_sampler(sound_type s, double step, double loop_start, rate_type sr, double hz, time_type t0, sound_type s_fm, long npoints)
{
    register sampler_susp_type susp;
    /* sr specified as input parameter */
    /* t0 specified as input parameter */
    int interp_desc = 0;
    sample_type scale_factor = 1.0F;
    time_type t0_min = t0;
    falloc_generic(susp, sampler_susp_node, "snd_make_sampler");
    susp->loop_to = loop_start * s->sr;
    susp->the_table = sound_to_table(s);
    susp->table_ptr = susp->the_table->samples;
    susp->table_len = susp->the_table->length;
    { long index = (long) susp->loop_to;
      double frac = susp->loop_to - index;
      susp->table_ptr[round(susp->table_len)] = /* copy interpolated start to last entry */
      (sample_type) (susp->table_ptr[index] * (1.0 - frac) + 
             susp->table_ptr[index + 1] * frac);};
    susp->phase = 0.0;
    susp->ph_incr = (s->sr / sr) * hz / step_to_hz(step);
    s_fm->scale = (sample_type) (s_fm->scale * (susp->ph_incr / hz));

    /* select a susp fn based on sample rates */
    interp_desc = (interp_desc << 2) + interp_style(s_fm, sr);
    switch (interp_desc) {
      case INTERP_n: /* handled below */
      case INTERP_s: susp->susp.fetch = sampler_s_fetch; break;
      case INTERP_i: susp->susp.fetch = sampler_i_fetch; break;
      case INTERP_r: susp->susp.fetch = sampler_r_fetch; break;
    }

    susp->terminate_cnt = UNKNOWN;
    /* handle unequal start times, if any */
    if (t0 < s_fm->t0) sound_prepend_zeros(s_fm, t0);
    /* minimum start time over all inputs: */
    t0_min = min(s_fm->t0, t0);
    /* how many samples to toss before t0: */
    susp->susp.toss_cnt = (long) ((t0 - t0_min) * sr + 0.5);
    if (susp->susp.toss_cnt > 0) {
    susp->susp.keep_fetch = susp->susp.fetch;
    susp->susp.fetch = sampler_toss_fetch;
    }

    /* initialize susp state */
    susp->susp.free = sampler_free;
    susp->susp.sr = sr;
    susp->susp.t0 = t0;
    susp->susp.mark = sampler_mark;
    susp->susp.print_tree = sampler_print_tree;
    susp->susp.name = "sampler";
    susp->logically_stopped = false;
    susp->susp.log_stop_cnt = logical_stop_cnt_cvt(s_fm);
    susp->started = false;
    susp->susp.current = 0;
    susp->s_fm = s_fm;
    susp->s_fm_cnt = 0;
    susp->s_fm_pHaSe = 0.0;
    susp->s_fm_pHaSe_iNcR = s_fm->sr / sr;
    susp->s_fm_n = 0;
    susp->output_per_s_fm = sr / s_fm->sr;
    return sound_create((snd_susp_type)susp, t0, sr, scale_factor);
}


sound_type snd_sampler(sound_type s, double step, double loop_start, rate_type sr, double hz, time_type t0, sound_type s_fm, long npoints)
{
    sound_type s_fm_copy = sound_copy(s_fm);
    return snd_make_sampler(s, step, loop_start, sr, hz, t0, s_fm_copy, npoints);
}
