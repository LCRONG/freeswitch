#include <switch.h>
#include <switch_buffer.h>
#include <libwebsockets.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_lcr_test_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lcr_test_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_lcr_test_runtime);
SWITCH_MODULE_DEFINITION(mod_lcr_test, mod_lcr_test_load, mod_lcr_test_shutdown, mod_lcr_test_runtime);

void test_switch_buffer()
{
	switch_buffer_t *audio_buffer;
	char a[10] = {'1','2','3','4','5'};
	char pre[LWS_PRE] ={0};
	memset(pre, 1, sizeof(pre));
	switch_buffer_create_dynamic(&audio_buffer, 2, 50, 100);
	switch_buffer_write(audio_buffer, pre, sizeof(pre));
	switch_buffer_read(audio_buffer,&a[5],1);
	//截断数据
	switch_buffer_toss(audio_buffer,2);
	switch_buffer_write(audio_buffer, a, strlen(a));
}

static switch_xml_t get_directory_xml()
{
	switch_xml_t xml = NULL;
	char *str = "<document type=\"freeswitch/xml\">\n"
		"<section name=\"directory\">\n"
			"<domain name=\"172.29.0.2\">\n"
				"<params>\n"
					"<param name=\"dial-string\" value=\"{^^:sip_invite_domain=${dialed_domain}:presence_id=${dialed_user}@${dialed_domain}}${sofia_contact(*/${dialed_user}@${dialed_domain})},${verto_contact(${dialed_user}@${dialed_domain})}\"/>\n"
					"<param name=\"jsonrpc-allowed-methods\" value=\"verto\"/>\n"
				"</params>\n"
				"<variables>\n"
					"<variable name=\"record_stereo\" value=\"true\"/>\n"
					"<variable name=\"default_gateway\" value=\"$${default_provider}\"/>\n"
					"<variable name=\"default_areacode\" value=\"$${default_areacode}\"/>\n"
					"<variable name=\"transfer_fallback_extension\" value=\"operator\"/>\n"
				"</variables>\n"
				"<groups>\n"
					"<group name=\"default\">\n"
						"<users>\n"
							"<user id=\"9999\">\n"
								"<params>\n"
									"<param name=\"password\" value=\"1234\"/>\n"
									"<param name=\"vm-password\" value=\"9999\"/>\n"
								"</params>\n"
								"<variables>\n"
									"<variable name=\"toll_allow\" value=\"domestic,international,local\"/>\n"
									"<variable name=\"accountcode\" value=\"9999\"/>\n"
									"<variable name=\"user_context\" value=\"default\"/>\n"
									"<variable name=\"effective_caller_id_name\" value=\"Extension 9999\"/>\n"
									"<variable name=\"effective_caller_id_number\" value=\"9999\"/>\n"
									"<variable name=\"outbound_caller_id_name\" value=\"$${outbound_caller_name}\"/>\n"
									"<variable name=\"outbound_caller_id_number\" value=\"$${outbound_caller_id}\"/>\n"
									"<variable name=\"callgroup\" value=\"techsupport\"/>\n"
								"</variables>\n"
							"</user>\n"
						"</users>\n"
					"</group>\n"
				"</groups>\n"
			"</domain>\n"
		"</section>\n"
	"</document>";
	xml = switch_xml_parse_str_dynamic(str, SWITCH_TRUE);
	if (!xml) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "xml parse fail!\n");
	}
	return xml;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_lcr_test_load)
{
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "LCR Hello World!\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "CRIT LCR Hello World!\n");

	switch_xml_bind_search_function(get_directory_xml, switch_xml_parse_section_string("directory"), NULL);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
 * Called when the system shuts down
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lcr_test_shutdown)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_lcr_test_shutdown Hello World!\n");
	return SWITCH_STATUS_SUCCESS;
}


/*
  If it exists, this is called in it's own thread when the module-load completes
  If it returns anything but SWITCH_STATUS_TERM it will be called again automaticly
  */
SWITCH_MODULE_RUNTIME_FUNCTION(mod_lcr_test_runtime)
{
	while(1)
	{
		test_switch_buffer();
		switch_yield(1000);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_lcr_test_runtime Hello World!\n");
		break;
	}
	return SWITCH_STATUS_TERM;
}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet expandtab:
 */
