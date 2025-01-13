#include <switch.h>
#include <test/switch_test.h>

FST_MINCORE_BEGIN("./conf")

FST_SUITE_BEGIN(SWITCH_STANDARD_STREAM)

FST_SETUP_BEGIN()
{
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()

FST_TEST_BEGIN(benchmark)
{
	char expected_result[] = {'A', 0x00, 0x01, 0x02, 'B'};
	char raw_data[] = {0x00, 0x01, 0x02};
	switch_stream_handle_t stream = { 0 };
	SWITCH_STANDARD_STREAM(stream);

	stream.write_function(&stream, "%s", "A");
	stream.raw_write_function(&stream, (uint8_t *) raw_data, sizeof(raw_data));
	stream.write_function(&stream, "B");

	fst_requires(stream.data_len == sizeof(expected_result));
	fst_requires(memcmp(stream.data, expected_result, sizeof(expected_result)) == 0);

	switch_safe_free(stream.data);
}
FST_TEST_END()

FST_SUITE_END()

FST_MINCORE_END()

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
