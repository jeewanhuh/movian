/*
 *  Audio framework
 *  Copyright (C) 2007 Andreas �man
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "showtime.h"
#include "audio.h"
#include "audio_ui.h"
#include "audio_fifo.h"
#include "audio_decoder.h"

#include <libglw/glw.h>
#include "app.h"

audio_mode_t *audio_mode_current;
pthread_mutex_t audio_mode_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct audio_mode_queue audio_modes;

static void *audio_output_thread(void *aux);

/**
 *
 */
void
audio_init(void)
{
  pthread_t ptid;

  TAILQ_INIT(&audio_modes);
  audio_decoder_init();
  audio_alsa_init();

  audio_widget_make();

  pthread_create(&ptid, NULL, audio_output_thread, NULL);
}


/**
 *
 */
audio_fifo_t *thefifo;
audio_fifo_t af0;

static void *
audio_output_thread(void *aux)
{
  audio_mode_t *am;
  int r;
  audio_fifo_t *af = &af0;

  audio_fifo_init(af, 16000, 8000);
  thefifo = af;

  am = TAILQ_FIRST(&audio_modes);
  audio_mode_current = am;

  printf("And there it was assigned\n");

  while(1) {
    am = audio_mode_current;
    printf("Audio output using %s\n", am->am_title);
    r = am->am_entry(am, af);

    if(r == -1) {
      /* Device error, sleep to avoid busy looping.
	 Hopefully the error will resolve itself (if another app
	 is blocking output, etc)
      */
      sleep(1);
    }
  }
  return NULL;
}


/**
 *
 */
void
audio_mode_register(audio_mode_t *am)
{
  TAILQ_INSERT_TAIL(&audio_modes, am, am_link);
}




/**
 *
 */
static void
audio_mode_change(void *opaque, int value)
{
  audio_mode_current = opaque;
}

/**
 *
 */
static void
audio_opt_int_cb(void *opaque, int value)
{
  int *ptr = opaque;
  *ptr = value;
}

/**
 *
 */
static void
audio_add_int_option_on_off(glw_t *l, const char *title, int *ptr)
{
  glw_t *opt, *sel;

  opt = glw_model_create("theme://settings/audio/audio-option.model", l);
  glw_set_caption(opt, "title", title);

  if((sel = glw_find_by_id(opt, "options", 0)) != NULL) {
    glw_selection_add_text_option(sel, "Off", 
				  audio_opt_int_cb, ptr, 0, *ptr == 0);
    glw_selection_add_text_option(sel, "On", 
				  audio_opt_int_cb, ptr, 1, *ptr == 1);
  }
}



/**
 *
 */
static void
audio_mode_add_to_settings(audio_mode_t *am, glw_t *parent)
{
  glw_t *w, *le, *l;
  glw_t *deck;
  char buf[50];
  int multich = am->am_formats & (AM_FORMAT_PCM_5DOT1 | AM_FORMAT_PCM_7DOT1);

  if((w = glw_find_by_id(parent, "outputdevice_list", 0)) == NULL)
    return;

  le = glw_selection_add_text_option(w, am->am_title, audio_mode_change,
				     am, 0, 0);

  snprintf(buf, sizeof(buf), "%s%s%s%s%s",
	   am->am_formats & AM_FORMAT_PCM_STEREO ? "Stereo  " : "",
	   am->am_formats & AM_FORMAT_PCM_5DOT1  ? "5.1  "    : "",
	   am->am_formats & AM_FORMAT_PCM_7DOT1  ? "7.1 "     : "",
	   am->am_formats & AM_FORMAT_AC3        ? "AC3 "     : "",
	   am->am_formats & AM_FORMAT_DTS        ? "DTS "     : "");


  deck = glw_model_create("theme://settings/audio/audio-device-settings.model",
			  NULL);

  glw_set_caption(deck, "output_formats", buf);

  if((l = glw_find_by_id(deck, "controllers", 0)) == NULL)
    return;

  if(multich) {
    audio_add_int_option_on_off(l, "Phantom Center:", &am->am_phantom_center);
    audio_add_int_option_on_off(l, "Phantom LFE:", &am->am_phantom_lfe);
    audio_add_int_option_on_off(l, "Small Front:", &am->am_small_front);
  }

  glw_add_tab(parent, NULL, le, "outputdevice_deck", deck);
}

/**
 *
 */
void
audio_settings_init(appi_t *ai, glw_t *m)
{
  glw_t *icon = glw_model_create("theme://settings/audio/audio-icon.model",
				 NULL);
  glw_t *tab  = glw_model_create("theme://settings/audio/audio.model",
				 NULL);
  audio_mode_t *am;
  glw_t *w;

  glw_add_tab(m, "settings_list", icon, "settings_deck", tab);

  TAILQ_FOREACH(am, &audio_modes, am_link)
    audio_mode_add_to_settings(am, tab);

  if((w = glw_find_by_id(tab, "outputdevice_list", 0)) != NULL) {
    w = glw_selection_get_widget_by_opaque(w, audio_mode_current);
    if(w != NULL)
      glw_select(w);
  }
}
