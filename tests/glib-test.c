#include <glib.h>

static void
test_pass (void)
{
  g_assert(TRUE);
}

static void
test_fail (void)
{
  g_assert(FALSE);
}


int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/test/pass", test_pass);
  g_test_add_func ("/test/fail", test_fail);

  return g_test_run ();
}
