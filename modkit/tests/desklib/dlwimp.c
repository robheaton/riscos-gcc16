/* dlwimp.c - a minimal DeskLib32 Wimp task, built with GCC 16: Event_Initialise (Wimp_Initialise), an icon on the icon bar, Select shows an error box, Menu opens a menu with Info and Quit, Quit ends the task.
   Run it from the desktop (double-click !Run).  What to see: the icon "file_fff" on the right of the icon bar; click Select: a box "DeskLib32 on GCC 16: click, N polls"; Menu: the menu; Quit: the icon goes. */
#include <stdio.h>
#include <string.h>
#include "DeskLib:Core.h"
#include "DeskLib:Event.h"
#include "DeskLib:EventMsg.h"
#include "DeskLib:Error.h"
#include "DeskLib:Icon.h"
#include "DeskLib:Menu.h"
#include "DeskLib:Screen.h"
#include "DeskLib:Str.h"
#include "DeskLib:Time.h"
#include "DeskLib:Wimp.h"

static menu_ptr menu;
static int polls, clicks, quit;

static BOOL quit_message (event_pollblock *e, void *ref)
{
  (void) e; (void) ref;
  quit = TRUE;
  return TRUE;
}
static BOOL icon_click (event_pollblock *e, void *ref)
{
  (void) ref;
  if (e->data.mouse.button.data.select)
    {
      char text[100];
      unsigned char clk[5];
      char date[40] = "";
      clicks++;
      Time_ReadClock (clk);
      Time_ConvertDateAndTime (clk, date, sizeof date, "%DY %M3 %CE%YR");
      Screen_CacheModeInfo ();
      sprintf (text, "DeskLib32 on GCC 16: click %d, %d polls, %s, screen %d x %d pixels, %d bpp", clicks, polls, date, screen_size.x >> screen_eig.x, screen_size.y >> screen_eig.y, screen_bpp);
      Error_Report (0, text);
    }
  else if (e->data.mouse.button.data.menu)
    Menu_Show (menu, e->data.mouse.pos.x, -1);
  return TRUE;
}
static BOOL menu_select (event_pollblock *e, void *ref)
{
  (void) ref;
  if (e->data.selection[0] == 1) quit = TRUE;
  else if (e->data.selection[0] == 0) Error_Report (0, "DLWimp: DeskLib32 built with GCC 16 and asasm");
  return TRUE;
}

int main (void)
{
  Event_Initialise ("DLWimp");
  EventMsg_Initialise ();
  EventMsg_Claim (message_QUIT, event_ANY, quit_message, NULL);
  Icon_BarIcon ("file_fff", iconbar_RIGHT);
  menu = Menu_New ("DLWimp", "Info,Quit");
  Event_Claim (event_CLICK, window_ICONBAR, event_ANY, icon_click, NULL);
  Event_Claim (event_MENU, event_ANY, event_ANY, menu_select, NULL);
  while (!quit) { Event_Poll (); polls++; }
  Event_CloseDown ();
  return 0;
}
