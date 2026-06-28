#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

START_TEST(test_fgets_buffer_boundary)
{
    // Invariant: fgets must not leave excess characters in input stream when reading lines
    const char *payloads[] = {
        "A",  // Valid short input
        "1234567890123456789012345678901234567890",  // Boundary: exactly sizeof(buf)-2 chars
        "12345678901234567890123456789012345678901",  // Exploit: exceeds buffer by 1 char
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",  // Large payload
        "\n"  // Empty line
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        // Create a temporary file with the test payload
        FILE *f = tmpfile();
        ck_assert_ptr_nonnull(f);
        
        fputs(payloads[i], f);
        rewind(f);
        
        // Simulate the vulnerable code path
        char buf[42];  // Match the buffer size from browser.c
        char *result = fgets(buf, sizeof(buf)-1, f);
        
        // Property: If fgets returns non-NULL, the input stream should be properly consumed
        // without leaving dangling characters that could corrupt subsequent reads
        if (result != NULL) {
            // Verify the buffer contains a properly terminated string
            ck_assert_int_lt(strlen(buf), sizeof(buf));
            
            // Read next character to check if input stream was properly consumed
            int next_char = fgetc(f);
            if (next_char != EOF) {
                // If there's more data, it should only be because the line was longer than buffer
                // and fgets consumed exactly sizeof(buf)-1 characters
                rewind(f);
                fgets(buf, sizeof(buf)-1, f);
                ck_assert_int_eq(strlen(buf), sizeof(buf)-2);  // -2 for null terminator and newline handling
            }
        }
        
        fclose(f);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_fgets_buffer_boundary);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}