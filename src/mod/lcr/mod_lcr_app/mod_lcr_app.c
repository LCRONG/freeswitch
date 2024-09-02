#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_lcr_app_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lcr_app_shutdown);
//SWITCH_MODULE_RUNTIME_FUNCTION(mod_lcr_app_runtime);
SWITCH_MODULE_DEFINITION(mod_lcr_app, mod_lcr_app_load, mod_lcr_app_shutdown, NULL);

#define LCR_APP_TEST_SYNTAX ""
SWITCH_STANDARD_APP(lcr_app_test_func)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "const char *data: %s\n", data);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", LCR_APP_TEST_SYNTAX);
	switch_ivr_park(session, NULL);
}

SWITCH_MODULE_LOAD_FUNCTION(mod_lcr_app_load)
{
	switch_application_interface_t *app_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "lcr_app_test", "Used for debugging", "Used for debugging various variables in the session", lcr_app_test_func, LCR_APP_TEST_SYNTAX, SAF_SUPPORT_NOMEDIA);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
 * Called when the system shuts down
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lcr_app_shutdown)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_lcr_app_shutdown Hello World!\n");
	return SWITCH_STATUS_SUCCESS;
}


/*
  If it exists, this is called in it's own thread when the module-load completes
  If it returns anything but SWITCH_STATUS_TERM it will be called again automaticly

SWITCH_MODULE_RUNTIME_FUNCTION(mod_lcr_app_runtime)
{
	while(1)
	{
		switch_yield(1000);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_lcr_app_runtime Hello World!\n");
		break;
	}
	return SWITCH_STATUS_TERM;
}
*/


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
