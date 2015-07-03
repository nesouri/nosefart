/* vim: set tabstop=3 expandtab:
You want an ID tag??? Here's your ID tag! :-)
$Id: main_linux.c,v 1.9 2000/07/03 13:20:26 neil Exp $
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/soundcard.h>
#include <sys/stat.h>

#include "types.h"
#include "nsf.h"

#include "config.h"

#ifndef bool
#define bool boolean
#endif
#ifndef true
#define true TRUE
#endif

/* This saves the trouble of passing our ONE nsf around the stack all day */
static nsf_t *nsf = 0;

/* thank you, nsf_playtrack, for making me pass freq and bits to you */
static int freq = 44100;
static int bits = 8;

/* sound */
static int dataSize;
static int bufferSize;
static unsigned char *buffer = 0, *bufferPos = 0;
static int audiofd;

/* HAS ROOT PERMISSIONS -- BE CAREFUL */
/* Open up the DSP, then drop the root permissions */
static void open_hardware(const char *device)
{
   struct stat status;

   /* Open the file (with root permissions, if we have them) */
   if(-1 == (audiofd = open(device, O_WRONLY, 0)))
   {
      switch(errno)
      {
      case EBUSY:
         printf("%s is busy.\n", device);
         exit(1);
      default:
         printf("Unable to open %s.  Check the permissions.\n", device);
         exit(1);
      }
   }

   /* For safety, we should check that device is, in fact, a device.
      `nosefart -d /etc/passwd MegaMan2.nsf` wouldn't sound so pretty. */
   if(-1 == fstat(audiofd, &status))
   {
      switch(errno)
      {
      case EFAULT:
      case ENOMEM:
         printf("Out of memory.\n");
         exit(1);
      case EBADF:
      case ELOOP:
      case EACCES:
      default:
         printf("Unable to stat %s.\n", device);
         exit(1);
      }
   }
   if( !S_ISCHR(status.st_mode) )
   {
      printf("%s is not a character device.\n", device);
      exit(1);
   }

   /* Drop root permissions */
   if(geteuid() != getuid()) setuid(getuid());
}

/* Configure the DSP */
static void init_hardware(void)
{
   int stereo = 0;
   int param, retval, logDataSize;
   int format = bits; /*AFMT_U8, AFMT_U16LE;*/
   /* sound buffer */
   dataSize = freq / nsf->playback_rate;

   /* Configure the DSP */
   logDataSize = -1;
   while((1 << ++logDataSize) < dataSize);
   param = 0x10000 | logDataSize + 4;
   retval = ioctl(audiofd, SNDCTL_DSP_SETFRAGMENT, &param);
   if(-1 == retval)
   {
      printf("Unable to set buffer size\n");
   }
   param = stereo;
   retval = ioctl(audiofd, SNDCTL_DSP_STEREO, &param);
   if(retval == -1 || param != stereo)
   {
      printf("Unable to set audio channels.\n");
      exit(1);
   }
   param = format;
   retval = ioctl(audiofd, SNDCTL_DSP_SETFMT, &param);
   if(retval == -1 || param != format)
   {
      printf("Unable to set audio format.\n");
      printf("Wanted %i, got %i\n", format, param);
      exit(1);
   }
   param = freq;
   retval = ioctl(audiofd, SNDCTL_DSP_SPEED, &param);
   if(retval == -1 || (abs (param - freq) > (freq / 10)))
   {
      printf("Unable to set audio frequency.\n");
      printf("Wanted %i, got %i\n", freq, param);
      exit(1);
   }
   retval = ioctl(audiofd, SNDCTL_DSP_GETBLKSIZE, &param);
   if(-1 == retval)
   {
      printf("Unable to get buffer size\n");
      exit(1);
   }
   /* set up our data buffer */
   bufferSize = param;
   buffer = malloc((bufferSize / dataSize + 1) * dataSize);
   bufferPos = buffer;
   memset(buffer, 0, bufferSize);
}

/* close what we've opened */
static void close_hardware(void)
{
   close(audiofd);
   free(buffer);
   buffer = 0;
   bufferSize = 0;
   bufferPos = 0;
}

static void show_help(void)
{
   printf("Usage: %s [OPTIONS] filename\n", NAME);
   printf("Play an NSF (NES Sound Format) file.\n");
   printf("\nOptions:\n");
   printf("\t-h  \tHelp\n");
   printf("\t-v  \tVersion information\n");
   printf("\n\t-t x\tStart playing track x (default: 1)\n");
   printf("\n\t-d x\tUse device x (default: /dev/dsp)\n");
   printf("\t-f x\tUse x sampling rate (default: 44100)\n");
   exit(0);
}

static void show_info(void)
{
   printf("%s -- NSF player for Linux\n", NAME);
   printf("Version " VERSION "\n");
   printf("Compiled with GCC %i.%i on %s %s\n", __GNUC__, __GNUC_MINOR__,
         __DATE__, __TIME__);
   printf("\nNSF support by Matthew Conte.\n");
   printf("Inspired by the MSP 0.50 source release by Sevy and Marp.\n");
   printf("Ported by Neil Stevens.\n");
   exit(0);
}

/* start track, display which it is */
static void nsf_setupsong(void)
{
   /* Why not printf directly?  Our termios hijinks for input kills the output */
   char *hi = (char *)malloc(255);
   snprintf(hi, 254, "Playing NSF track %d of %d   \r", nsf->current_song, nsf->num_songs);
   write(STDOUT_FILENO, (void *)hi, strlen(hi));
   nsf_playtrack(nsf, nsf->current_song, freq, bits, 0);
   free(hi);
   return;
}

/* display info about an NSF file */
static void nsf_displayinfo(void)
{
   printf("Keys:\n");
   printf("q to quit\n");
   printf("x to play the next track\n");
   printf("z to play the previous track\n");
   printf("return to restart track\n");
   printf("1-6 to toggle channels\n\n");

   printf("NSF Information:\n");
   printf("Song name:       %s\n", nsf->song_name);
   printf("Artist:          %s\n", nsf->artist_name);
   printf("Copyright:       %s\n", nsf->copyright);
   printf("Number of Songs: %d\n\n", nsf->num_songs);
}

/* set a channel to on/off */
static void set_channel(int channel)
{
   static bool enabled[6] = { true, true, true, true, true, true };
   enabled[channel] ^= true;
   nsf_setchan(nsf, channel, enabled[channel]);
   return;
}

/* handle keypresses */
static int nsf_handlekey(char ch)
{
   ch = tolower(ch);
   switch(ch)
   {
   case 'q':
   case 27: /* escape */
      printf("\n");
      return 1;
   case 'x':
      if(nsf->current_song == nsf->num_songs)
      break;
      nsf->current_song++;
      nsf_setupsong();
      break;
   case 'z':
      if(1 == nsf->current_song)
      break;
      nsf->current_song--;
      nsf_setupsong();
      break;
   case '\n':
      nsf_playtrack(nsf, nsf->current_song, freq, bits, 0);
      break;
   case '1':
   case '2':
   case '3':
   case '4':
   case '5':
   case '6':
      set_channel(ch - '1');
      break;
   default:
      break;
   }
   return 0;
}

/* load the nsf, so we know how to set up the audio */
static int load_nsf_file(char *filename)
{
   nsf_init();
   /* load up an NSF file */
   nsf = nsf_load(filename, 0, 0);
   if(!nsf)
   {
      printf("Error opening \"%s\"\n", filename);
      exit(1);
   }
   printf("Now playing %s\n\n", filename);
}

/* Make some noise, run the show */
static void play(int track)
{
   struct termios term, oldterm;
   int done = 0;

   /* determine which track to play */
   if(track > nsf->num_songs || track < 1)
   {
      nsf->current_song = nsf->start_song;
      fprintf(stderr, "track %d out of range, playing track %d\n", track,
         nsf->current_song);
   }
   else
   nsf->current_song = track;

   /* display file information */
   nsf_displayinfo();
   nsf_setupsong();

   /* UI settings */
   tcgetattr(STDIN_FILENO, &term);
   oldterm = term;
   term.c_lflag &= ~ICANON;
   term.c_lflag &= ~ECHO;
   tcsetattr(STDIN_FILENO, TCSANOW, &term);

   fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);

   while(!done)
   {
      char ch;
      if(1 == fread((void *)&ch, 1, 1, stdin))
      {
         done = nsf_handlekey(ch);
      }

      nsf_frame(nsf);
      apu_process(bufferPos, dataSize);
      bufferPos += dataSize;
      if(bufferPos >= buffer + bufferSize)
      {
         write(audiofd, buffer, bufferPos - buffer);
         bufferPos = buffer;
      }
   }
   tcsetattr(STDIN_FILENO, TCSANOW, &oldterm);
}

/* free what we've allocated */
static void close_nsf_file(void)
{
   nsf_free(&nsf);
   nsf = 0;
}

/* HAS ROOT PERMISSIONS -- BE CAREFUL */
int main(int argc, char **argv)
{
   char *device = "/dev/dsp";
   char *filename;
   int track = 1;
   int done = 0;

   /* parse options */
   const char *opts = "hvd:t:f:";
   while(!done)
   {
      switch(getopt (argc, argv, opts))
      {
      case EOF:
         done = 1;
         break;
      case 'v':
         show_info();
         break;
      case 'd':
         device = malloc( strlen(optarg) + 1 );
         strcpy(device, optarg);
         break;
      case 't':
         track = strtol(optarg, 0, 10);
         break;
      case 'f':
         freq = strtol(optarg, 0, 10);
         break;
      case 'h':
      case ':':
      case '?':
      default:
         show_help();
         break;
      }
   }

   /* filename comes after all other options */
   if(argc <= optind)
   show_help();
   filename = malloc( strlen(argv[optind]) + 1 );
   strcpy(filename, argv[optind]);

   /* open_hardware uses, then discards, root permissions */
   open_hardware(device);
   load_nsf_file(filename);
   init_hardware();
   play(track);
   close_nsf_file();
   close_hardware();
}

/*
** $Log: main_linux.c,v $
** Revision 1.9  2000/07/03 13:20:26  neil
** Forgot a default case when stat fails
**
** Revision 1.8  2000/07/03 13:04:51  neil
** Fail if passed device isn't a character device
**
** Revision 1.7  2000/07/01 09:35:19  neil
** Clearer output
**
** Revision 1.6  2000/06/30 14:12:56  neil
** Track output fixed
**
** Revision 1.5  2000/06/30 12:33:27  neil
** Linux nosefart finally works again
**
** Revision 1.4  2000/06/09 15:12:26  matt
** initial revision
**
*/
