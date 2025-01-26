#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_lcr_xml_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lcr_xml_shutdown);
SWITCH_MODULE_DEFINITION(mod_lcr_xml, mod_lcr_xml_load, mod_lcr_xml_shutdown, NULL);

struct xml_binding_ud_s
{
	char *query_sql;
	char *bind_mask;
};
typedef struct xml_binding_ud_s xml_binding_ud_t;

typedef struct tmp_file {
	char name[512];
	int fd;
} tmp_file_s;

static struct {
	switch_memory_pool_t *pool;
	char *odbc_dsn;
	char *user_xml_str;
	switch_size_t user_xml_str_size;
} globals;

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

static switch_xml_t get_directory_xml(const char *section, const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params,
								  void *user_data)
{
	xml_binding_ud_t *ud_obj = (xml_binding_ud_t *) user_data;
	switch_xml_t user_xml = NULL;
	switch_cache_db_handle_t *dbh = NULL;
	/* Initialize database */
	if (!(dbh = lx_get_db_handle())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot open DB!\n");
		return NULL;
	}

	switch_cache_db_execute_sql2str(dbh, ud_obj->query_sql, globals.user_xml_str,globals.user_xml_str_size,NULL);
	switch_cache_db_release_db_handle(&dbh);
	if (!(user_xml = switch_xml_parse_str_dup(globals.user_xml_str))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "xml parse fail!\n");
	}

	return user_xml;
}

static switch_xml_t lx_xml_sql_fetch(const char *section, const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params,
								  void *user_data)
{
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	tmp_file_s tmp_file_obj;
	xml_binding_ud_t *ud_obj = (xml_binding_ud_t *) user_data;
	switch_xml_t user_xml = NULL;
	switch_cache_db_handle_t *dbh = NULL;
	ssize_t user_xml_str_len;
	ssize_t write_res;
	/* Initialize database */
	if (!(dbh = lx_get_db_handle())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot open DB!\n");
		return NULL;
	}

	switch_cache_db_execute_sql2str(dbh, ud_obj->query_sql, globals.user_xml_str,globals.user_xml_str_size,NULL);
	switch_cache_db_release_db_handle(&dbh);

	/* 创建存储xml的临时文件 */
	memset(&tmp_file_obj, 0, sizeof(tmp_file_obj));
	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);
	switch_snprintf(tmp_file_obj.name, sizeof(tmp_file_obj.name), "%s%s%s%s.tmp.xml", SWITCH_GLOBAL_dirs.temp_dir, SWITCH_PATH_SEPARATOR, "lcr_xml_",uuid_str);
	if ((tmp_file_obj.fd = open(tmp_file_obj.name, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR)) <= -1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open the %s file fail!\n",tmp_file_obj.name);
	}
	/* 计算字符串的长度 */
	user_xml_str_len = strlen(globals.user_xml_str);
	/* 写入数据到文件 */
	write_res = write(tmp_file_obj.fd, globals.user_xml_str, user_xml_str_len);

	if (write_res != user_xml_str_len) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to write to the %s file!\n",tmp_file_obj.name);
	}

	close(tmp_file_obj.fd);
	if (!(user_xml = switch_xml_parse_file(tmp_file_obj.name))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "xml parse fail!\n");
	}

	return user_xml;
}

static switch_status_t load_config(void)
{
	char *cf = "lcr_xml.conf";
	switch_xml_t cfg, xml, settings_tag, bindings_tag, param;
	xml_binding_ud_t *binding_ud_obj = NULL;

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
		}else if(!strcasecmp(var, "user-xml-size")) {
			globals.user_xml_str_size = switch_atoui(val);
		}
	}

	if ((bindings_tag = switch_xml_child(cfg, "bindings")) == NULL) {
		goto done;
	}

	for (param = switch_xml_child(bindings_tag, "binding"); param; param = param->next) {
		char *query_sql = (char *) switch_xml_attr_soft(param, "query-sql");
		char *bind_mask = (char *) switch_xml_attr_soft(param, "bindings");
		if (!strcasecmp(query_sql, "") || !strcasecmp(bind_mask, "")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "miss query-sql or bindings\n");
			continue;
		}

		/* 给结构体分配内存*/
		if (!(binding_ud_obj = switch_core_alloc(globals.pool, sizeof(*binding_ud_obj)))) {
			goto done;
		}

		memset(binding_ud_obj, 0, sizeof(*binding_ud_obj));

		binding_ud_obj->query_sql = switch_core_strdup(globals.pool, query_sql);
		binding_ud_obj->bind_mask = switch_core_strdup(globals.pool, bind_mask);

		switch_xml_bind_search_function(lx_xml_sql_fetch, switch_xml_parse_section_string(binding_ud_obj->bind_mask),binding_ud_obj);
		binding_ud_obj = NULL;
	}

	done:
		switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
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

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
 * Called when the system shuts down
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lcr_xml_shutdown)
{
	switch_xml_unbind_search_function_ptr(get_directory_xml);
	switch_xml_unbind_search_function_ptr(lx_xml_sql_fetch);
	switch_safe_free(globals.odbc_dsn);
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
