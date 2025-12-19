# Coding Style

We mostly follow the [coding style guidelines of the PulseAudio project](https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Developer/CodingStyle/). However, there are a few deviations:

- Avahi uses GLib style CamelCase names for structs.
- Our prefix for functions is `avahi_` and not `pa_`.
- We have no `pa_assert()` counterpart. Use standard libc `assert()` instead.
- We lack a `pa_assert_se()` counterpart. Use libc `assert()`, but make sure your code still works when NDEBUG is defined.
