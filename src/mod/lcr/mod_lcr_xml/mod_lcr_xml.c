#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_lcr_xml_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lcr_xml_shutdown);
SWITCH_MODULE_DEFINITION(mod_lcr_xml, mod_lcr_xml_load, mod_lcr_xml_shutdown, NULL);

static struct {
	switch_memory_pool_t *pool;
	char *odbc_dsn;
	char *query_sql;
	char *user_xml_str;
	switch_size_t user_xml_str_size;
} globals;

static switch_status_t load_config(void)
{
	char *cf = "lcr_xml.conf";
	switch_xml_t cfg, xml, settings_tag, param;

	//读取xml配置
	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}


	if ((settings_tag =switch_xml_child(cfg, "settings")) == NULL) {
		goto done;
	}

	for (param = switch_xml_child(settings_tag, "param"); param; param = param->next) {
		char *var = (char *) switch_xml_attr_soft(param, "name");
		char *val = (char *) switch_xml_attr_soft(param, "value");
		if (!strcasecmp(var, "odbc-dsn")) {
			globals.odbc_dsn = strdup(val);
		}else if(!strcasecmp(var, "query-sql")) {
			globals.query_sql = strdup(val);
		}else if(!strcasecmp(var, "user-xml-size")) {
			globals.user_xml_str_size = switch_atoui(val);
		}
	}

	done:
		switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

static switch_cache_db_handle_t *lx_get_db_handle(void)
{
	switch_cache_db_handle_t *dbh = NULL;

	if (zstr(globals.odbc_dsn)) {
		return NULL;
	}

	if (switch_cache_db_get_db_handle_dsn(&dbh, globals.odbc_dsn) != SWITCH_STATUS_SUCCESS) {
		dbh = NULL;
	}

	return dbh;

}

static switch_xml_t get_directory_xml()
{
	switch_xml_t user_xml = NULL;
	switch_cache_db_handle_t *dbh = NULL;
	/* Initialize database */
	if (!(dbh = lx_get_db_handle())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot open DB!\n");
		return NULL;
	}

	switch_cache_db_execute_sql2str(dbh, globals.query_sql, globals.user_xml_str,globals.user_xml_str_size,NULL);
	switch_cache_db_release_db_handle(&dbh);
	if (!(user_xml = switch_xml_parse_str_dynamic(globals.user_xml_str, SWITCH_TRUE))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "xml parse fail!\n");
	}

	return user_xml;
}
SWITCH_MODULE_LOAD_FUNCTION(mod_lcr_xml_load)
{
	switch_status_t status;
	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;

	if ((status = load_config()) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	globals.user_xml_str= switch_core_alloc(pool, globals.user_xml_str_size);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_xml_bind_search_function(get_directory_xml, switch_xml_parse_section_string("directory"), NULL);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
 * Called when the system shuts down
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lcr_xml_shutdown)
{
	switch_safe_free(globals.odbc_dsn);
	switch_safe_free(globals.query_sql);
	return SWITCH_STATUS_SUCCESS;
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
