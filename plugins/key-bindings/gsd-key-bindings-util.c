#include <string.h>

#include "gsd-key-bindings-handler.h"
#include "gsd-key-bindings-util.h"
#include "gsd-key-bindings-settings.h"
#include "keybinder.h"


static gboolean
check_is_default (gchar* gsettings_key)
{
//	if (g_strcmp0 (key, ...)
//		return TRUE;

	return FALSE;
}
/*
 *	return : NULL on invalid or 
 */
KeysAndCmd* 
gsd_kb_util_parse_gsettings_value (char* gsettings_key, char* string)
{
	char* _keystring = NULL;
	char* _cmdstring = NULL;
	if (check_is_default(gsettings_key))
	{
	    //default key bindings
	    _cmdstring = g_strdup (gsettings_key);
	    _keystring = g_strdup (string);
	}
	else
	{
	    //empty slots:   <command> ';' <keys>
	    char* _tmp = strchr (string, BINDING_DELIMITER);
	    if (_tmp == NULL)  //no ';', invalid, return NULL.
		return NULL;
	    *_tmp = '\0'; //split _str into to two strings.

	    _cmdstring = g_strdup (g_strstrip (string));
	    _keystring = g_strdup (g_strstrip (_tmp+1));
	    

	}
	if ((_cmdstring == NULL)||(_keystring == NULL))
		return NULL;
	if ((!strlen(_cmdstring))||(!strlen(_keystring))) //either string is 0-length, invalid.
		return NULL;
	
	g_debug ("keybindings: %s ----> %s", _keystring, _cmdstring);

	KeysAndCmd* _kandc_ptr = g_new0 (KeysAndCmd, 1);
	_kandc_ptr->keystring = _keystring;
	_kandc_ptr->cmdstring = _cmdstring;

	return _kandc_ptr;
}
/*
 *	NOTE: this is only used for initializing gsettings_ht.
 *	@gsettings_ht: gsettings key --> KeysAndCmd
 */
void
gsd_kb_util_read_gsettings (GSettings* settings, GHashTable* gsettings_ht)
{
	char* _gsettings_key_str = NULL;
	char* _gsettings_value_str = NULL;
	KeysAndCmd* _kandc_ptr = NULL;
	//1. read default key 
	
	//2. read other keys.
	_gsettings_key_str = "key1"; //use macros defined in gsd-key-bindings-settings.h
	_gsettings_value_str = g_settings_get_string (settings, _gsettings_key_str);
	_kandc_ptr = gsd_kb_util_parse_gsettings_value (_gsettings_key_str,
							_gsettings_value_str);

	KeybinderHandler _handler = gsd_kb_handler_default;
	keybinder_bind (_kandc_ptr->keystring, _handler, _kandc_ptr->cmdstring);
	    
	g_hash_table_insert (gsettings_ht, _gsettings_key_str, _kandc_ptr);
}

void 
gsd_kb_util_key_free_func (gpointer data)
{
	char* _gsettings_key_str = (char*) data;
	g_free (_gsettings_key_str);
}
void 
gsd_kb_util_value_free_func (gpointer data)
{
	KeysAndCmd *_kandc_ptr = (KeysAndCmd*) (data);
	char* _keystring = _kandc_ptr->keystring;
	char* _cmdstring = _kandc_ptr->cmdstring;
	//Currently we only support one handler. 
	KeybinderHandler _handler = gsd_kb_handler_default;

	keybinder_unbind (_keystring, _handler);

	g_free (_keystring);
	g_free (_cmdstring);
	g_free (_kandc_ptr);
}

