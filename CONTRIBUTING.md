# Coding Style

We mostly follow the [coding style guidelines of the PulseAudio project](https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Developer/CodingStyle/). However, there are a few deviations:

- Avahi uses GLib style CamelCase names for structs.
- Our prefix for functions is `avahi_` and not `pa_`.
- We have no `pa_assert()` counterpart. Use standard libc `assert()` instead.
- We lack a `pa_assert_se()` counterpart. Use libc `assert()`, but make sure your code still works when NDEBUG is defined.

# LLMs

See https://discourse.llvm.org/t/rfc-llvm-ai-tool-policy-human-in-the-loop/89159

Contributors must read and review all LLM-generated code or text before they
ask other project members to review it. The contributor is always the author
and is fully accountable for their contributions. Contributors should be
sufficiently confident that the contribution is high enough quality that asking
for a review is a good use of scarce maintainer time, and they should be able
to answer questions about their work during review.

Contributors are expected to be transparent and label contributions that
contain substantial amounts of LLM-generated content.

Our golden rule is that a contribution should be worth more to the project
than the time it takes to review it.
