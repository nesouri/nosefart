/* nsfinfo : get/set nsf file info.
 *
 * by benjamin gerard <ben@sashipa.com> 2003/04/18
 *
 * This program supports nsf playing time calculation extension.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Library General 
 * Public License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 * Library General Public License for more details.  To obtain a 
 * copy of the GNU Library General Public License, write to the Free 
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Any permitted reproduction of these routines, in whole or in part,
 * must bear this legend.
 */

/*
	This file has been modified by Matthew Strait so that it can be
	used to do playing time calculation without using ben's idea of
	changing the NSF file format.  He wrote this as a stand-alone
	program.  I have modified it so that the parts I want are called
	from nosefart.  I also modified the calculation algorithm 
	somewhat.
	
	For this to work, the changes he made to other files in this source
	tree have been left alone.  Therefore, there may be references to 
	the "NSF2" file format in the code, even though this version of
	nosefart does not use it.
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "nes6502.h"
#include "types.h"
#include "nsf.h"

static int quiet = 0;

#ifdef NSF_PLAYER
#define  NES6502_4KBANKS
#endif

#ifdef NES6502_4KBANKS
#define  NES6502_NUMBANKS  16
#define  NES6502_BANKSHIFT 12
#else
#define  NES6502_NUMBANKS  8
#define  NES6502_BANKSHIFT 13
#endif

extern uint8 *acc_nes6502_banks[NES6502_NUMBANKS];
extern int max_access[NES6502_NUMBANKS];

static void info_show_help(void)
{
   printf("\n"
	  "nsfinfo : get and set NSF (NES Sound File) information.\n"
	  "\n"
	  "by Benjamin Gerard (2003/04/18)\n"
	  "\n"
	  "Usage: `nsfinfo nsf-file [COMMAND | OPTION ...]'\n"
	  "\n"
	  "OPTION:\n"
	  " --help     : Display this message and exit\n"
	  " --warranty : Display warranty message and exit\n"
	  " --quiet    : Remove all messages\n"
	  "\n"
	  "COMMANDS:\n"
	  " track-list : Set track list for following commands\n"
	  " --V        : Display format spec version\n"
	  " --F        : Display playback rate in Hz\n"
	  " --B        : Display PAL/NTSC bits\n"
	  " --B=#      : Set PAL/NTSC bits\n"
	  " --T        : Display number of song\n"
	  " --t        : Display current song\n"
	  " --D        : Display default song\n"
	  " --D=#      : Set default song [0=current]\n"
	  " --n        : Display song name\n"
	  " --n=xxx    : Set song name\n"
	  " --a        : Display artist\n"
	  " --a=xxx    : Set artist\n"
	  " --c        : Display copyright\n"
	  " --c=xxx    : Set copyright\n"
	  " --w[=file] : Write NSF file (default is input file)\n"
	  " --nl       : Display a newline char.\n"
	  " --p=STRING : Display STRING.\n"
	  " --Tf       : Display current track time (in frames)\n"
	  " --Ts       : Display current track time (in seconds)\n"
	  " --Tx       : Display current track time (formatted)\n"
	  " --AT       : Launch auto time calculation.\n"
	  " STRING     : Display STRING.\n"
	  "\n"
	  "track-list  : --track[[,track]|[-track]]\n"
	  "\n"
	  "  track-list executes following commands for all listed tracks\n"
	  "  until another track-list or end of commands is reached.\n"
	  "  First track is number 1. 0 is replaced by the number of tracks.\n"
	  "\n"
	  "e.g:\n"
	  "\n"
	  " `nsfinfo file.nsf --1,5,4-6 track: --t ' ' frames: --Tf --nl'\n"
	  "\n"
	  " Works with tracks 1,5,4,5 and 6 in this order and will\n"
	  " display `track:1 frames:2342' for all theses tracks.\n"
	  "\n"
	  "  `--1-0' loops over all tracks.\n" 
	  "\n"
	  );
}

static void msg(const char *fmt, ...)
{
  va_list list;
  if (quiet) 
    return;
  
  va_start(list, fmt);
  vfprintf(stderr, fmt, list);
  va_end(list);
}

static void info_show_warranty(void)
{
  printf("\n"
	 "This software comes with absolutely no warranty. "
	 "You may redistribute this\nprogram under the terms of the GPL. "
	 "(See the file COPYING in the source tree.)\n\n"
	 );
}

static void lpoke(uint8 *p, int v)
{
  p[0] = v;
  p[1] = v >> 8;
  p[2] = v >> 16;
  p[3] = v >> 24;
}

static int nsf_write_file(const char * fname,
			  const nsf_t * header,
			  const char * buffer, int len,
			  unsigned int * timeinfo)
{
  int err = -1;
  FILE *f;
  nsf_t nsf;
  int i, songs;
  unsigned int total_time;

  msg("Creating nsf version 2 [%s]\n", fname);
  f = fopen(fname,"wb");
  if (!f) {
    perror(fname);
    goto error;
  }

  /* Copy header. */
  memcpy(&nsf, header, 128);
  /* Set new version. */
  nsf.version = 2;
  /* Set data length (24 bit). */
  lpoke(nsf.reserved, len & 0xFFFFFF);

  /* Write header. */
  msg(" - Write header\n");
  if (fwrite(&nsf, 1, 128, f) != 128) {
    perror(fname);
    goto error;
  }

  /* Write music data. */
  msg(" - Write data\n");
  if (fwrite(buffer, 1, len, f) != len) {
    perror(fname);
    goto error;
  }

  /* Check for time info */
  songs = nsf.num_songs;
  total_time = 0;
  if (timeinfo) {
    for (i=1; i<=songs; ++i) {
      total_time += timeinfo[i];
    }
    timeinfo[0] = total_time;
  }

  /* Write time info. */
  if (!total_time) {
    msg(" - No time info\n");
  } else {
    static const char * id = "NESMTIME";
    unsigned char word[4];

    /* Compute NESMTIME chunk size. */
    msg(" - Write TIME chunk header (%d songs)\n", songs);
    lpoke(word, 8 + 4 + 4 * (songs+1));
    if (fwrite(id, 1, 8, f) != 8 || fwrite(word, 1, 4, f) != 4) {
      perror(fname);
      goto error;
    }

    for (i=0; i <= songs; ++i) {
      msg(" - Write TIME info #%02d [%u frames]\n", i, timeinfo[i]);
      lpoke(word, timeinfo[i]);
      if (fwrite(word, 1, 4, f) != 4) {
	perror(fname);
	goto error;
      }
    }
  }

  err = 0;
 error:
  if (f) {
    fclose(f);
  }
  return err;
}


static int nsf_playback_rate(nsf_t * nsf)
{
  uint8 * p;
  unsigned int def, v;
  
  
  if (nsf->pal_ntsc_bits & NSF_DEDICATED_PAL) {
    p = (uint8*)&nsf->pal_speed;
    def = 50;
  } else {
    p = (uint8*)&nsf->ntsc_speed;
    def = 60;
  }
  v = p[0] | (p[1]<<8);
  return v ? 1000000 / v : def;
}

static int nsf_inited = 0;

static unsigned int nsf_calc_time(nsf_t * src,
  int len,  int track,  unsigned int frame_frag, int force)
{
  unsigned int result1 = 0, result2 = 0;
  nsf_t * nsf = 0;
  float sec; 
  unsigned int playback_rate = nsf_playback_rate(src);
  int err;
  unsigned int max_frag;

  // trouble with zelda2:7?
  int default_frag_size = 20 * playback_rate; // 2 * 60 * playback_rate; 

  if (track < 0 || track > src->num_songs) {
    fprintf(stderr, "nsfinfo : calc time, track #%d out of range.\n", track);
  }
  /* always called with frame_frag == 0 --matt s. */
  frame_frag = frame_frag ? frame_frag : default_frag_size;
  max_frag = 60 * 60 * playback_rate;

  if (!nsf_inited) {
    //msg("nsfinfo : init nosefart engine...\n");
    if (nsf_init() == -1) {
      fprintf(stderr, "nsfinfo :  init failed.\n");
      goto error;
    }
    nsf_inited = 1;
    //msg("nsfinfo : nosefart engine initialized\n");
  }

  //msg("nsfinfo : loading nsf...\n");
  nsf = nsf_load(0, src, len);
  if (!nsf) {
    fprintf(stderr,"nsfinfo: load failed\n");
    goto error;
  }
  if (!force && nsf->song_frames && nsf->song_frames[track]) {
    result1 = nsf->song_frames[track];
    goto error; /* Not en error :) */
  }

  //msg("nsfinfo : init [%s] #%d...\n", nsf->song_name, track);
  /* ben : Don't care about sound format here,
   * since it will not be rendered.
   */
  err = nsf_playtrack(nsf, track, 8000, 8, 1);
  if (err != track) {
    if (err == -1) {
      /* $$$ ben : Becoz nsf_playtrack() kicks nsf ass :( */
      nsf = 0;
    }
    fprintf(stderr,"nsfinfo: track %d not initialized\n", track);
    goto error;
  }

  /* the way this works is that it finds the last place that new memory 
     is accessed.  The time it takes to do this is the length of the song.
     Well, sorta.  It's the time after which no new material is played.
     If the song has an intro which is not repeated, it counts that as part
     of the length.  (And if a song goes A B C B B B B ..., it will be the
     length of A B C.  I don't think I've encountered this, however, 
     although I think it could happen.)
     -matt s. 
  */
  {
    int done = 0;
    uint32 last_accessed_frame = 0, prev_frag = 0, starting_frame = 0;

    //msg("nsfinfo : Emulating up to %u frames (%d hz)\n", frame_frag, playback_rate);

    while (!done) 
    {
      nsf_frame(nsf); /* advance one frame. -matt s. */

      //msg("%d ", nsf->cur_frame);
      if (nes6502_mem_access)
      {
        //msg("!");
	last_accessed_frame = nsf->cur_frame;
      }
      //msg("\n");

      if (nsf->cur_frame > frame_frag) 
      {
        if (last_accessed_frame > prev_frag) 
        {
	  prev_frag = nsf->cur_frame;
	  frame_frag += default_frag_size;
	  //msg("nsfinfo : memory was accessed, enlarging search to next %u frames\n",
	  //    default_frag_size);
	  
          if (frame_frag >= max_frag) 
          {
	    msg("\nnsfinfo : unable to find end of music within %u frames, giving up!\n", max_frag);
	    goto error;
	  }
	} 
        else 
	  done = 1;
      }
    }
    result1 = last_accessed_frame + 16 /* fudge room */;
    sec = (float)(result1 + nsf->playback_rate - 1) / (float)nsf->playback_rate;
    //printf("track %d with intro is %u frames, %.2f seconds\n",
	//track, result1, sec);

    // doesn't make a difference
    //nsf->cur_frame = last_accessed_frame;
  }

  /* clear out the memory access information.  This is a kludge
     because I, matt s, don't totally understand what ben is doing! 
     max_access information is collected in src/cpu/nes6502/nes6502.c */
  {
    int a;
    for(a = 0; a < NES6502_NUMBANKS; a++)
    {
	//msg("max_access[%d] = %d\n", a, max_access[a]);
  	memset(acc_nes6502_banks[a], 0, max_access[a]);
    }
  }

  //msg("starting second run\n");

  /* This finds the length of the song _without_ the intro. -matt s */
  {
    /* don't want to count what we've already looked at */
    int starting_frame = nsf->cur_frame; 

    int done = 0;
    uint32 last_accessed_frame = 0, prev_frag = 0;

    //msg("nsfinfo : Emulating up to %u frames (%d hz)\n", frame_frag, playback_rate);

    while (!done) 
    {
      nsf_frame(nsf); /* advance one frame. -matt s. */

      //msg("%d ", nsf->cur_frame - starting_frame);
      if (nes6502_mem_access)
      {
        //msg("!");
	last_accessed_frame = nsf->cur_frame;
      }
      //msg("\n");

      if (nsf->cur_frame > frame_frag) 
      {
        if (last_accessed_frame > prev_frag) 
        {
	  prev_frag = nsf->cur_frame;
	  frame_frag += default_frag_size;
	  //msg("nsfinfo : memory was accessed, enlarging search to next %u frames\n",
	  //    default_frag_size);
	  
          if (frame_frag >= max_frag) 
          {
	    msg("\nnsfinfo : unable to find end of music within %u frames\n\tgiving up!", max_frag);
	    goto error;
	  }
	} 
        else 
	  done = 1;
      }
    }
    result2 = last_accessed_frame - starting_frame + 16 /* fudge room */;
    sec = (float)(result2 + nsf->playback_rate - 1) / (float)nsf->playback_rate;
    //printf("track %d without intro is %u frames, %.2f seconds\n",
//	track, result2, sec);
  }

  /* Want to get both results back to nosefart.  Neither should be 
     larger than 2^16, so let's shove them together into one int.
     the high order bits are result1 (with intro) and the lower order
     bits are result2 (without intro) */
	return (result1 * 0x1000 + result2);

 error:
  nsf_free(&nsf);
  fprintf(stderr, "Error with time calculation, bailing out!\n");
  return 0x00000001; /* something small, but non-zero (zero means unlimited) */
}


static char * clean_string(char *d, const char *s, int max)
{
  int i;

  --max;
  for (i=0; i<max && s[i]; ++i) {
    d[i] = s[i];
  }
  for (; i<=max; ++i) {
    d[i] = 0;
  }

  return (char*)d;
}

static int get_integer(const char * arg, const char * opt, int * pv)
{
  char * end;
  long v;

  if (strstr(arg,opt) != arg) {
    return 1;
  }
  arg += strlen(opt);
  end = 0;
  v = strtol(arg, &end, 0);
  if (!end || *end) {
    return -1;
  }
  *pv = v;
  return 0;
}

static int get_string(const char * arg, const char * opt, char * v, int max)
{
  if (strstr(arg,opt) != arg) {
    return 1;
  }
  clean_string(v, arg + strlen(opt), max);
  return 0;
}

static int track_number(char **ps, int max)
{
  int n;
  if (!isdigit(**ps)) {
    return -1;
  }
  n = strtol(*ps, ps, 10);
  if (n<0 || n>max) {
    fprintf(stderr,"nsfinfo : track #%d out of range\n", n);
    return -1;
  } else if (n==0) {
    n = max;
  }
  return n;
}

static int read_track_list(char **trackList, int max, int *from, int *to)
{
  int fromTrack, toTrack;
  char *t = *trackList;

  if (t) {
    /* Skip comma ',' */
    while(*t == ',') {
      ++t;
    }
  }

  /* Done with this list. */
  if (!t || !*t) {
/*     msg("track list finish here\n"); */
    *trackList = 0;
    return 0;
  }

/*   msg("parse:%s\n", t); */
  *trackList = t;
  fromTrack = track_number(trackList, max);

/*   msg("-> %d [%s]\n", fromTrack, *trackList); */

  if (fromTrack < 0) {
    return -1;
  }

  switch(**trackList) {
  case ',': case 0:
    toTrack = fromTrack;
    break;
  case '-':
    (*trackList)++;
    toTrack = track_number(trackList, max);
    if (toTrack < 0) {
      return -2;
    }
    break;
  default:
    return -1;
  }

  *from = fromTrack;
  *to   = toTrack;

/*   msg("from:%d, to:%d [%s]\n", fromTrack, toTrack, *trackList); */

  return 1;
}

int main(int na, char **a)
{
  int i;
  const char * iname;
  char * buffer = 0;
  int len = 0;
  nsf_t * nsf;
  int cursong, err, loopArg, toTrack, curTrack;
  char *trackList, *trackListBase;

  static unsigned int times[256];
 
  /* First loop search for --help, --warranty ,--quiet */
  for (i=1; i<na; ++i) {
    if (!strcmp(a[i],"--help")) {
      info_show_help();
      return 1;
    }
    if (!strcmp(a[i],"--warranty")) {
      info_show_warranty();
      return 1;
    }
    if (!strcmp(a[i],"--quiet")) {
      quiet = 1;
    }
  }

  if (na < 2 || !a[1] || !a[1][0]) {
    info_show_help();
    return 1;
  }
  iname = a[1];

  //msg("Loading [%s] file.\n", iname);
  /* Load whole file. */
  {
    FILE * f = fopen(iname, "rb");
    if (!f) {
      perror(iname);
      return 2;
    }

    if (fseek(f,0,SEEK_END) == -1) {
      perror(iname);
    } else {
      len = ftell(f);
      if (len == -1) {
	perror(iname);
      } else if (len < 128) {
	fprintf(stderr, "nsfinfo : %s not an nsf file (too small).\n", iname);
      } else {
	char tmp_hd[128];
	int err;
	//msg("File length is %d bytes.\n", len);
	err = fseek(f,0,SEEK_SET);
	if (err != -1) {
	  err = fread(tmp_hd, 1, 128, f);
	}
	if (err != 128) {
	  perror(iname);
	} else if (memcmp(tmp_hd,NSF_MAGIC,5)) {
	  fprintf(stderr, "nsfinfo : %s not an nsf file (invalid magic).\n",
		  iname);
	} else {
	  //msg("Looks like a valid NSF file, loading data ...\n");
	  buffer = malloc(len);
	  if (!buffer) {
	    perror(iname);
	  } else {
	    int err;
	    memcpy(buffer, tmp_hd, 128);
	    err = fread(buffer+128, 1, len-128, f);
	    if (err != len-128) {
	      perror(iname);
	      free(buffer);
	      buffer = 0;
	    }
	  }
	}
      }
    }
    fclose(f);
  }
  if (!buffer) {
    return 3;
  }
  //msg("Successfully loaded [%s].\n", iname);

  nsf = (nsf_t *)buffer;
  cursong = nsf->start_song % (nsf->num_songs+1);
  clean_string((char*)nsf->song_name, (char*)nsf->song_name, sizeof(nsf->song_name));
  clean_string((char*)nsf->artist_name, (char*)nsf->artist_name, sizeof(nsf->artist_name));
  clean_string((char*)nsf->copyright, (char*)nsf->copyright, sizeof(nsf->copyright));
  memset(times,0,sizeof(times));

  /* Setup variable */
  toTrack = curTrack = cursong;
  trackList = 0;

  for (i=2; i<=na; ++i) {
    int v, err;
    char * arg = a[i];

    err = 0;
    if (i == na || (arg[0] == '-' && arg[1] == '-' && isdigit(arg[2]))) {
      int res;
      /* Entering track-list */
      if (trackList) {
        /* Already in trackList, finish this one. */
        if (curTrack < toTrack) {
          curTrack++;
          res = 1;
        } else {
          res = read_track_list(&trackList, nsf->num_songs,
				&curTrack , &toTrack);
        }
        if (res < 0) {
	  err = -1;
	  fprintf(stderr, "nsfinfo : invalid track list [%s]\n",
		  trackListBase);
          break;
        } else if (res > 0) {
          /* Must loop */
          i = loopArg;
          continue;
        }
      }
      if (i==na) {
        /* All is over capt'n */
        break;
      }

      /* Previous list is over, start new one */
      trackListBase = trackList = arg+2;
      loopArg = i; /* $$$ or i+1 */

      /* get From track parm */
      res = read_track_list(&trackList, nsf->num_songs, &curTrack , &toTrack);
      if (res < 0) {
	  fprintf(stderr, "nsfinfo : invalid track list [%s]\n",
		  trackListBase);
      } else if (!res) {
        /* This should not happen becoz we check that almost one digit
	 * was there above */
        fprintf(stderr, "noseinfo : Internal error;"
		" program should not reach this point\n");
        err = -1;
	break;
      }
      /*curTrack = toTrack;*/
      continue;
    }
    cursong = curTrack;

    if (0
	|| !strcmp(arg,"--help")
	|| !strcmp(a[i],"--warranty")
	|| !strcmp(a[i],"--quiet")) {
      continue;
    }

    if (!strcmp(arg,"--AT")) {
      unsigned int nf;
      //nf = nsf_calc_time(nsf, len, cursong, 0, 1);
      return nsf_calc_time(nsf, len, cursong, 0, 1);
     if (nf) {
	times[cursong] = nf;
      }

    } else if (!strcmp(arg,"--V")) {
      /* Print spec version. */
      printf("%d", nsf->version);
    } else if (!strcmp(arg,"--B")) {
      /* Print */
      printf("%d", nsf->pal_ntsc_bits);
    } else if (!strcmp(arg,"--F")) {
      /* Print */
      printf("%d", nsf_playback_rate(nsf));
    } else if (err=get_integer(arg,"--B=",&v), !err) {
      /* Set NTSC/PAL bits  */
      nsf->pal_ntsc_bits = v;
    } else if (!strcmp(arg,"--T")) {
      /* Print number of songs. */
      printf("%d", nsf->num_songs);
    } else if (!strcmp(arg,"--t")) {
      /* Display current song. */
      printf("%d", cursong);
    } else if (!strcmp(arg,"--D")) {
      /* Display default song. */
      printf("%d", nsf->start_song);
    } else if (err=get_integer(arg,"--D=",&v), !err) {
      v = (v == 0) ? cursong : v;
      if (v > 0 && v <= nsf->num_songs) {
	nsf->start_song = v;
      }
    } else if (!strcmp(arg,"--Tf")
	       ||!strcmp(arg,"--Ts")
	       || !strcmp(arg,"--Tx")) {
      unsigned int time;

      time = times[cursong]
	? times[cursong]
	: nsf_calc_time(nsf, len, cursong, 0, 0);

      if (!times[cursong] && time) {
	times[cursong] = time;
      }

      if (arg[3] == 'f') {
	printf("%u",time);
      } else {
	unsigned int frame_rate = nsf_playback_rate(nsf);
	unsigned int sec = (time + frame_rate - 1) / frame_rate;
	if (arg[3] == 's') {
	  printf("%u",sec);
	} else {
	  printf("%02u:%02u",sec/60u,sec%60u);
	}
      }
    } else if (!strcmp(arg,"--nl")) {
      fputs("\n",stdout);
    } else if (!strcmp(arg,"--n")) {
      fputs((char*)nsf->song_name,stdout);
    } else if (err = get_string(arg, "--n=",
				(char*)nsf->song_name,
				sizeof(nsf->song_name)), !err) {
      ; /* Nothing more to do. */
    } else if (!strcmp(arg,"--a")) {
      fputs((char*)nsf->artist_name,stdout);
    } else if (err = get_string(arg, "--a=",
				(char*)nsf->artist_name,
				sizeof(nsf->artist_name)), !err) {
      ; /* Nothing more to do. */
    } else if (!strcmp(arg,"--c")) {
      fputs((char*)nsf->copyright,stdout);
    } else if (err = get_string(arg, "--c=",
				(char*)nsf->copyright,
				sizeof(nsf->copyright)), !err) {
      ; /* Nothing more to do. */
    } else if (!strcmp(arg,"--w") || strstr(arg,"--w=") == arg) {
      const char * oname = iname;
      oname = !strcmp(arg,"--w") ? iname : arg + 4;
      err = nsf_write_file(oname, nsf, buffer+128, len-128, times);
    } else if (strstr(arg,"--p=") == arg) {
      fputs(arg+4,stdout);
    } else if (err >= 0) {
      fputs(arg,stdout);
    } else {
      break;
    }

    if (err<0) {
      break;
    }
  }
  free (buffer);
  return (err < 0) ? 255 : 0;
}

void itoa(int n, char * res)
{
        if(n < 10)
        {
                res[0] = (char)(n) + '0';
                res[1] = '\0';
        }
        else if(n < 100)
        {
                res[0] = (char)(n/10) + '0';
                res[1] = (char)(n%10) + '0';
                res[2] = '\0';
        }
        else if(n < 1000)
        {
                res[0] = (char)(n/100) + '0';
                res[1] = (char)((n/10) % 10)+ '0';
                res[2] = (char)(n%10) + '0';
        }
        else
                fprintf(stderr, "I'm too dumb to handle numbers over 1000\n");  
}

/* returns an int whose top bits are the length with introduction
   and bottom bits are the length without introduction */
unsigned int time_info(char * filename, int track)
{
        char * argv[4];
        char argv1[] = "--   ";
        unsigned int frames, result, wintro, wointro;
        char num[] = "   ";

        itoa(track, num);

        argv1[2] = num[0];
        argv1[3] = num[1];
        argv1[4] = num[2];

        argv[0] = "nsfinfo";
        argv[1] = filename;
        argv[2] = argv1;
        argv[3] = "--AT";

        return main(4, argv);
}
