#include "util.h"

int main(int argc, char *argv[]) {

    GTimeVal a = { 5, 5 }, b;

    b = a;

    g_message("%li.%li", a.tv_sec, a.tv_usec);
    avahi_timeval_add(&a, -50);

    g_message("%li.%li", a.tv_sec, a.tv_usec);

    g_message("%lli", avahi_timeval_diff(&a, &b));
}
