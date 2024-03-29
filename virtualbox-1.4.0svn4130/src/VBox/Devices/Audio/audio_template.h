/*
 * QEMU Audio subsystem header
 *
 * Copyright (c) 2005 Vassili Karpov (malc)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifdef DAC
#define NAME "playback"
#define HWBUF hw->mix_buf
#define TYPE out
#define HW HWVoiceOut
#define SW SWVoiceOut
#else
#define NAME "capture"
#define TYPE in
#define HW HWVoiceIn
#define SW SWVoiceIn
#define HWBUF hw->conv_buf
#endif

static void glue (audio_init_nb_voices_, TYPE) (
    AudioState *s,
    struct audio_driver *drv
    )
{
    int max_voices = glue (drv->max_voices_, TYPE);
    int voice_size = glue (drv->voice_size_, TYPE);

    if (glue (s->nb_hw_voices_, TYPE) > max_voices) {
        if (!max_voices) {
#ifdef DAC
            dolog ("Driver `%s' does not support " NAME "\n", drv->name);
#endif
        }
        else {
            dolog ("Driver `%s' does not support %d " NAME " voices, max %d\n",
                   drv->name,
                   glue (s->nb_hw_voices_, TYPE),
                   max_voices);
        }
        glue (s->nb_hw_voices_, TYPE) = max_voices;
    }

    if (audio_bug (AUDIO_FUNC, !voice_size && max_voices)) {
        dolog ("drv=`%s' voice_size=0 max_voices=%d\n",
               drv->name, max_voices);
        glue (s->nb_hw_voices_, TYPE) = 0;
    }

    if (audio_bug (AUDIO_FUNC, voice_size && !max_voices)) {
        dolog ("drv=`%s' voice_size=%d max_voices=0\n",
               drv->name, voice_size);
    }
}

static void glue (audio_pcm_hw_free_resources_, TYPE) (HW *hw)
{
    if (HWBUF) {
        qemu_free (HWBUF);
    }

    HWBUF = NULL;
}

static int glue (audio_pcm_hw_alloc_resources_, TYPE) (HW *hw)
{
    HWBUF = audio_calloc (AUDIO_FUNC, hw->samples, sizeof (st_sample_t));
    if (!HWBUF) {
        dolog ("Could not allocate " NAME " buffer (%d samples)\n",
               hw->samples);
        return -1;
    }

    return 0;
}

static void glue (audio_pcm_sw_free_resources_, TYPE) (SW *sw)
{
    if (sw->buf) {
        qemu_free (sw->buf);
    }

    if (sw->rate) {
        st_rate_stop (sw->rate);
    }

    sw->buf = NULL;
    sw->rate = NULL;
}

static int glue (audio_pcm_sw_alloc_resources_, TYPE) (SW *sw)
{
    int samples;

#ifdef DAC
    samples = sw->hw->samples;
#else
    samples = ((int64_t) sw->hw->samples << 32) / sw->ratio;
#endif

    sw->buf = audio_calloc (AUDIO_FUNC, samples, sizeof (st_sample_t));
    if (!sw->buf) {
        dolog ("Could not allocate buffer for `%s' (%d samples)\n",
               SW_NAME (sw), samples);
        return -1;
    }

#ifdef DAC
    sw->rate = st_rate_start (sw->info.freq, sw->hw->info.freq);
#else
    sw->rate = st_rate_start (sw->hw->info.freq, sw->info.freq);
#endif
    if (!sw->rate) {
        qemu_free (sw->buf);
        sw->buf = NULL;
        return -1;
    }
    return 0;
}

static int glue (audio_pcm_sw_init_, TYPE) (
    SW *sw,
    HW *hw,
    const char *name,
    audsettings_t *as
    )
{
    int err;

    audio_pcm_init_info (&sw->info, as);
    sw->hw = hw;
    sw->active = 0;
#ifdef DAC
    sw->ratio = ((int64_t) sw->hw->info.freq << 32) / sw->info.freq;
    sw->total_hw_samples_mixed = 0;
    sw->empty = 1;
#else
    sw->ratio = ((int64_t) sw->info.freq << 32) / sw->hw->info.freq;
#endif

#ifdef DAC
    sw->conv = mixeng_conv
#else
    sw->clip = mixeng_clip
#endif
        [sw->info.nchannels == 2]
        [sw->info.sign]
        [sw->info.swap_endianness]
        [sw->info.bits == 16];

    sw->name = qemu_strdup (name);
    err = glue (audio_pcm_sw_alloc_resources_, TYPE) (sw);
    if (err) {
        qemu_free (sw->name);
        sw->name = NULL;
    }
    return err;
}

static void glue (audio_pcm_sw_fini_, TYPE) (SW *sw)
{
    glue (audio_pcm_sw_free_resources_, TYPE) (sw);
    if (sw->name) {
        qemu_free (sw->name);
        sw->name = NULL;
    }
}

static void glue (audio_pcm_hw_add_sw_, TYPE) (HW *hw, SW *sw)
{
    LIST_INSERT_HEAD (&hw->sw_head, sw, entries);
}

static void glue (audio_pcm_hw_del_sw_, TYPE) (SW *sw)
{
    LIST_REMOVE (sw, entries);
}

static void glue (audio_pcm_hw_gc_, TYPE) (AudioState *s, HW **hwp)
{
    HW *hw = *hwp;

    if (!hw->sw_head.lh_first) {
#ifdef DAC
        audio_detach_capture (hw);
#endif
        LIST_REMOVE (hw, entries);
        glue (s->nb_hw_voices_, TYPE) += 1;
        glue (audio_pcm_hw_free_resources_ ,TYPE) (hw);
        glue (hw->pcm_ops->fini_, TYPE) (hw);
        qemu_free (hw);
        *hwp = NULL;
    }
}

static HW *glue (audio_pcm_hw_find_any_, TYPE) (AudioState *s, HW *hw)
{
    return hw ? hw->entries.le_next : s->glue (hw_head_, TYPE).lh_first;
}

static HW *glue (audio_pcm_hw_find_any_enabled_, TYPE) (AudioState *s, HW *hw)
{
    while ((hw = glue (audio_pcm_hw_find_any_, TYPE) (s, hw))) {
        if (hw->enabled) {
            return hw;
        }
    }
    return NULL;
}

static HW *glue (audio_pcm_hw_find_specific_, TYPE) (
    AudioState *s,
    HW *hw,
    audsettings_t *as
    )
{
    while ((hw = glue (audio_pcm_hw_find_any_, TYPE) (s, hw))) {
        if (audio_pcm_info_eq (&hw->info, as)) {
            return hw;
        }
    }
    return NULL;
}

static HW *glue (audio_pcm_hw_add_new_, TYPE) (AudioState *s, audsettings_t *as)
{
    HW *hw;
    struct audio_driver *drv = s->drv;

    if (!glue (s->nb_hw_voices_, TYPE)) {
        return NULL;
    }

    if (audio_bug (AUDIO_FUNC, !drv)) {
        dolog ("No host audio driver\n");
        return NULL;
    }

    if (audio_bug (AUDIO_FUNC, !drv->pcm_ops)) {
        dolog ("Host audio driver without pcm_ops\n");
        return NULL;
    }

    hw = audio_calloc (AUDIO_FUNC, 1, glue (drv->voice_size_, TYPE));
    if (!hw) {
        dolog ("Can not allocate voice `%s' size %d\n",
               drv->name, glue (drv->voice_size_, TYPE));
        return NULL;
    }

    hw->pcm_ops = drv->pcm_ops;
    LIST_INIT (&hw->sw_head);

#ifdef DAC
    LIST_INIT (&hw->cap_head);
#endif
    if (glue (hw->pcm_ops->init_, TYPE) (hw, as)) {
        goto err0;
    }

    if (audio_bug (AUDIO_FUNC, hw->samples <= 0)) {
        dolog ("hw->samples=%d\n", hw->samples);
        goto err1;
    }

#ifdef DAC
    hw->clip = mixeng_clip
#else
    hw->conv = mixeng_conv
#endif
        [hw->info.nchannels == 2]
        [hw->info.sign]
        [hw->info.swap_endianness]
        [hw->info.bits == 16];

    if (glue (audio_pcm_hw_alloc_resources_, TYPE) (hw)) {
        goto err1;
    }

    LIST_INSERT_HEAD (&s->glue (hw_head_, TYPE), hw, entries);
    glue (s->nb_hw_voices_, TYPE) -= 1;
#ifdef DAC
    audio_attach_capture (s, hw);
#endif
    return hw;

 err1:
    glue (hw->pcm_ops->fini_, TYPE) (hw);
 err0:
    qemu_free (hw);
    return NULL;
}

static HW *glue (audio_pcm_hw_add_, TYPE) (AudioState *s, audsettings_t *as)
{
    HW *hw;

    if (glue (conf.fixed_, TYPE).enabled && glue (conf.fixed_, TYPE).greedy) {
        hw = glue (audio_pcm_hw_add_new_, TYPE) (s, as);
        if (hw) {
            return hw;
        }
    }

    hw = glue (audio_pcm_hw_find_specific_, TYPE) (s, NULL, as);
    if (hw) {
        return hw;
    }

    hw = glue (audio_pcm_hw_add_new_, TYPE) (s, as);
    if (hw) {
        return hw;
    }

    return glue (audio_pcm_hw_find_any_, TYPE) (s, NULL);
}

static SW *glue (audio_pcm_create_voice_pair_, TYPE) (
    AudioState *s,
    const char *sw_name,
    audsettings_t *as
    )
{
    SW *sw;
    HW *hw;
    audsettings_t hw_as;

    if (glue (conf.fixed_, TYPE).enabled) {
        hw_as = glue (conf.fixed_, TYPE).settings;
    }
    else {
        hw_as = *as;
    }

    sw = audio_calloc (AUDIO_FUNC, 1, sizeof (*sw));
    if (!sw) {
#if defined __STDC_VERSION__ && __STDC_VERSION__ > 199901L
        dolog ("Could not allocate soft voice `%s' (%zu bytes)\n",
               sw_name ? sw_name : "unknown", sizeof (*sw));
#else
        dolog ("Could not allocate soft voice `%s' (%u bytes)\n",
               sw_name ? sw_name : "unknown", sizeof (*sw));
#endif
        goto err1;
    }

    hw = glue (audio_pcm_hw_add_, TYPE) (s, &hw_as);
    if (!hw) {
        goto err2;
    }

    glue (audio_pcm_hw_add_sw_, TYPE) (hw, sw);

    if (glue (audio_pcm_sw_init_, TYPE) (sw, hw, sw_name, as)) {
        goto err3;
    }

    return sw;

err3:
    glue (audio_pcm_hw_del_sw_, TYPE) (sw);
    glue (audio_pcm_hw_gc_, TYPE) (s, &hw);
err2:
    qemu_free (sw);
err1:
    return NULL;
}

static void glue (audio_close_, TYPE) (AudioState *s, SW *sw)
{
    glue (audio_pcm_sw_fini_, TYPE) (sw);
    glue (audio_pcm_hw_del_sw_, TYPE) (sw);
    glue (audio_pcm_hw_gc_, TYPE) (s, &sw->hw);
    qemu_free (sw);
}

void glue (AUD_close_, TYPE) (QEMUSoundCard *card, SW *sw)
{
    if (sw) {
        if (audio_bug (AUDIO_FUNC, !card || !card->audio)) {
            dolog ("card=%p card->audio=%p\n",
                   (void *) card, card ? (void *) card->audio : NULL);
            return;
        }

        glue (audio_close_, TYPE) (card->audio, sw);
    }
}

SW *glue (AUD_open_, TYPE) (
    QEMUSoundCard *card,
    SW *sw,
    const char *name,
    void *callback_opaque ,
    audio_callback_fn_t callback_fn,
    audsettings_t *as
    )
{
    AudioState *s;
#ifdef DAC
    int live = 0;
    SW *old_sw = NULL;
#endif

    ldebug ("open %s, freq %d, nchannels %d, fmt %d\n",
            name, as->freq, as->nchannels, as->fmt);

    if (audio_bug (AUDIO_FUNC,
                   !card || !card->audio || !name || !callback_fn || !as)) {
        dolog ("card=%p card->audio=%p name=%p callback_fn=%p as=%p\n",
               (void *) card, card ? (void *) card->audio : NULL,
               name,
               (void *) callback_fn,
               (void *) as);
        goto fail;
    }

    s = card->audio;

    if (audio_bug (AUDIO_FUNC, audio_validate_settings (as))) {
        audio_print_settings (as);
        goto fail;
    }

    if (audio_bug (AUDIO_FUNC, !s->drv)) {
        dolog ("Can not open `%s' (no host audio driver)\n", name);
        goto fail;
    }

    if (sw && audio_pcm_info_eq (&sw->info, as)) {
        return sw;
    }

#ifdef DAC
    if (conf.plive && sw && (!sw->active && !sw->empty)) {
        live = sw->total_hw_samples_mixed;

#ifdef DEBUG_PLIVE
        dolog ("Replacing voice %s with %d live samples\n", SW_NAME (sw), live);
        dolog ("Old %s freq %d, bits %d, channels %d\n",
               SW_NAME (sw), sw->info.freq, sw->info.bits, sw->info.nchannels);
        dolog ("New %s freq %d, bits %d, channels %d\n",
               name,
               freq,
               (fmt == AUD_FMT_S16 || fmt == AUD_FMT_U16) ? 16 : 8,
               nchannels);
#endif

        if (live) {
            old_sw = sw;
            old_sw->callback.fn = NULL;
            sw = NULL;
        }
    }
#endif

    if (!glue (conf.fixed_, TYPE).enabled && sw) {
        glue (AUD_close_, TYPE) (card, sw);
        sw = NULL;
    }

    if (sw) {
        HW *hw = sw->hw;

        if (!hw) {
            dolog ("Internal logic error voice `%s' has no hardware store\n",
                   SW_NAME (sw));
            goto fail;
        }

        glue (audio_pcm_sw_fini_, TYPE) (sw);
        if (glue (audio_pcm_sw_init_, TYPE) (sw, hw, name, as)) {
            goto fail;
        }
    }
    else {
        sw = glue (audio_pcm_create_voice_pair_, TYPE) (s, name, as);
        if (!sw) {
            dolog ("Failed to create voice `%s'\n", name);
            return NULL;
        }
    }

    if (sw) {
        sw->vol = nominal_volume;
        sw->callback.fn = callback_fn;
        sw->callback.opaque = callback_opaque;

#ifdef DAC
        if (live) {
            int mixed =
                (live << old_sw->info.shift)
                * old_sw->info.bytes_per_second
                / sw->info.bytes_per_second;

#ifdef DEBUG_PLIVE
            dolog ("Silence will be mixed %d\n", mixed);
#endif
            sw->total_hw_samples_mixed += mixed;
        }
#endif

#ifdef DEBUG_AUDIO
        dolog ("%s\n", name);
        audio_pcm_print_info ("hw", &sw->hw->info);
        audio_pcm_print_info ("sw", &sw->info);
#endif
    }

    return sw;

 fail:
    glue (AUD_close_, TYPE) (card, sw);
    return NULL;
}

int glue (AUD_is_active_, TYPE) (SW *sw)
{
    return sw ? sw->active : 0;
}

void glue (AUD_init_time_stamp_, TYPE) (SW *sw, QEMUAudioTimeStamp *ts)
{
    if (!sw) {
        return;
    }

    ts->old_ts = sw->hw->ts_helper;
}

uint64_t glue (AUD_get_elapsed_usec_, TYPE) (SW *sw, QEMUAudioTimeStamp *ts)
{
    uint64_t delta, cur_ts, old_ts;

    if (!sw) {
        return 0;
    }

    cur_ts = sw->hw->ts_helper;
    old_ts = ts->old_ts;
    /* dolog ("cur %lld old %lld\n", cur_ts, old_ts); */

    if (cur_ts >= old_ts) {
        delta = cur_ts - old_ts;
    }
    else {
        delta = UINT64_MAX - old_ts + cur_ts;
    }

    if (!delta) {
        return 0;
    }

    return (delta * sw->hw->info.freq) / 1000000;
}

#undef TYPE
#undef HW
#undef SW
#undef HWBUF
#undef NAME
