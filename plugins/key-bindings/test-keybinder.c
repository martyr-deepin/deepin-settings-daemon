/* main.c
 * Created in 2010 by Ulrik Sverdrup <ulrik.sverdrup@gmail.com>
 *
 * This work is placed in the public domain.
 */

#include <stdio.h>

#include <gtk/gtk.h>
#include <keybinder.h>

#define EXAMPLE_KEY1 "<Super>X"
#define EXAMPLE_KEY2 "<Alt>Print"

void handler (const char *keystring, void *user_data) {
  printf("Handle %s (%p)!\n", keystring, user_data);
  keybinder_unbind(keystring, handler);
  gtk_main_quit();
}

int main (int argc, char *argv[])
{
  gtk_init(&argc, &argv);

  keybinder_init();
  keybinder_bind(EXAMPLE_KEY1, handler, NULL);
  keybinder_bind(EXAMPLE_KEY2, handler, NULL);
  printf("Press " EXAMPLE_KEY1 " to activate keybinding and quit\n");
  printf("Press " EXAMPLE_KEY2 " to activate keybinding and quit\n");

  gtk_main();
  return 0;
}

