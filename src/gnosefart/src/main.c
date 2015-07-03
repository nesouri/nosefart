/*
    Copyright (C) 2004 Matthew Strait <quadong@users.sf.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

/*
 * Initial main.c file generated by Glade. Edit as required.
 * Glade will not overwrite this file.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkrc.h>

#include <stdio.h>
#include <signal.h>
#include <fcntl.h>

#include "interface.h"
#include "support.h"

void wakeupandplay();
void sfexit();
void otherexit();
void play();
void getnumtracksandhandleit();

extern gchar * filename, * oldfilename;

extern GtkWidget * nferror;

#define NEEDNFVERSION "2.2"

// only ever run once
void gnosefart_init(int argc, char ** argv)
{
	/* Check for the existance and version of nosefart.
	   This is all very hackful and probably easy to break.
	   Note that the version line does string comparison, not numerical comparison. 
	   (Numerical comparison does integers only.) */
	if(!system("nosefart &> /dev/stdout | grep \"command not found\"") ||
	   !system("[[ \"`nosefart -v | grep -i Version | cut -d\\  -f 2 | cut -d- -f 1`\" < \"" NEEDNFVERSION "\" ]]"))
	{
		nferror = create_nonosefart_error();
		gtk_widget_show(nferror);		
	}

	if(argc > 1)
	{	
		filename = (char *)malloc(strlen(argv[1])+1);
		strcpy(filename, argv[1]);

	        oldfilename = (gchar *)malloc(strlen(filename) + 1);
	        strcpy(oldfilename, filename);

		if(!strcmp(filename, "-h") || !strcmp(filename, "--help"))
		{
			printf("syntax: gnosefart [filename]\n"
			"If you want more advanced capabilities from the command line\n"
			"you should use the non-graphical version, \"nosefart\"\n");
			exit(0);
		}

		play();
	}
}

int idleloop();

int
main (int argc, char *argv[])
{
  GtkWidget *mainwindow;
  GtkWidget *fileselection;
  GtkWidget *dialog2;

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  if( signal(SIGSEGV, sfexit) != SIG_DFL )
    fprintf( stderr, "I'm confused about SIGSEGV.\n" );

  if( signal(SIGINT, otherexit) != SIG_DFL )
    fprintf( stderr, "I'm confused about SIGINT.\n" );

  if( signal(SIGHUP, otherexit) != SIG_DFL )
    fprintf( stderr, "I'm confused about SIGHUP.\n" );

  gtk_set_locale ();
  gtk_init (&argc, &argv);

  gnosefart_init(argc, argv);

  add_pixmap_directory (PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps");

  /*
   * The following code was added by Glade to create one of each component
   * (except popup menus), just so that you see something after building
   * the project. Delete any components that you don't want shown initially.
   */
  mainwindow = create_mainwindow ();
  gtk_window_set_resizable((GtkWindow *)mainwindow, 0);
  gtk_widget_show (mainwindow);

  g_timeout_add (1000, (GtkFunction)wakeupandplay, NULL);

  if(filename) getnumtracksandhandleit();

  gtk_main ();

  return 0;
}

