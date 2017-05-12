/*
 * config.c - the platform-independent parts of the PuTTY
 * configuration box.
 */

#include <assert.h>
#include <stdlib.h>

#include "putty.h"
#include "dialog.h"
#include "storage.h"

// CYGTERM patch
int cygterm_get_flag( void );

#define PRINTER_DISABLED_STRING "없음 (프린트 사용 안 함)"

#define HOST_BOX_TITLE "호스트 이름(또는 IP 주소)"
#define PORT_BOX_TITLE "포트"

#ifdef PUTTY_WINSTUFF_H
/*
 * HACK: PuttyTray / PuTTY File
 * Define and struct moved up here so they can be used in storagetype_handler
 */
#define SAVEDSESSION_LEN 2048
#endif
struct sessionsaver_data {
    union control *editbox, *listbox, *loadbutton, *savebutton, *delbutton;
    union control *okbutton, *cancelbutton;
    struct sesslist sesslist;
    int midsession;
    char *savedsession;     /* the current contents of ssd->editbox */
};

void conf_radiobutton_handler(union control *ctrl, void *dlg,
			      void *data, int event)
{
    int button;
    Conf *conf = (Conf *)data;

    /*
     * For a standard radio button set, the context parameter gives
     * the primary key (CONF_foo), and the extra data per button
     * gives the value the target field should take if that button
     * is the one selected.
     */
    if (event == EVENT_REFRESH) {
	int val = conf_get_int(conf, ctrl->radio.context.i);
	for (button = 0; button < ctrl->radio.nbuttons; button++)
	    if (val == ctrl->radio.buttondata[button].i)
		break;
	/* We expected that `break' to happen, in all circumstances. */
	assert(button < ctrl->radio.nbuttons);
	dlg_radiobutton_set(ctrl, dlg, button);
    } else if (event == EVENT_VALCHANGE) {
	button = dlg_radiobutton_get(ctrl, dlg);
	assert(button >= 0 && button < ctrl->radio.nbuttons);
	conf_set_int(conf, ctrl->radio.context.i,
		     ctrl->radio.buttondata[button].i);
    }
}

#define CHECKBOX_INVERT (1<<30)
void conf_checkbox_handler(union control *ctrl, void *dlg,
			   void *data, int event)
{
    int key, invert;
    Conf *conf = (Conf *)data;

    /*
     * For a standard checkbox, the context parameter gives the
     * primary key (CONF_foo), optionally ORed with CHECKBOX_INVERT.
     */
    key = ctrl->checkbox.context.i;
    if (key & CHECKBOX_INVERT) {
	key &= ~CHECKBOX_INVERT;
	invert = 1;
    } else
	invert = 0;

    /*
     * C lacks a logical XOR, so the following code uses the idiom
     * (!a ^ !b) to obtain the logical XOR of a and b. (That is, 1
     * iff exactly one of a and b is nonzero, otherwise 0.)
     */

    if (event == EVENT_REFRESH) {
	int val = conf_get_int(conf, key);
	dlg_checkbox_set(ctrl, dlg, (!val ^ !invert));
    } else if (event == EVENT_VALCHANGE) {
	conf_set_int(conf, key, !dlg_checkbox_get(ctrl,dlg) ^ !invert);
    }
}

void conf_editbox_handler(union control *ctrl, void *dlg,
			  void *data, int event)
{
    /*
     * The standard edit-box handler expects the main `context'
     * field to contain the primary key. The secondary `context2'
     * field indicates the type of this field:
     *
     *  - if context2 > 0, the field is a string.
     *  - if context2 == -1, the field is an int and the edit box
     *    is numeric.
     *  - if context2 < -1, the field is an int and the edit box is
     *    _floating_, and (-context2) gives the scale. (E.g. if
     *    context2 == -1000, then typing 1.2 into the box will set
     *    the field to 1200.)
     */
    int key = ctrl->editbox.context.i;
    int length = ctrl->editbox.context2.i;
    Conf *conf = (Conf *)data;

    if (length > 0) {
	if (event == EVENT_REFRESH) {
	    char *field = conf_get_str(conf, key);
	    dlg_editbox_set(ctrl, dlg, field);
	} else if (event == EVENT_VALCHANGE) {
	    char *field = dlg_editbox_get(ctrl, dlg);
	    conf_set_str(conf, key, field);
	    sfree(field);
	}
    } else if (length < 0) {
	if (event == EVENT_REFRESH) {
	    char str[80];
	    int value = conf_get_int(conf, key);
	    if (length == -1)
		sprintf(str, "%d", value);
	    else
		sprintf(str, "%g", (double)value / (double)(-length));
	    dlg_editbox_set(ctrl, dlg, str);
	} else if (event == EVENT_VALCHANGE) {
	    char *str = dlg_editbox_get(ctrl, dlg);
	    if (length == -1)
		conf_set_int(conf, key, atoi(str));
	    else
		conf_set_int(conf, key, (int)((-length) * atof(str)));
	    sfree(str);
	}
    }
}

void conf_filesel_handler(union control *ctrl, void *dlg,
			  void *data, int event)
{
    int key = ctrl->fileselect.context.i;
    Conf *conf = (Conf *)data;

    if (event == EVENT_REFRESH) {
	dlg_filesel_set(ctrl, dlg, conf_get_filename(conf, key));
    } else if (event == EVENT_VALCHANGE) {
	Filename *filename = dlg_filesel_get(ctrl, dlg);
	conf_set_filename(conf, key, filename);
        filename_free(filename);
    }
}

void conf_fontsel_handler(union control *ctrl, void *dlg,
			  void *data, int event)
{
    int key = ctrl->fontselect.context.i;
    Conf *conf = (Conf *)data;

    if (event == EVENT_REFRESH) {
	dlg_fontsel_set(ctrl, dlg, conf_get_fontspec(conf, key));
    } else if (event == EVENT_VALCHANGE) {
	FontSpec *fontspec = dlg_fontsel_get(ctrl, dlg);
	conf_set_fontspec(conf, key, fontspec);
        fontspec_free(fontspec);
    }
}

static void config_host_handler(union control *ctrl, void *dlg,
				void *data, int event)
{
    Conf *conf = (Conf *)data;

    /*
     * This function works just like the standard edit box handler,
     * only it has to choose the control's label and text from two
     * different places depending on the protocol.
     */
    if (event == EVENT_REFRESH) {
	if (conf_get_int(conf, CONF_protocol) == PROT_SERIAL) {
	    /*
	     * This label text is carefully chosen to contain an n,
	     * since that's the shortcut for the host name control.
	     */
	    dlg_label_change(ctrl, dlg, "시리얼 라인");
	    dlg_editbox_set(ctrl, dlg, conf_get_str(conf, CONF_serline));
	// CYGTERM patch
	} else if (conf_get_int(conf, CONF_protocol) == PROT_CYGTERM) {
	    dlg_label_change(ctrl, dlg, "명령 (login shell은 - 를 사용)");
	    dlg_editbox_set(ctrl, dlg, conf_get_str(conf, CONF_cygcmd));
	} else {
	    dlg_label_change(ctrl, dlg, HOST_BOX_TITLE);
	    dlg_editbox_set(ctrl, dlg, conf_get_str(conf, CONF_host));
	}
    } else if (event == EVENT_VALCHANGE) {
	char *s = dlg_editbox_get(ctrl, dlg);
	if (conf_get_int(conf, CONF_protocol) == PROT_SERIAL)
	    conf_set_str(conf, CONF_serline, s);
	// CYGTERM patch
	else if (conf_get_int(conf, CONF_protocol) == PROT_CYGTERM) {
	    char *s = dlg_editbox_get(ctrl, dlg);
	    conf_set_str(conf, CONF_cygcmd, s);
	}
	else
	    conf_set_str(conf, CONF_host, s);
	sfree(s);
    }
}

static void config_port_handler(union control *ctrl, void *dlg,
				void *data, int event)
{
    Conf *conf = (Conf *)data;
    char buf[80];

    /*
     * This function works similarly to the standard edit box handler,
     * only it has to choose the control's label and text from two
     * different places depending on the protocol.
     */
    if (event == EVENT_REFRESH) {
	if (conf_get_int(conf, CONF_protocol) == PROT_SERIAL) {
	    /*
	     * This label text is carefully chosen to contain a p,
	     * since that's the shortcut for the port control.
	     */
	    dlg_label_change(ctrl, dlg, "속도");
	    sprintf(buf, "%d", conf_get_int(conf, CONF_serspeed));
	// CYGTERM patch
	} else if (conf_get_int(conf, CONF_protocol) == PROT_CYGTERM) {
	    dlg_label_change(ctrl, dlg, "포트 (무시)");
	    strcpy(buf, "-");
	} else {
	    dlg_label_change(ctrl, dlg, PORT_BOX_TITLE);
	    if (conf_get_int(conf, CONF_port) != 0)
		sprintf(buf, "%d", conf_get_int(conf, CONF_port));
	    else
		/* Display an (invalid) port of 0 as blank */
		buf[0] = '\0';
	}
	dlg_editbox_set(ctrl, dlg, buf);
    } else if (event == EVENT_VALCHANGE) {
	char *s = dlg_editbox_get(ctrl, dlg);
	int i = atoi(s);
	sfree(s);

	if (conf_get_int(conf, CONF_protocol) == PROT_SERIAL)
	    conf_set_int(conf, CONF_serspeed, i);
	else if (conf_get_int(conf, CONF_protocol) == PROT_CYGTERM) { ; } // CYGTERM patch
	else
	    conf_set_int(conf, CONF_port, i);
    }
}

struct hostport {
    union control *host, *port;
};

/*
 * We export this function so that platform-specific config
 * routines can use it to conveniently identify the protocol radio
 * buttons in order to add to them.
 */
void config_protocolbuttons_handler(union control *ctrl, void *dlg,
				    void *data, int event)
{
    int button;
    Conf *conf = (Conf *)data;
    struct hostport *hp = (struct hostport *)ctrl->radio.context.p;

    /*
     * This function works just like the standard radio-button
     * handler, except that it also has to change the setting of
     * the port box, and refresh both host and port boxes when. We
     * expect the context parameter to point at a hostport
     * structure giving the `union control's for both.
     */
    if (event == EVENT_REFRESH) {
	int protocol = conf_get_int(conf, CONF_protocol);
	for (button = 0; button < ctrl->radio.nbuttons; button++)
	    if (protocol == ctrl->radio.buttondata[button].i)
		break;
	/* We expected that `break' to happen, in all circumstances. */
	assert(button < ctrl->radio.nbuttons);
	dlg_radiobutton_set(ctrl, dlg, button);
    } else if (event == EVENT_VALCHANGE) {
	int oldproto = conf_get_int(conf, CONF_protocol);
	int newproto, port;

	button = dlg_radiobutton_get(ctrl, dlg);
	assert(button >= 0 && button < ctrl->radio.nbuttons);
	newproto = ctrl->radio.buttondata[button].i;
	conf_set_int(conf, CONF_protocol, newproto);

	if (oldproto != newproto) {
	    Backend *ob = backend_from_proto(oldproto);
	    Backend *nb = backend_from_proto(newproto);
	    assert(ob);
	    assert(nb);
	    /* Iff the user hasn't changed the port from the old protocol's
	     * default, update it with the new protocol's default.
	     * (This includes a "default" of 0, implying that there is no
	     * sensible default for that protocol; in this case it's
	     * displayed as a blank.)
	     * This helps with the common case of tabbing through the
	     * controls in order and setting a non-default port before
	     * getting to the protocol; we want that non-default port
	     * to be preserved. */
	    port = conf_get_int(conf, CONF_port);
	    if (port == ob->default_port)
		conf_set_int(conf, CONF_port, nb->default_port);
	}
	dlg_refresh(hp->host, dlg);
	dlg_refresh(hp->port, dlg);
    }
}

#ifdef PUTTY_WINSTUFF_H
/*
 * HACK: PuttyTray / PuTTY File
 * Storagetype radio buttons event handler
 */
void storagetype_handler(union control *ctrl, void *dlg, void *data, int event)
{
    int button;
    struct sessionsaver_data *ssd =(struct sessionsaver_data *)ctrl->generic.context.p;
    Conf *conf = (Conf *)data;

    /*
     * For a standard radio button set, the context parameter gives
     * offsetof(targetfield, Config), and the extra data per button
     * gives the value the target field should take if that button
     * is the one selected.
     */
    if (event == EVENT_REFRESH) {
	button = conf_get_int(conf, CONF_session_storagetype); // Button index = same as storagetype number. Set according to config
	dlg_radiobutton_set(ctrl, dlg, button);
    } else if (event == EVENT_VALCHANGE) {
	button = dlg_radiobutton_get(ctrl, dlg);

	// Switch between registry and file
	if (ctrl->radio.buttondata[button].i == 0) {
	    get_sesslist(&ssd->sesslist, FALSE, 0);
	    get_sesslist(&ssd->sesslist, TRUE, 0);
	    dlg_refresh(ssd->editbox, dlg);
	    dlg_refresh(ssd->listbox, dlg);

	    // Save setting into config (the whole *(int *)ATOFFSET(data, ctrl->radio.context.i) = ctrl->radio.buttondata[button].i; didn't work)
	    // and I don't see why I shouldn't do it this way (it works?)
	    conf_set_int(conf, CONF_session_storagetype, 0);
	} else {
	    get_sesslist(&ssd->sesslist, FALSE, 1);
	    get_sesslist(&ssd->sesslist, TRUE, 1);
	    dlg_refresh(ssd->editbox, dlg);
	    dlg_refresh(ssd->listbox, dlg);

	    // Here as well
	    conf_set_int(conf, CONF_session_storagetype, 1);
	}
    }
}
/** HACK: END **/
#endif

static void loggingbuttons_handler(union control *ctrl, void *dlg,
				   void *data, int event)
{
    int button;
    Conf *conf = (Conf *)data;
    /* This function works just like the standard radio-button handler,
     * but it has to fall back to "no logging" in situations where the
     * configured logging type isn't applicable.
     */
    if (event == EVENT_REFRESH) {
	int logtype = conf_get_int(conf, CONF_logtype);

        for (button = 0; button < ctrl->radio.nbuttons; button++)
            if (logtype == ctrl->radio.buttondata[button].i)
	        break;

	/* We fell off the end, so we lack the configured logging type */
	if (button == ctrl->radio.nbuttons) {
	    button = 0;
	    conf_set_int(conf, CONF_logtype, LGTYP_NONE);
	}
	dlg_radiobutton_set(ctrl, dlg, button);
    } else if (event == EVENT_VALCHANGE) {
        button = dlg_radiobutton_get(ctrl, dlg);
        assert(button >= 0 && button < ctrl->radio.nbuttons);
        conf_set_int(conf, CONF_logtype, ctrl->radio.buttondata[button].i);
    }
}

static void numeric_keypad_handler(union control *ctrl, void *dlg,
				   void *data, int event)
{
    int button;
    Conf *conf = (Conf *)data;
    /*
     * This function works much like the standard radio button
     * handler, but it has to handle two fields in Conf.
     */
    if (event == EVENT_REFRESH) {
	if (conf_get_int(conf, CONF_nethack_keypad))
	    button = 2;
	else if (conf_get_int(conf, CONF_app_keypad))
	    button = 1;
	else
	    button = 0;
	assert(button < ctrl->radio.nbuttons);
	dlg_radiobutton_set(ctrl, dlg, button);
    } else if (event == EVENT_VALCHANGE) {
	button = dlg_radiobutton_get(ctrl, dlg);
	assert(button >= 0 && button < ctrl->radio.nbuttons);
	if (button == 2) {
	    conf_set_int(conf, CONF_app_keypad, FALSE);
	    conf_set_int(conf, CONF_nethack_keypad, TRUE);
	} else {
	    conf_set_int(conf, CONF_app_keypad, (button != 0));
	    conf_set_int(conf, CONF_nethack_keypad, FALSE);
	}
    }
}

static void cipherlist_handler(union control *ctrl, void *dlg,
			       void *data, int event)
{
    Conf *conf = (Conf *)data;
    if (event == EVENT_REFRESH) {
	int i;

	static const struct { const char *s; int c; } ciphers[] = {
            { "ChaCha20 (SSH-2 only)",  CIPHER_CHACHA20 },
	    { "3DES",			CIPHER_3DES },
	    { "Blowfish",		CIPHER_BLOWFISH },
	    { "DES",			CIPHER_DES },
	    { "AES (SSH-2 only)",	CIPHER_AES },
	    { "Arcfour (SSH-2 only)",	CIPHER_ARCFOUR },
	    { "-- 이하는 위험함 --",	CIPHER_WARN }
	};

	/* Set up the "selected ciphers" box. */
	/* (cipherlist assumed to contain all ciphers) */
	dlg_update_start(ctrl, dlg);
	dlg_listbox_clear(ctrl, dlg);
	for (i = 0; i < CIPHER_MAX; i++) {
	    int c = conf_get_int_int(conf, CONF_ssh_cipherlist, i);
	    int j;
	    const char *cstr = NULL;
	    for (j = 0; j < (sizeof ciphers) / (sizeof ciphers[0]); j++) {
		if (ciphers[j].c == c) {
		    cstr = ciphers[j].s;
		    break;
		}
	    }
	    dlg_listbox_addwithid(ctrl, dlg, cstr, c);
	}
	dlg_update_done(ctrl, dlg);

    } else if (event == EVENT_VALCHANGE) {
	int i;

	/* Update array to match the list box. */
	for (i=0; i < CIPHER_MAX; i++)
	    conf_set_int_int(conf, CONF_ssh_cipherlist, i,
			     dlg_listbox_getid(ctrl, dlg, i));
    }
}

#ifndef NO_GSSAPI
static void gsslist_handler(union control *ctrl, void *dlg,
			    void *data, int event)
{
    Conf *conf = (Conf *)data;
    if (event == EVENT_REFRESH) {
	int i;

	dlg_update_start(ctrl, dlg);
	dlg_listbox_clear(ctrl, dlg);
	for (i = 0; i < ngsslibs; i++) {
	    int id = conf_get_int_int(conf, CONF_ssh_gsslist, i);
	    assert(id >= 0 && id < ngsslibs);
	    dlg_listbox_addwithid(ctrl, dlg, gsslibnames[id], id);
	}
	dlg_update_done(ctrl, dlg);

    } else if (event == EVENT_VALCHANGE) {
	int i;

	/* Update array to match the list box. */
	for (i=0; i < ngsslibs; i++)
	    conf_set_int_int(conf, CONF_ssh_gsslist, i,
			     dlg_listbox_getid(ctrl, dlg, i));
    }
}
#endif

static void kexlist_handler(union control *ctrl, void *dlg,
			    void *data, int event)
{
    Conf *conf = (Conf *)data;
    if (event == EVENT_REFRESH) {
	int i;

	static const struct { const char *s; int k; } kexes[] = {
	    { "Diffie-Hellman group 1",		KEX_DHGROUP1 },
	    { "Diffie-Hellman group 14",	KEX_DHGROUP14 },
	    { "Diffie-Hellman group exchange",	KEX_DHGEX },
	    { "RSA-based key exchange", 	KEX_RSA },
            { "ECDH key exchange",              KEX_ECDH },
	    { "-- 이하는 위험함 --",		KEX_WARN }
	};

	/* Set up the "kex preference" box. */
	/* (kexlist assumed to contain all algorithms) */
	dlg_update_start(ctrl, dlg);
	dlg_listbox_clear(ctrl, dlg);
	for (i = 0; i < KEX_MAX; i++) {
	    int k = conf_get_int_int(conf, CONF_ssh_kexlist, i);
	    int j;
	    const char *kstr = NULL;
	    for (j = 0; j < (sizeof kexes) / (sizeof kexes[0]); j++) {
		if (kexes[j].k == k) {
		    kstr = kexes[j].s;
		    break;
		}
	    }
	    dlg_listbox_addwithid(ctrl, dlg, kstr, k);
	}
	dlg_update_done(ctrl, dlg);

    } else if (event == EVENT_VALCHANGE) {
	int i;

	/* Update array to match the list box. */
	for (i=0; i < KEX_MAX; i++)
	    conf_set_int_int(conf, CONF_ssh_kexlist, i,
			     dlg_listbox_getid(ctrl, dlg, i));
    }
}

static void hklist_handler(union control *ctrl, void *dlg,
                            void *data, int event)
{
    Conf *conf = (Conf *)data;
    if (event == EVENT_REFRESH) {
        int i;

        static const struct { const char *s; int k; } hks[] = {
            { "Ed25519",               HK_ED25519 },
            { "ECDSA",                 HK_ECDSA },
            { "DSA",                   HK_DSA },
            { "RSA",                   HK_RSA },
            { "-- 이하는 위험함 --", HK_WARN }
        };

        /* Set up the "host key preference" box. */
        /* (hklist assumed to contain all algorithms) */
        dlg_update_start(ctrl, dlg);
        dlg_listbox_clear(ctrl, dlg);
        for (i = 0; i < HK_MAX; i++) {
            int k = conf_get_int_int(conf, CONF_ssh_hklist, i);
            int j;
            const char *kstr = NULL;
            for (j = 0; j < lenof(hks); j++) {
                if (hks[j].k == k) {
                    kstr = hks[j].s;
                    break;
                }
            }
            dlg_listbox_addwithid(ctrl, dlg, kstr, k);
        }
        dlg_update_done(ctrl, dlg);

    } else if (event == EVENT_VALCHANGE) {
        int i;

        /* Update array to match the list box. */
        for (i=0; i < HK_MAX; i++)
            conf_set_int_int(conf, CONF_ssh_hklist, i,
                             dlg_listbox_getid(ctrl, dlg, i));
    }
}

static void printerbox_handler(union control *ctrl, void *dlg,
			       void *data, int event)
{
    Conf *conf = (Conf *)data;
    if (event == EVENT_REFRESH) {
	int nprinters, i;
	printer_enum *pe;
	const char *printer;

	dlg_update_start(ctrl, dlg);
	/*
	 * Some backends may wish to disable the drop-down list on
	 * this edit box. Be prepared for this.
	 */
	if (ctrl->editbox.has_list) {
	    dlg_listbox_clear(ctrl, dlg);
	    dlg_listbox_add(ctrl, dlg, PRINTER_DISABLED_STRING);
	    pe = printer_start_enum(&nprinters);
	    for (i = 0; i < nprinters; i++)
		dlg_listbox_add(ctrl, dlg, printer_get_name(pe, i));
	    printer_finish_enum(pe);
	}
	printer = conf_get_str(conf, CONF_printer);
	if (!printer)
	    printer = PRINTER_DISABLED_STRING;
	dlg_editbox_set(ctrl, dlg, printer);
	dlg_update_done(ctrl, dlg);
    } else if (event == EVENT_VALCHANGE) {
	char *printer = dlg_editbox_get(ctrl, dlg);
	if (!strcmp(printer, PRINTER_DISABLED_STRING))
	    printer[0] = '\0';
	conf_set_str(conf, CONF_printer, printer);
	sfree(printer);
    }
}

static void codepage_handler(union control *ctrl, void *dlg,
			     void *data, int event)
{
    Conf *conf = (Conf *)data;
    if (event == EVENT_REFRESH) {
	int i;
	const char *cp, *thiscp;
	dlg_update_start(ctrl, dlg);
	thiscp = cp_name(decode_codepage(conf_get_str(conf,
						      CONF_line_codepage)));
	dlg_listbox_clear(ctrl, dlg);
	for (i = 0; (cp = cp_enumerate(i)) != NULL; i++)
	    dlg_listbox_add(ctrl, dlg, cp);
	dlg_editbox_set(ctrl, dlg, thiscp);
	conf_set_str(conf, CONF_line_codepage, thiscp);
	dlg_update_done(ctrl, dlg);
    } else if (event == EVENT_VALCHANGE) {
	char *codepage = dlg_editbox_get(ctrl, dlg);
	conf_set_str(conf, CONF_line_codepage,
		     cp_name(decode_codepage(codepage)));
	sfree(codepage);
    }
}

static void sshbug_handler(union control *ctrl, void *dlg,
			   void *data, int event)
{
    Conf *conf = (Conf *)data;
    if (event == EVENT_REFRESH) {
        /*
         * We must fetch the previously configured value from the Conf
         * before we start modifying the drop-down list, otherwise the
         * spurious SELCHANGE we trigger in the process will overwrite
         * the value we wanted to keep.
         */
        int oldconf = conf_get_int(conf, ctrl->listbox.context.i);
	dlg_update_start(ctrl, dlg);
	dlg_listbox_clear(ctrl, dlg);
	dlg_listbox_addwithid(ctrl, dlg, "자동", AUTO);
	dlg_listbox_addwithid(ctrl, dlg, "끄기", FORCE_OFF);
	dlg_listbox_addwithid(ctrl, dlg, "켜기", FORCE_ON);
	switch (oldconf) {
	  case AUTO:      dlg_listbox_select(ctrl, dlg, 0); break;
	  case FORCE_OFF: dlg_listbox_select(ctrl, dlg, 1); break;
	  case FORCE_ON:  dlg_listbox_select(ctrl, dlg, 2); break;
	}
	dlg_update_done(ctrl, dlg);
    } else if (event == EVENT_SELCHANGE) {
	int i = dlg_listbox_index(ctrl, dlg);
	if (i < 0)
	    i = AUTO;
	else
	    i = dlg_listbox_getid(ctrl, dlg, i);
	conf_set_int(conf, ctrl->listbox.context.i, i);
    }
}

static void sessionsaver_data_free(void *ssdv)
{
    struct sessionsaver_data *ssd = (struct sessionsaver_data *)ssdv;
#ifdef PUTTY_WINSTUFF_H
    get_sesslist(&ssd->sesslist, FALSE, 0);
#else
    get_sesslist(&ssd->sesslist, FALSE);
#endif
    sfree(ssd->savedsession);
    sfree(ssd);
}

/* 
 * Helper function to load the session selected in the list box, if
 * any, as this is done in more than one place below. Returns 0 for
 * failure.
 */
static int load_selected_session(struct sessionsaver_data *ssd,
				 void *dlg, Conf *conf, int *maybe_launch)
{
    int i = dlg_listbox_index(ssd->listbox, dlg);
    int isdef;
    if (i < 0) {
	dlg_beep(dlg);
	return 0;
    }
    isdef = !strcmp(ssd->sesslist.sessions[i], "기본 설정");
    load_settings(ssd->sesslist.sessions[i], conf);
    sfree(ssd->savedsession);
    ssd->savedsession = dupstr(isdef ? "" : ssd->sesslist.sessions[i]);
    if (maybe_launch)
        *maybe_launch = !isdef;
    dlg_refresh(NULL, dlg);
    /* Restore the selection, which might have been clobbered by
     * changing the value of the edit box. */
    dlg_listbox_select(ssd->listbox, dlg, i);
    return 1;
}

static void sessionsaver_handler(union control *ctrl, void *dlg,
				 void *data, int event)
{
    Conf *conf = (Conf *)data;
    struct sessionsaver_data *ssd =
	(struct sessionsaver_data *)ctrl->generic.context.p;
#ifdef PUTTY_WINSTUFF_H
    char *savedsession;

    /*
     * The first time we're called in a new dialog, we must
     * allocate space to store the current contents of the saved
     * session edit box (since it must persist even when we switch
     * panels, but is not part of the Config).
     */
    if (!ssd->editbox) {
	savedsession = NULL;
    } else if (!dlg_get_privdata(ssd->editbox, dlg)) {
	savedsession = (char *)
	    dlg_alloc_privdata(ssd->editbox, dlg, SAVEDSESSION_LEN);
	savedsession[0] = '\0';
    } else {
	savedsession = dlg_get_privdata(ssd->editbox, dlg);
    }
#endif

    if (event == EVENT_REFRESH) {
	if (ctrl == ssd->editbox) {
	    dlg_editbox_set(ctrl, dlg, ssd->savedsession);
	} else if (ctrl == ssd->listbox) {
	    int i;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    for (i = 0; i < ssd->sesslist.nsessions; i++)
		dlg_listbox_add(ctrl, dlg, ssd->sesslist.sessions[i]);
	    dlg_update_done(ctrl, dlg);
	}
    } else if (event == EVENT_VALCHANGE) {
        int top, bottom, halfway, i;
	if (ctrl == ssd->editbox) {
            sfree(ssd->savedsession);
            ssd->savedsession = dlg_editbox_get(ctrl, dlg);
	    top = ssd->sesslist.nsessions;
	    bottom = -1;
	    while (top-bottom > 1) {
	        halfway = (top+bottom)/2;
	        i = strcmp(ssd->savedsession, ssd->sesslist.sessions[halfway]);
	        if (i <= 0 ) {
		    top = halfway;
	        } else {
		    bottom = halfway;
	        }
	    }
	    if (top == ssd->sesslist.nsessions) {
	        top -= 1;
	    }
	    dlg_listbox_select(ssd->listbox, dlg, top);
	}
    } else if (event == EVENT_ACTION) {
	int mbl = FALSE;
	if (!ssd->midsession &&
	    (ctrl == ssd->listbox ||
	     (ssd->loadbutton && ctrl == ssd->loadbutton))) {
	    /*
	     * The user has double-clicked a session, or hit Load.
	     * We must load the selected session, and then
	     * terminate the configuration dialog _if_ there was a
	     * double-click on the list box _and_ that session
	     * contains a hostname.
	     */
	    if (load_selected_session(ssd, dlg, conf, &mbl) &&
		(mbl && ctrl == ssd->listbox && conf_launchable(conf))) {
		dlg_end(dlg, 1);       /* it's all over, and succeeded */
	    }
	} else if (ctrl == ssd->savebutton) {
	    int isdef = !strcmp(ssd->savedsession, "기본 설정");
	    if (!ssd->savedsession[0]) {
		int i = dlg_listbox_index(ssd->listbox, dlg);
		if (i < 0) {
		    dlg_beep(dlg);
		    return;
		}
		isdef = !strcmp(ssd->sesslist.sessions[i], "기본 설정");
                sfree(ssd->savedsession);
                ssd->savedsession = dupstr(isdef ? "" :
                                           ssd->sesslist.sessions[i]);
	    }
            {
                char *errmsg = save_settings(ssd->savedsession, conf);
                if (errmsg) {
                    dlg_error_msg(dlg, errmsg);
                    sfree(errmsg);
                }
            }

#ifdef PUTTY_WINSTUFF_H
	    /*
	     * HACK: PuttyTray / PuTTY File
	     * Added storagetype to get_sesslist
	     */
	    get_sesslist(&ssd->sesslist, FALSE, conf_get_int(conf, CONF_session_storagetype));
	    get_sesslist(&ssd->sesslist, TRUE, conf_get_int(conf, CONF_session_storagetype));
#else
            get_sesslist(&ssd->sesslist, FALSE);
            get_sesslist(&ssd->sesslist, TRUE);
#endif
	    dlg_refresh(ssd->editbox, dlg);
	    dlg_refresh(ssd->listbox, dlg);
	} else if (!ssd->midsession &&
		   ssd->delbutton && ctrl == ssd->delbutton) {
	    int i = dlg_listbox_index(ssd->listbox, dlg);
	    if (i <= 0) {
		dlg_beep(dlg);
	    } else {
		del_settings(ssd->sesslist.sessions[i]);

#ifdef PUTTY_WINSTUFF_H
		/*
		 * HACK: PuttyTray / PuTTY File
		 * Added storagetype to get_sesslist
		 */
		get_sesslist(&ssd->sesslist, FALSE, conf_get_int(conf, CONF_session_storagetype));
		get_sesslist(&ssd->sesslist, TRUE, conf_get_int(conf, CONF_session_storagetype));
#else
                get_sesslist(&ssd->sesslist, FALSE);
                get_sesslist(&ssd->sesslist, TRUE);
#endif
		dlg_refresh(ssd->listbox, dlg);
	    }
	} else if (ctrl == ssd->okbutton) {
            if (ssd->midsession) {
                /* In a mid-session Change Settings, Apply is always OK. */
		dlg_end(dlg, 1);
                return;
            }
	    /*
	     * Annoying special case. If the `Open' button is
	     * pressed while no host name is currently set, _and_
	     * the session list previously had the focus, _and_
	     * there was a session selected in that which had a
	     * valid host name in it, then load it and go.
	     */
	    if (dlg_last_focused(ctrl, dlg) == ssd->listbox &&
		!conf_launchable(conf)) {
		Conf *conf2 = conf_new();
		int mbl = FALSE;
		if (!load_selected_session(ssd, dlg, conf2, &mbl)) {
		    dlg_beep(dlg);
		    conf_free(conf2);
		    return;
		}
		/* If at this point we have a valid session, go! */
		if (mbl && conf_launchable(conf2)) {
		    conf_copy_into(conf, conf2);
		    dlg_end(dlg, 1);
		} else
		    dlg_beep(dlg);

		conf_free(conf2);
                return;
	    }

	    /*
	     * Otherwise, do the normal thing: if we have a valid
	     * session, get going.
	     */
	    if (conf_launchable(conf)) {
		dlg_end(dlg, 1);
	    } else
		dlg_beep(dlg);
	} else if (ctrl == ssd->cancelbutton) {
	    dlg_end(dlg, 0);
	}
    }
}

struct charclass_data {
    union control *listbox, *editbox, *button;
};

static void charclass_handler(union control *ctrl, void *dlg,
			      void *data, int event)
{
    Conf *conf = (Conf *)data;
    struct charclass_data *ccd =
	(struct charclass_data *)ctrl->generic.context.p;

    if (event == EVENT_REFRESH) {
	if (ctrl == ccd->listbox) {
	    int i;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    for (i = 0; i < 128; i++) {
		char str[100];
		sprintf(str, "%d\t(0x%02X)\t%c\t%d", i, i,
			(i >= 0x21 && i != 0x7F) ? i : ' ',
			conf_get_int_int(conf, CONF_wordness, i));
		dlg_listbox_add(ctrl, dlg, str);
	    }
	    dlg_update_done(ctrl, dlg);
	}
    } else if (event == EVENT_ACTION) {
	if (ctrl == ccd->button) {
	    char *str;
	    int i, n;
	    str = dlg_editbox_get(ccd->editbox, dlg);
	    n = atoi(str);
	    sfree(str);
	    for (i = 0; i < 128; i++) {
		if (dlg_listbox_issel(ccd->listbox, dlg, i))
		    conf_set_int_int(conf, CONF_wordness, i, n);
	    }
	    dlg_refresh(ccd->listbox, dlg);
	}
    }
}

struct colour_data {
    union control *listbox, *redit, *gedit, *bedit, *button;
};

static const char *const colours[] = {
    "Default Foreground", "Default Bold Foreground",
    "Default Background", "Default Bold Background",
    "Cursor Text", "Cursor Colour",
    "ANSI Black", "ANSI Black Bold",
    "ANSI Red", "ANSI Red Bold",
    "ANSI Green", "ANSI Green Bold",
    "ANSI Yellow", "ANSI Yellow Bold",
    "ANSI Blue", "ANSI Blue Bold",
    "ANSI Magenta", "ANSI Magenta Bold",
    "ANSI Cyan", "ANSI Cyan Bold",
    "ANSI White", "ANSI White Bold"
};

static void colour_handler(union control *ctrl, void *dlg,
			    void *data, int event)
{
    Conf *conf = (Conf *)data;
    struct colour_data *cd =
	(struct colour_data *)ctrl->generic.context.p;
    int update = FALSE, clear = FALSE, r, g, b;

    if (event == EVENT_REFRESH) {
	if (ctrl == cd->listbox) {
	    int i;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    for (i = 0; i < lenof(colours); i++)
		dlg_listbox_add(ctrl, dlg, colours[i]);
	    dlg_update_done(ctrl, dlg);
	    clear = TRUE;
	    update = TRUE;
	}
    } else if (event == EVENT_SELCHANGE) {
	if (ctrl == cd->listbox) {
	    /* The user has selected a colour. Update the RGB text. */
	    int i = dlg_listbox_index(ctrl, dlg);
	    if (i < 0) {
		clear = TRUE;
	    } else {
		clear = FALSE;
		r = conf_get_int_int(conf, CONF_colours, i*3+0);
		g = conf_get_int_int(conf, CONF_colours, i*3+1);
		b = conf_get_int_int(conf, CONF_colours, i*3+2);
	    }
	    update = TRUE;
	}
    } else if (event == EVENT_VALCHANGE) {
	if (ctrl == cd->redit || ctrl == cd->gedit || ctrl == cd->bedit) {
	    /* The user has changed the colour using the edit boxes. */
	    char *str;
	    int i, cval;

	    str = dlg_editbox_get(ctrl, dlg);
	    cval = atoi(str);
	    sfree(str);
	    if (cval > 255) cval = 255;
	    if (cval < 0)   cval = 0;

	    i = dlg_listbox_index(cd->listbox, dlg);
	    if (i >= 0) {
		if (ctrl == cd->redit)
		    conf_set_int_int(conf, CONF_colours, i*3+0, cval);
		else if (ctrl == cd->gedit)
		    conf_set_int_int(conf, CONF_colours, i*3+1, cval);
		else if (ctrl == cd->bedit)
		    conf_set_int_int(conf, CONF_colours, i*3+2, cval);
	    }
	}
    } else if (event == EVENT_ACTION) {
	if (ctrl == cd->button) {
	    int i = dlg_listbox_index(cd->listbox, dlg);
	    if (i < 0) {
		dlg_beep(dlg);
		return;
	    }
	    /*
	     * Start a colour selector, which will send us an
	     * EVENT_CALLBACK when it's finished and allow us to
	     * pick up the results.
	     */
	    dlg_coloursel_start(ctrl, dlg,
				conf_get_int_int(conf, CONF_colours, i*3+0),
				conf_get_int_int(conf, CONF_colours, i*3+1),
				conf_get_int_int(conf, CONF_colours, i*3+2));
	}
    } else if (event == EVENT_CALLBACK) {
	if (ctrl == cd->button) {
	    int i = dlg_listbox_index(cd->listbox, dlg);
	    /*
	     * Collect the results of the colour selector. Will
	     * return nonzero on success, or zero if the colour
	     * selector did nothing (user hit Cancel, for example).
	     */
	    if (dlg_coloursel_results(ctrl, dlg, &r, &g, &b)) {
		conf_set_int_int(conf, CONF_colours, i*3+0, r);
		conf_set_int_int(conf, CONF_colours, i*3+1, g);
		conf_set_int_int(conf, CONF_colours, i*3+2, b);
		clear = FALSE;
		update = TRUE;
	    }
	}
    }

    if (update) {
	if (clear) {
	    dlg_editbox_set(cd->redit, dlg, "");
	    dlg_editbox_set(cd->gedit, dlg, "");
	    dlg_editbox_set(cd->bedit, dlg, "");
	} else {
	    char buf[40];
	    sprintf(buf, "%d", r); dlg_editbox_set(cd->redit, dlg, buf);
	    sprintf(buf, "%d", g); dlg_editbox_set(cd->gedit, dlg, buf);
	    sprintf(buf, "%d", b); dlg_editbox_set(cd->bedit, dlg, buf);
	}
    }
}

struct ttymodes_data {
    union control *valradio, *valbox, *setbutton, *listbox;
};

static void ttymodes_handler(union control *ctrl, void *dlg,
			     void *data, int event)
{
    Conf *conf = (Conf *)data;
    struct ttymodes_data *td =
	(struct ttymodes_data *)ctrl->generic.context.p;

    if (event == EVENT_REFRESH) {
	if (ctrl == td->listbox) {
	    char *key, *val;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    for (val = conf_get_str_strs(conf, CONF_ttymodes, NULL, &key);
		 val != NULL;
		 val = conf_get_str_strs(conf, CONF_ttymodes, key, &key)) {
		char *disp = dupprintf("%s\t%s", key,
				       (val[0] == 'A') ? "(auto)" :
				       ((val[0] == 'N') ? "(don't send)"
							: val+1));
		dlg_listbox_add(ctrl, dlg, disp);
		sfree(disp);
	    }
	    dlg_update_done(ctrl, dlg);
	} else if (ctrl == td->valradio) {
	    dlg_radiobutton_set(ctrl, dlg, 0);
	}
    } else if (event == EVENT_SELCHANGE) {
	if (ctrl == td->listbox) {
	    int ind = dlg_listbox_index(td->listbox, dlg);
	    char *val;
	    if (ind < 0) {
		return; /* no item selected */
	    }
	    val = conf_get_str_str(conf, CONF_ttymodes,
				   conf_get_str_nthstrkey(conf, CONF_ttymodes,
							  ind));
	    assert(val != NULL);
	    /* Do this first to defuse side-effects on radio buttons: */
	    dlg_editbox_set(td->valbox, dlg, val+1);
	    dlg_radiobutton_set(td->valradio, dlg,
				val[0] == 'A' ? 0 : (val[0] == 'N' ? 1 : 2));
	}
    } else if (event == EVENT_VALCHANGE) {
	if (ctrl == td->valbox) {
	    /* If they're editing the text box, we assume they want its
	     * value to be used. */
	    dlg_radiobutton_set(td->valradio, dlg, 2);
	}
    } else if (event == EVENT_ACTION) {
	if (ctrl == td->setbutton) {
	    int ind = dlg_listbox_index(td->listbox, dlg);
	    const char *key;
	    char *str, *val;
	    char type;

	    {
		const char *types = "ANV";
		int button = dlg_radiobutton_get(td->valradio, dlg);
		assert(button >= 0 && button < lenof(types));
		type = types[button];
	    }

	    /* Construct new entry */
	    if (ind >= 0) {
		key = conf_get_str_nthstrkey(conf, CONF_ttymodes, ind);
		str = (type == 'V' ? dlg_editbox_get(td->valbox, dlg)
				   : dupstr(""));
		val = dupprintf("%c%s", type, str);
		sfree(str);
		conf_set_str_str(conf, CONF_ttymodes, key, val);
		sfree(val);
		dlg_refresh(td->listbox, dlg);
		dlg_listbox_select(td->listbox, dlg, ind);
	    } else {
		/* Not a multisel listbox, so this means nothing selected */
		dlg_beep(dlg);
	    }
	}
    }
}

struct environ_data {
    union control *varbox, *valbox, *addbutton, *rembutton, *listbox;
};

static void environ_handler(union control *ctrl, void *dlg,
			    void *data, int event)
{
    Conf *conf = (Conf *)data;
    struct environ_data *ed =
	(struct environ_data *)ctrl->generic.context.p;

    if (event == EVENT_REFRESH) {
	if (ctrl == ed->listbox) {
	    char *key, *val;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    for (val = conf_get_str_strs(conf, CONF_environmt, NULL, &key);
		 val != NULL;
		 val = conf_get_str_strs(conf, CONF_environmt, key, &key)) {
		char *p = dupprintf("%s\t%s", key, val);
		dlg_listbox_add(ctrl, dlg, p);
		sfree(p);
	    }
	    dlg_update_done(ctrl, dlg);
	}
    } else if (event == EVENT_ACTION) {
	if (ctrl == ed->addbutton) {
	    char *key, *val, *str;
	    key = dlg_editbox_get(ed->varbox, dlg);
	    if (!*key) {
		sfree(key);
		dlg_beep(dlg);
		return;
	    }
	    val = dlg_editbox_get(ed->valbox, dlg);
	    if (!*val) {
		sfree(key);
		sfree(val);
		dlg_beep(dlg);
		return;
	    }
	    conf_set_str_str(conf, CONF_environmt, key, val);
	    str = dupcat(key, "\t", val, NULL);
	    dlg_editbox_set(ed->varbox, dlg, "");
	    dlg_editbox_set(ed->valbox, dlg, "");
	    sfree(str);
	    sfree(key);
	    sfree(val);
	    dlg_refresh(ed->listbox, dlg);
	} else if (ctrl == ed->rembutton) {
	    int i = dlg_listbox_index(ed->listbox, dlg);
	    if (i < 0) {
		dlg_beep(dlg);
	    } else {
		char *key, *val;

		key = conf_get_str_nthstrkey(conf, CONF_environmt, i);
		if (key) {
		    /* Populate controls with the entry we're about to delete
		     * for ease of editing */
		    val = conf_get_str_str(conf, CONF_environmt, key);
		    dlg_editbox_set(ed->varbox, dlg, key);
		    dlg_editbox_set(ed->valbox, dlg, val);
		    /* And delete it */
		    conf_del_str_str(conf, CONF_environmt, key);
		}
	    }
	    dlg_refresh(ed->listbox, dlg);
	}
    }
}

struct portfwd_data {
    union control *addbutton, *rembutton, *listbox;
    union control *sourcebox, *destbox, *direction;
#ifndef NO_IPV6
    union control *addressfamily;
#endif
};

static void portfwd_handler(union control *ctrl, void *dlg,
			    void *data, int event)
{
    Conf *conf = (Conf *)data;
    struct portfwd_data *pfd =
	(struct portfwd_data *)ctrl->generic.context.p;

    if (event == EVENT_REFRESH) {
	if (ctrl == pfd->listbox) {
	    char *key, *val;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    for (val = conf_get_str_strs(conf, CONF_portfwd, NULL, &key);
		 val != NULL;
		 val = conf_get_str_strs(conf, CONF_portfwd, key, &key)) {
		char *p;
                if (!strcmp(val, "D")) {
                    char *L;
                    /*
                     * A dynamic forwarding is stored as L12345=D or
                     * 6L12345=D (since it's mutually exclusive with
                     * L12345=anything else), but displayed as D12345
                     * to match the fiction that 'Local', 'Remote' and
                     * 'Dynamic' are three distinct modes and also to
                     * align with OpenSSH's command line option syntax
                     * that people will already be used to. So, for
                     * display purposes, find the L in the key string
                     * and turn it into a D.
                     */
                    p = dupprintf("%s\t", key);
                    L = strchr(p, 'L');
                    if (L) *L = 'D';
                } else
                    p = dupprintf("%s\t%s", key, val);
		dlg_listbox_add(ctrl, dlg, p);
		sfree(p);
	    }
	    dlg_update_done(ctrl, dlg);
	} else if (ctrl == pfd->direction) {
	    /*
	     * Default is Local.
	     */
	    dlg_radiobutton_set(ctrl, dlg, 0);
#ifndef NO_IPV6
	} else if (ctrl == pfd->addressfamily) {
	    dlg_radiobutton_set(ctrl, dlg, 0);
#endif
	}
    } else if (event == EVENT_ACTION) {
	if (ctrl == pfd->addbutton) {
	    const char *family, *type;
            char *src, *key, *val;
	    int whichbutton;

#ifndef NO_IPV6
	    whichbutton = dlg_radiobutton_get(pfd->addressfamily, dlg);
	    if (whichbutton == 1)
		family = "4";
	    else if (whichbutton == 2)
		family = "6";
	    else
#endif
		family = "";

	    whichbutton = dlg_radiobutton_get(pfd->direction, dlg);
	    if (whichbutton == 0)
		type = "L";
	    else if (whichbutton == 1)
		type = "R";
	    else
		type = "D";

	    src = dlg_editbox_get(pfd->sourcebox, dlg);
	    if (!*src) {
		dlg_error_msg(dlg, "You need to specify a source port number");
		sfree(src);
		return;
	    }
	    if (*type != 'D') {
		val = dlg_editbox_get(pfd->destbox, dlg);
		if (!*val || !host_strchr(val, ':')) {
		    dlg_error_msg(dlg,
				  "You need to specify a destination address\n"
				  "in the form \"host.name:port\"");
		    sfree(src);
		    sfree(val);
		    return;
		}
	    } else {
                type = "L";
		val = dupstr("D");     /* special case */
            }

	    key = dupcat(family, type, src, NULL);
	    sfree(src);

	    if (conf_get_str_str_opt(conf, CONF_portfwd, key)) {
		dlg_error_msg(dlg, "Specified forwarding already exists");
	    } else {
		conf_set_str_str(conf, CONF_portfwd, key, val);
	    }

	    sfree(key);
	    sfree(val);
	    dlg_refresh(pfd->listbox, dlg);
	} else if (ctrl == pfd->rembutton) {
	    int i = dlg_listbox_index(pfd->listbox, dlg);
	    if (i < 0) {
		dlg_beep(dlg);
	    } else {
		char *key, *p;
                const char *val;

		key = conf_get_str_nthstrkey(conf, CONF_portfwd, i);
		if (key) {
		    static const char *const afs = "A46";
		    static const char *const dirs = "LRD";
		    char *afp;
		    int dir;
#ifndef NO_IPV6
		    int idx;
#endif

		    /* Populate controls with the entry we're about to delete
		     * for ease of editing */
		    p = key;

		    afp = strchr(afs, *p);
#ifndef NO_IPV6
		    idx = afp ? afp-afs : 0;
#endif
		    if (afp)
			p++;
#ifndef NO_IPV6
		    dlg_radiobutton_set(pfd->addressfamily, dlg, idx);
#endif

		    dir = *p;

                    val = conf_get_str_str(conf, CONF_portfwd, key);
		    if (!strcmp(val, "D")) {
                        dir = 'D';
			val = "";
		    }

		    dlg_radiobutton_set(pfd->direction, dlg,
					strchr(dirs, dir) - dirs);
		    p++;

		    dlg_editbox_set(pfd->sourcebox, dlg, p);
		    dlg_editbox_set(pfd->destbox, dlg, val);
		    /* And delete it */
		    conf_del_str_str(conf, CONF_portfwd, key);
		}
	    }
	    dlg_refresh(pfd->listbox, dlg);
	}
    }
}

struct manual_hostkey_data {
    union control *addbutton, *rembutton, *listbox, *keybox;
};

static void manual_hostkey_handler(union control *ctrl, void *dlg,
                                   void *data, int event)
{
    Conf *conf = (Conf *)data;
    struct manual_hostkey_data *mh =
	(struct manual_hostkey_data *)ctrl->generic.context.p;

    if (event == EVENT_REFRESH) {
	if (ctrl == mh->listbox) {
	    char *key, *val;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    for (val = conf_get_str_strs(conf, CONF_ssh_manual_hostkeys,
                                         NULL, &key);
		 val != NULL;
		 val = conf_get_str_strs(conf, CONF_ssh_manual_hostkeys,
                                         key, &key)) {
		dlg_listbox_add(ctrl, dlg, key);
	    }
	    dlg_update_done(ctrl, dlg);
	}
    } else if (event == EVENT_ACTION) {
	if (ctrl == mh->addbutton) {
	    char *key;

	    key = dlg_editbox_get(mh->keybox, dlg);
	    if (!*key) {
		dlg_error_msg(dlg, "You need to specify a host key or "
                              "fingerprint");
		sfree(key);
		return;
	    }

            if (!validate_manual_hostkey(key)) {
		dlg_error_msg(dlg, "Host key is not in a valid format");
            } else if (conf_get_str_str_opt(conf, CONF_ssh_manual_hostkeys,
                                            key)) {
		dlg_error_msg(dlg, "Specified host key is already listed");
	    } else {
		conf_set_str_str(conf, CONF_ssh_manual_hostkeys, key, "");
	    }

	    sfree(key);
	    dlg_refresh(mh->listbox, dlg);
	} else if (ctrl == mh->rembutton) {
	    int i = dlg_listbox_index(mh->listbox, dlg);
	    if (i < 0) {
		dlg_beep(dlg);
	    } else {
		char *key;

		key = conf_get_str_nthstrkey(conf, CONF_ssh_manual_hostkeys, i);
		if (key) {
		    dlg_editbox_set(mh->keybox, dlg, key);
		    /* And delete it */
		    conf_del_str_str(conf, CONF_ssh_manual_hostkeys, key);
		}
	    }
	    dlg_refresh(mh->listbox, dlg);
	}
    }
}

#ifdef PUTTY_WINSTUFF_H
void setup_config_box(struct controlbox *b, int midsession,
		      int protocol, int protcfginfo, int session_storagetype) // HACK: PuttyTray / PuTTY File - Added 'int session_storagetype'
#else
void setup_config_box(struct controlbox *b, int midsession,
		      int protocol, int protcfginfo)
#endif
{
    struct controlset *s;
    struct sessionsaver_data *ssd;
    struct charclass_data *ccd;
    struct colour_data *cd;
    struct ttymodes_data *td;
    struct environ_data *ed;
    struct portfwd_data *pfd;
    struct manual_hostkey_data *mh;
    union control *c;
    char *str;
#ifdef PUTTY_WINSTUFF_H
    int current_storagetype; // HACK: PuttyTray / PuTTY File - stores storagetype after sess_list may or may not have switched between file/registry because registry is empty
#endif

    ssd = (struct sessionsaver_data *)
	ctrl_alloc_with_free(b, sizeof(struct sessionsaver_data),
                             sessionsaver_data_free);
    memset(ssd, 0, sizeof(*ssd));
    ssd->savedsession = dupstr("");
    ssd->midsession = midsession;

    /*
     * The standard panel that appears at the bottom of all panels:
     * Open, Cancel, Apply etc.
     */
    s = ctrl_getset(b, "", "", "");
    ctrl_columns(s, 5, 20, 20, 20, 20, 20);
    ssd->okbutton = ctrl_pushbutton(s,
				    (midsession ? "적용" : "열기"),
				    (char)(midsession ? 'a' : 'o'),
				    HELPCTX(no_help),
				    sessionsaver_handler, P(ssd));
    ssd->okbutton->button.isdefault = TRUE;
    ssd->okbutton->generic.column = 3;
    ssd->cancelbutton = ctrl_pushbutton(s, "취소", 'c', HELPCTX(no_help),
					sessionsaver_handler, P(ssd));
    ssd->cancelbutton->button.iscancel = TRUE;
    ssd->cancelbutton->generic.column = 4;
    /* We carefully don't close the 5-column part, so that platform-
     * specific add-ons can put extra buttons alongside Open and Cancel. */

    /*
     * The Session panel.
     */
    str = dupprintf("%s 세션 기본 옵션", appname);
    ctrl_settitle(b, "세션", str);
    sfree(str);

    if (!midsession) {
	struct hostport *hp = (struct hostport *)
	    ctrl_alloc(b, sizeof(struct hostport));

	s = ctrl_getset(b, "세션", "hostport",
			"기본 접속 정보");
	ctrl_columns(s, 2, 75, 25);
	c = ctrl_editbox(s, HOST_BOX_TITLE, 'n', 100,
			 HELPCTX(session_hostname),
			 config_host_handler, I(0), I(0));
	c->generic.column = 0;
	hp->host = c;
	c = ctrl_editbox(s, PORT_BOX_TITLE, 'p', 100,
			 HELPCTX(session_hostname),
			 config_port_handler, I(0), I(0));
	c->generic.column = 1;
	hp->port = c;
	ctrl_columns(s, 1, 100);

	if (!backend_from_proto(PROT_SSH)) {
	    ctrl_radiobuttons(s, "연결 형식:", NO_SHORTCUT, 3,
			      HELPCTX(session_hostname),
			      config_protocolbuttons_handler, P(hp),
			      "Raw", 'w', I(PROT_RAW),
			      "Telnet", 't', I(PROT_TELNET),
			      "Rlogin", 'i', I(PROT_RLOGIN),
			      NULL);
	} else {
	    ctrl_radiobuttons(s, "연결 형식:", NO_SHORTCUT, 4,
			      HELPCTX(session_hostname),
			      config_protocolbuttons_handler, P(hp),
			      "Raw", 'w', I(PROT_RAW),
			      "Telnet", 't', I(PROT_TELNET),
			      "Rlogin", 'i', I(PROT_RLOGIN),
			      "SSH", 's', I(PROT_SSH),
			      NULL);
	}
    }

    /*
     * The Load/Save panel is available even in mid-session.
     */
    s = ctrl_getset(b, "세션", "savedsessions",
		    midsession ? "현재 세션 설정을 저장" :
		    "불러오기, 저장, 저장된 세션 삭제");
    ctrl_columns(s, 2, 75, 25);

#ifdef PUTTY_WINSTUFF_H
    current_storagetype = get_sesslist(&ssd->sesslist, TRUE, (midsession ? session_storagetype : (session_storagetype + 2))); // HACK: PuttyTray / PuTTY File - The +2 triggers storagetype autoswitching
#endif
    ssd->editbox = ctrl_editbox(s, "저장된 세션", 'e', 100,
				HELPCTX(session_saved),
				sessionsaver_handler, P(ssd), P(NULL));
    ssd->editbox->generic.column = 0;
    /* Reset columns so that the buttons are alongside the list, rather
     * than alongside that edit box. */
    ctrl_columns(s, 1, 100);
    ctrl_columns(s, 2, 75, 25);
    ssd->listbox = ctrl_listbox(s, NULL, NO_SHORTCUT,
				HELPCTX(session_saved),
				sessionsaver_handler, P(ssd));
    ssd->listbox->generic.column = 0;
    ssd->listbox->listbox.height = 7;
    // CYGTERM patch
    if (cygterm_get_flag()) ssd->listbox->listbox.height--;
    if (!midsession) {
	ssd->loadbutton = ctrl_pushbutton(s, "불러오기", 'l',
					  HELPCTX(session_saved),
					  sessionsaver_handler, P(ssd));
	ssd->loadbutton->generic.column = 1;
    } else {
	/* We can't offer the Load button mid-session, as it would allow the
	 * user to load and subsequently save settings they can't see. (And
	 * also change otherwise immutable settings underfoot; that probably
	 * shouldn't be a problem, but.) */
	ssd->loadbutton = NULL;
    }
    /* "Save" button is permitted mid-session. */
    ssd->savebutton = ctrl_pushbutton(s, "저장", 'v',
				      HELPCTX(session_saved),
				      sessionsaver_handler, P(ssd));
    ssd->savebutton->generic.column = 1;
    if (!midsession) {
	ssd->delbutton = ctrl_pushbutton(s, "삭제", 'd',
					 HELPCTX(session_saved),
					 sessionsaver_handler, P(ssd));
	ssd->delbutton->generic.column = 1;
    } else {
	/* Disable the Delete button mid-session too, for UI consistency. */
	ssd->delbutton = NULL;
    }
    ctrl_columns(s, 1, 100);

#ifdef PUTTY_WINSTUFF_H
    /*
     * HACK: PuttyTray / PuTTY File
     * Add radio buttons
     *
     * Couldn't get the default selection to switch, so I switched the button position instead.
     * Must be the lamest solution I ever came up with.
     *
     * In midsession, changing causes it to be reversed again (wrong). So don't.
     */
    if (midsession || current_storagetype == 0) {
	c = ctrl_radiobuttons(s, NULL, 'w', 2,
			      HELPCTX(no_help),
			      storagetype_handler,
			      P(ssd), "레지스트리에 관리", I(0), "파일로 관리", I(1), NULL);
    } else {
	c = ctrl_radiobuttons(s, NULL, 'w', 2,
			      HELPCTX(no_help),
			      storagetype_handler,
			      P(ssd), "파일로 관리", I(1), "레지스트리에 관리", I(0), NULL);
    }
    /** HACK: END **/
#endif

    s = ctrl_getset(b, "세션", "otheropts", NULL);
    ctrl_radiobuttons(s, "종료시 창 닫기:", 'x', 4,
                      HELPCTX(session_coe),
                      conf_radiobutton_handler,
                      I(CONF_close_on_exit),
                      "항상", I(FORCE_ON),
                      "안함", I(FORCE_OFF),
                      "명확한 종료 시에만", I(AUTO), NULL);

    /*
     * The Session/Logging panel.
     */
    ctrl_settitle(b, "세션/로깅", "세션 로깅 조절 설정");

    s = ctrl_getset(b, "세션/로깅", "main", NULL);
    /*
     * The logging buttons change depending on whether SSH packet
     * logging can sensibly be available.
     */
    {
	const char *sshlogname, *sshrawlogname;
	if ((midsession && protocol == PROT_SSH) ||
	    (!midsession && backend_from_proto(PROT_SSH))) {
	    sshlogname = "SSH 패킷";
	    sshrawlogname = "SSH 패킷과 raw 데이타";
        } else {
	    sshlogname = NULL;	       /* this will disable both buttons */
	    sshrawlogname = NULL;      /* this will just placate optimisers */
        }
	ctrl_radiobuttons(s, "세션 로깅:", NO_SHORTCUT, 2,
			  HELPCTX(logging_main),
			  loggingbuttons_handler,
			  I(CONF_logtype),
			  "안함", 't', I(LGTYP_NONE),
			  "출력 가능한 글자만", 'p', I(LGTYP_ASCII),
			  "세션의 모든 출력", 'l', I(LGTYP_DEBUG),
			  sshlogname, 's', I(LGTYP_PACKETS),
			  sshrawlogname, 'r', I(LGTYP_SSHRAW),
			  NULL);
    }
    ctrl_filesel(s, "로그 파일 이름:", 'f',
		 NULL, TRUE, "로그 파일 이름 선택",
		 HELPCTX(logging_filename),
		 conf_filesel_handler, I(CONF_logfilename));
    ctrl_text(s, "(로그 파일 이름에 날자(&Y, &M, &D),"
	      " 시간(&T), 호스트 이름(&H), 포트(&P)를 포함할 수 있습니다.)",
	      HELPCTX(logging_filename));
    ctrl_radiobuttons(s, "로그 파일이 이미 존재할 경우:", 'e', 1,
		      HELPCTX(logging_exists),
		      conf_radiobutton_handler, I(CONF_logxfovr),
		      "항상 덮어쓰기", I(LGXF_OVR),
		      "항상 파일의 끝에 추가하기", I(LGXF_APN),
		      "매번 물어보기", I(LGXF_ASK), NULL);
    ctrl_checkbox(s, "로그 파일을 자주 동기화", 'u',
		 HELPCTX(logging_flush),
		 conf_checkbox_handler, I(CONF_logflush));

    if ((midsession && protocol == PROT_SSH) ||
	(!midsession && backend_from_proto(PROT_SSH))) {
	s = ctrl_getset(b, "세션/로깅", "ssh",
			"SSH 패킷 로깅 관련 옵션");
	ctrl_checkbox(s, "암호 필드로 보이는 것은 생략", 'k',
		      HELPCTX(logging_ssh_omit_password),
		      conf_checkbox_handler, I(CONF_logomitpass));
	ctrl_checkbox(s, "세션 데이타 생략", 'd',
		      HELPCTX(logging_ssh_omit_data),
		      conf_checkbox_handler, I(CONF_logomitdata));
    }

    /*
     * The Terminal panel.
     */
    ctrl_settitle(b, "터미널", "터미널 에뮬레이션 제어 옵션");

    s = ctrl_getset(b, "터미널", "general", "다양한 터미널 옵션 설정");
    ctrl_checkbox(s, "기본으로 자동 줄 넘김", 'w',
		  HELPCTX(terminal_autowrap),
		  conf_checkbox_handler, I(CONF_wrap_mode));
    ctrl_checkbox(s, "DEC 줄 모드를 기본으로 활성화(&D)", 'd',
		  HELPCTX(terminal_decom),
		  conf_checkbox_handler, I(CONF_dec_om));
    ctrl_checkbox(s, "CR이 없어도 LF가 발견되면 줄넘김 인정", 'r',
		  HELPCTX(terminal_lfhascr),
		  conf_checkbox_handler, I(CONF_lfhascr));
    ctrl_checkbox(s, "LF가 없어도 CR이 발견되면 줄넘김 인정", 'f',
		  HELPCTX(terminal_crhaslf),
		  conf_checkbox_handler, I(CONF_crhaslf));
    ctrl_checkbox(s, "배경색으로 화면 지우기", 'e',
		  HELPCTX(terminal_bce),
		  conf_checkbox_handler, I(CONF_bce));
    ctrl_checkbox(s, "글자 깜빡임 사용", 'n',
		  HELPCTX(terminal_blink),
		  conf_checkbox_handler, I(CONF_blinktext));
    ctrl_editbox(s, "^E 문자 수신에 대한 응답 문자열:", 's', 100,
		 HELPCTX(terminal_answerback),
		 conf_editbox_handler, I(CONF_answerback), I(1));

    s = ctrl_getset(b, "터미널", "ldisc", "행 구분 옵션");
    ctrl_radiobuttons(s, "자국 반향:", 'l', 3,
		      HELPCTX(terminal_localecho),
		      conf_radiobutton_handler,I(CONF_localecho),
		      "자동", I(AUTO),
		      "항상 켬", I(FORCE_ON),
		      "항상 끔", I(FORCE_OFF), NULL);
    ctrl_radiobuttons(s, "자체 줄 편집:", 't', 3,
		      HELPCTX(terminal_localedit),
		      conf_radiobutton_handler,I(CONF_localedit),
		      "자동", I(AUTO),
		      "항상 켬", I(FORCE_ON),
		      "항상 끔", I(FORCE_OFF), NULL);

    s = ctrl_getset(b, "터미널", "printing", "원격 제어 프린터");
    ctrl_combobox(s, "ANSI 프린터 출력을 보낼 프린터:", 'p', 100,
		  HELPCTX(terminal_printing),
		  printerbox_handler, P(NULL), P(NULL));

    /*
     * The Terminal/Keyboard panel.
     */
    ctrl_settitle(b, "터미널/키보드",
		  "키 효과 제어 옵션");

    s = ctrl_getset(b, "터미널/키보드", "mappings",
		    "시퀀스 변경 제어:");
    ctrl_radiobuttons(s, "백스페이스 키", 'b', 2,
		      HELPCTX(keyboard_backspace),
		      conf_radiobutton_handler,
		      I(CONF_bksp_is_delete),
		      "Control-H", I(0), "Control-? (127)", I(1), NULL);
    ctrl_radiobuttons(s, "Home 과 End 키", 'e', 2,
		      HELPCTX(keyboard_homeend),
		      conf_radiobutton_handler,
		      I(CONF_rxvt_homeend),
		      "표준", I(0), "rxvt", I(1), NULL);
    ctrl_radiobuttons(s, "펑션키와 키패드", 'f', 3,
		      HELPCTX(keyboard_funkeys),
		      conf_radiobutton_handler,
		      I(CONF_funky_type),
		      "ESC[n~", I(0), "Linux", I(1), "Xterm R6", I(2),
		      "VT400", I(3), "VT100+", I(4), "SCO", I(5), NULL);

    s = ctrl_getset(b, "터미널/키보드", "appkeypad",
		    "응용 프로그램 키패드 설정:");
    ctrl_radiobuttons(s, "커서 키 기본 상태:", 'r', 3,
		      HELPCTX(keyboard_appcursor),
		      conf_radiobutton_handler,
		      I(CONF_app_cursor),
		      "일반", I(0), "프로그램", I(1), NULL);
    ctrl_radiobuttons(s, "숫자 키패드 기본 상태:", 'n', 3,
		      HELPCTX(keyboard_appkeypad),
		      numeric_keypad_handler, P(NULL),
		      "일반", I(0), "프로그램", I(1), "넷핵", I(2),
		      NULL);

    /*
     * The Terminal/Bell panel.
     */
    ctrl_settitle(b, "터미널/벨",
		  "터미널 벨 제어 옵션");

    s = ctrl_getset(b, "터미널/벨", "style", "벨 스타일 설정");
    ctrl_radiobuttons(s, "벨 울림 효과:", 'b', 1,
		      HELPCTX(bell_style),
		      conf_radiobutton_handler, I(CONF_beep),
		      "없음 (벨 사용 안 함)", I(BELL_DISABLED),
		      "기본 시스템 벨소리", I(BELL_DEFAULT),
		      "시각 벨(창 번쩍임)", I(BELL_VISUAL), NULL);

    s = ctrl_getset(b, "터미널/벨", "overload",
		    "일시적인 벨소리 과부하 제어");
    ctrl_checkbox(s, "벨소리가 시끄러울 때 임시로 무시함", 'd',
		  HELPCTX(bell_overload),
		  conf_checkbox_handler, I(CONF_bellovl));
    ctrl_editbox(s, "최소 벨 과부하 결정 기준", 'm', 20,
		 HELPCTX(bell_overload),
		 conf_editbox_handler, I(CONF_bellovl_n), I(-1));
    ctrl_editbox(s, "멈춤 시간(초)", 't', 20,
		 HELPCTX(bell_overload),
		 conf_editbox_handler, I(CONF_bellovl_t),
		 I(-TICKSPERSEC));
    ctrl_text(s, "일정 시간 조용하면 벨소리 다시 복구",
	      HELPCTX(bell_overload));
    ctrl_editbox(s, "해제 시간(초)", 's', 20,
		 HELPCTX(bell_overload),
		 conf_editbox_handler, I(CONF_bellovl_s),
		 I(-TICKSPERSEC));

    /*
     * The Terminal/Features panel.
     */
    ctrl_settitle(b, "터미널/기능",
		  "부가적인 터미널 기능 활성화 및 비활성화");

    s = ctrl_getset(b, "터미널/기능", "main", NULL);
    ctrl_checkbox(s, "어플리케이션 방향 키 모드 사용 안 함", 'u',
		  HELPCTX(features_application),
		  conf_checkbox_handler, I(CONF_no_applic_c));
    ctrl_checkbox(s, "어플리케이션 키패드 모드 사용 안 함", 'k',
		  HELPCTX(features_application),
		  conf_checkbox_handler, I(CONF_no_applic_k));
    ctrl_checkbox(s, "xterm 스타일 마우스 사용 안 함", 'x',
		  HELPCTX(features_mouse),
		  conf_checkbox_handler, I(CONF_no_mouse_rep));
    ctrl_checkbox(s, "원격 창 크기 조절 사용 안 함", 's',
		  HELPCTX(features_resize),
		  conf_checkbox_handler,
		  I(CONF_no_remote_resize));
    ctrl_checkbox(s, "대체 터미널 스크린 변경 기능 사용 안 함", 'w',
		  HELPCTX(features_altscreen),
		  conf_checkbox_handler, I(CONF_no_alt_screen));
    ctrl_checkbox(s, "원격 창 제목 변경 사용 안 함", 't',
		  HELPCTX(features_retitle),
		  conf_checkbox_handler,
		  I(CONF_no_remote_wintitle));
    ctrl_checkbox(s, "원격 창 스크롤 초기화 사용 안 함", 'e',
		  HELPCTX(features_clearscroll),
		  conf_checkbox_handler,
		  I(CONF_no_remote_clearscroll));
    ctrl_radiobuttons(s, "원격 창 제목 질의에 대한 응답(보안):", 'q', 3,
		      HELPCTX(features_qtitle),
		      conf_radiobutton_handler,
		      I(CONF_remote_qtitle_action),
		      "무응답", I(TITLE_NONE),
		      "빈 문자열", I(TITLE_EMPTY),
		      "윈도우 제목", I(TITLE_REAL), NULL);
    ctrl_checkbox(s, "서버가 보내는 ^? 삭제를 사용 안 함",'b',
		  HELPCTX(features_dbackspace),
		  conf_checkbox_handler, I(CONF_no_dbackspace));
    ctrl_checkbox(s, "원격 문자 셋 설정 사용 안 함",
		  'r', HELPCTX(features_charset), conf_checkbox_handler,
		  I(CONF_no_remote_charset));
    ctrl_checkbox(s, "아랍어 출력을 사용 안 함",
		  'l', HELPCTX(features_arabicshaping), conf_checkbox_handler,
		  I(CONF_arabicshaping));
    ctrl_checkbox(s, "오른쪽에서 왼쪽으로 글쓰기 사용 안 함",
		  'd', HELPCTX(features_bidi), conf_checkbox_handler,
		  I(CONF_bidi));

    /*
     * The Window panel.
     */
    str = dupprintf("%s 창 옵션 제어", appname);
    ctrl_settitle(b, "창", str);
    sfree(str);

    s = ctrl_getset(b, "창", "size", "창 크기 조절");
    ctrl_columns(s, 2, 50, 50);
    c = ctrl_editbox(s, "가로", 'm', 100,
		     HELPCTX(window_size),
		     conf_editbox_handler, I(CONF_width), I(-1));
    c->generic.column = 0;
    c = ctrl_editbox(s, "줄", 'r', 100,
		     HELPCTX(window_size),
		     conf_editbox_handler, I(CONF_height),I(-1));
    c->generic.column = 1;
    ctrl_columns(s, 1, 100);

    s = ctrl_getset(b, "창", "scrollback",
		    "창 이전 내용 올려보기 설정");
    ctrl_editbox(s, "이전 화면 저장 줄 수", 's', 50,
		 HELPCTX(window_scrollback),
		 conf_editbox_handler, I(CONF_savelines), I(-1));
    ctrl_checkbox(s, "스크롤 바 표시", 'd',
		  HELPCTX(window_scrollback),
		  conf_checkbox_handler, I(CONF_scrollbar));
    ctrl_checkbox(s, "키 입력시 스크롤 초기화", 'k',
		  HELPCTX(window_scrollback),
		  conf_checkbox_handler, I(CONF_scroll_on_key));
    ctrl_checkbox(s, "출력이 있으면 스크롤 초기화", 'p',
		  HELPCTX(window_scrollback),
		  conf_checkbox_handler, I(CONF_scroll_on_disp));
    ctrl_checkbox(s, "지워진 글을 이전화면으로 밀어냄", 'e',
		  HELPCTX(window_erased),
		  conf_checkbox_handler,
		  I(CONF_erase_to_scrollback));

    /*
     * The Window/Appearance panel.
     */
    str = dupprintf("%s 창 모양 설정", appname);
    ctrl_settitle(b, "창/모양", str);
    sfree(str);

    s = ctrl_getset(b, "창/모양", "cursor",
		    "커서 동작 조절");
    ctrl_radiobuttons(s, "커서 모양:", NO_SHORTCUT, 3,
		      HELPCTX(appearance_cursor),
		      conf_radiobutton_handler,
		      I(CONF_cursor_type),
		      "사각형", 'l', I(0),
		      "밑줄", 'u', I(1),
		      "세로줄", 'v', I(2), NULL);
    ctrl_checkbox(s, "커서 깜박임", 'b',
		  HELPCTX(appearance_cursor),
		  conf_checkbox_handler, I(CONF_blink_cur));

    s = ctrl_getset(b, "창/모양", "font",
		    "글꼴 설정");
    ctrl_fontsel(s, "터미널 글꼴 선택", 'n',
		 HELPCTX(appearance_font),
		 conf_fontsel_handler, I(CONF_font));

    s = ctrl_getset(b, "창/모양", "mouse",
		    "마우스 포인터 설정");
    ctrl_checkbox(s, "입력할 때 마우스 포인터 숨김", 'p',
		  HELPCTX(appearance_hidemouse),
		  conf_checkbox_handler, I(CONF_hide_mouseptr));

    s = ctrl_getset(b, "창/모양", "border",
		    "창 틀설정");
    ctrl_editbox(s, "본문과 창 틀 사이의 간격:", 'e', 20,
		 HELPCTX(appearance_border),
		 conf_editbox_handler,
		 I(CONF_window_border), I(-1));

    /*
     * The Window/Behaviour panel.
     */
    str = dupprintf("%s 창 특성 설정", appname);
    ctrl_settitle(b, "창/특성", str);
    sfree(str);

    s = ctrl_getset(b, "창/특성", "title",
		    "창 제목 설정");
    ctrl_editbox(s, "창 제목:", 't', 100,
		 HELPCTX(appearance_title),
		 conf_editbox_handler, I(CONF_wintitle), I(1));
    ctrl_checkbox(s, "창과 아이콘 제목을 분리", 'i',
		  HELPCTX(appearance_title),
		  conf_checkbox_handler,
		  I(CHECKBOX_INVERT | CONF_win_name_always));

    s = ctrl_getset(b, "창/특성", "main", NULL);
    ctrl_checkbox(s, "창 닫기 전에 경고", 'w',
		  HELPCTX(behaviour_closewarn),
		  conf_checkbox_handler, I(CONF_warn_on_close));

    /*
     * The Window/Translation panel.
     */
    ctrl_settitle(b, "창/변환",
		  "문자셋 변환 옵션");

    s = ctrl_getset(b, "창/변환", "trans",
		    "문자셋 변환");
    ctrl_combobox(s, "수신한 데이터의 문자셋:",
		  'r', 100, HELPCTX(translation_codepage),
		  codepage_handler, P(NULL), P(NULL));

    s = ctrl_getset(b, "창/변환", "tweaks", NULL);
    ctrl_checkbox(s, "너비가 지정되지 않은 한중일 글자를 넓게 표시", 'w',
		  HELPCTX(translation_cjk_ambig_wide),
		  conf_checkbox_handler, I(CONF_cjk_ambig_wide));

    str = dupprintf("%s 선 그리기 처리 방법", appname);
    s = ctrl_getset(b, "창/변환", "linedraw", str);
    sfree(str);
    ctrl_radiobuttons(s, "선 그리기 처리 방법:", NO_SHORTCUT,1,
		      HELPCTX(translation_linedraw),
		      conf_radiobutton_handler,
		      I(CONF_vtmode),
		      "유니코드 선문자 사용",'u',I(VT_UNICODE),
		      "Poor man 선문자 (+, -, |)",'p',I(VT_POORMAN),
		      NULL);
    ctrl_checkbox(s, "VT100 선그림 문자를 lqqqk로 뿌림",'d',
		  HELPCTX(selection_linedraw),
		  conf_checkbox_handler, I(CONF_rawcnp));

    /*
     * The Window/Selection panel.
     */
    ctrl_settitle(b, "창/선택", "복사, 붙여넣기 옵션");
	
    s = ctrl_getset(b, "창/선택", "mouse",
		    "마우스 설정");
    ctrl_checkbox(s, "Shift로 응용 프로그램의 마우스 사용을 무시", 'p',
		  HELPCTX(selection_shiftdrag),
		  conf_checkbox_handler, I(CONF_mouse_override));
    ctrl_radiobuttons(s,
		      "기본 영역 선택 방법 (Alt+drag로 반대 방법 사용):",
		      NO_SHORTCUT, 2,
		      HELPCTX(selection_rect),
		      conf_radiobutton_handler,
		      I(CONF_rect_select),
		      "일반", 'n', I(0),
		      "사각 블럭", 'r', I(1), NULL);

    s = ctrl_getset(b, "창/선택", "charclass",
		    "클릭해서 한 단어만 선택하기 설정");
    ccd = (struct charclass_data *)
	ctrl_alloc(b, sizeof(struct charclass_data));
    ccd->listbox = ctrl_listbox(s, "문자 종류:", 'e',
				HELPCTX(selection_charclasses),
				charclass_handler, P(ccd));
    ccd->listbox->listbox.multisel = 1;
    ccd->listbox->listbox.ncols = 4;
    ccd->listbox->listbox.percentages = snewn(4, int);
    ccd->listbox->listbox.percentages[0] = 15;
    ccd->listbox->listbox.percentages[1] = 25;
    ccd->listbox->listbox.percentages[2] = 20;
    ccd->listbox->listbox.percentages[3] = 40;
    ctrl_columns(s, 2, 67, 33);
    ccd->editbox = ctrl_editbox(s, "클래스", 't', 50,
				HELPCTX(selection_charclasses),
				charclass_handler, P(ccd), P(NULL));
    ccd->editbox->generic.column = 0;
    ccd->button = ctrl_pushbutton(s, "변경", 's',
				  HELPCTX(selection_charclasses),
				  charclass_handler, P(ccd));
    ccd->button->generic.column = 1;
    ctrl_columns(s, 1, 100);

    /*
     * The Window/Colours panel.
     */
    ctrl_settitle(b, "창/색상", "색상 관련 옵션");

    s = ctrl_getset(b, "창/색상", "general",
		    "색상 관련 일반 옵션");
    ctrl_checkbox(s, "터미널에서 ANSI 색상을 조절 할 수 있도록 허용", 'i',
		  HELPCTX(colours_ansi),
		  conf_checkbox_handler, I(CONF_ansi_colour));
    ctrl_checkbox(s, "터미널에서 xterm 256색 모드 사용", '2',
		  HELPCTX(colours_xterm256), conf_checkbox_handler,
		  I(CONF_xterm_256_colour));
    ctrl_radiobuttons(s, "굵은 글씨를 다른방법으로 대체:", 'b', 3,
                      HELPCTX(colours_bold),
                      conf_radiobutton_handler, I(CONF_bold_style),
                      "굵은 폰트 사용", I(1),
                      "색상을 다르게", I(2),
                      "둘다", I(3),
                      NULL);

    str = dupprintf("%s 색상 미세 조절", appname);
    s = ctrl_getset(b, "창/색상", "adjust", str);
    sfree(str);
    ctrl_text(s, "목록에서 색상을 선택 후 변경 버튼을 눌러 적용하세요",
	      HELPCTX(colours_config));
    ctrl_columns(s, 2, 67, 33);
    cd = (struct colour_data *)ctrl_alloc(b, sizeof(struct colour_data));
    cd->listbox = ctrl_listbox(s, "변경할 색상 선택:", 'u',
			       HELPCTX(colours_config), colour_handler, P(cd));
    cd->listbox->generic.column = 0;
    cd->listbox->listbox.height = 7;
    c = ctrl_text(s, "RGB 값:", HELPCTX(colours_config));
    c->generic.column = 1;
    cd->redit = ctrl_editbox(s, "Red", 'r', 50, HELPCTX(colours_config),
			     colour_handler, P(cd), P(NULL));
    cd->redit->generic.column = 1;
    cd->gedit = ctrl_editbox(s, "Green", 'n', 50, HELPCTX(colours_config),
			     colour_handler, P(cd), P(NULL));
    cd->gedit->generic.column = 1;
    cd->bedit = ctrl_editbox(s, "Blue", 'e', 50, HELPCTX(colours_config),
			     colour_handler, P(cd), P(NULL));
    cd->bedit->generic.column = 1;
    cd->button = ctrl_pushbutton(s, "변경", 'm', HELPCTX(colours_config),
				 colour_handler, P(cd));
    cd->button->generic.column = 1;
    ctrl_columns(s, 1, 100);

    /*
     * The Connection panel. This doesn't show up if we're in a
     * non-network utility such as pterm. We tell this by being
     * passed a protocol < 0.
     */
    if (protocol >= 0) {
	ctrl_settitle(b, "연결", "연결 제어 옵션");

	s = ctrl_getset(b, "연결", "keepalive",
			"세션 유지를 위해 빈 패킷 보내기");
	ctrl_editbox(s, "접속 유지 간격 (초, 0 은 사용 안함)", 'k', 20,
		     HELPCTX(connection_keepalive),
		     conf_editbox_handler, I(CONF_ping_interval),
		     I(-1));

	if (!midsession) {
	    s = ctrl_getset(b, "연결", "tcp",
			    "저수준 TCP 옵션");
	    ctrl_checkbox(s, "Nagle 알고리즘 사용 안 함 (TCP_NODELAY 옵션)",
			  'n', HELPCTX(connection_nodelay),
			  conf_checkbox_handler,
			  I(CONF_tcp_nodelay));
	    ctrl_checkbox(s, "TCP keepalive 사용 (SO_KEEPALIVE 옵션)",
			  'p', HELPCTX(connection_tcpkeepalive),
			  conf_checkbox_handler,
			  I(CONF_tcp_keepalives));
#ifndef NO_IPV6
	    s = ctrl_getset(b, "연결", "ipversion",
			  "인터넷 프로토콜 버전");
	    ctrl_radiobuttons(s, NULL, NO_SHORTCUT, 3,
			  HELPCTX(connection_ipversion),
			  conf_radiobutton_handler,
			  I(CONF_addressfamily),
			  "자동", 'u', I(ADDRTYPE_UNSPEC),
			  "IPv4", '4', I(ADDRTYPE_IPV4),
			  "IPv6", '6', I(ADDRTYPE_IPV6),
			  NULL);
#endif

	    {
		const char *label = backend_from_proto(PROT_SSH) ?
		    "논리적인 원격 호스트 이름 (SSH key를 찾을때 사용):" :
		    "논리적인 원격 호스트 이름:";
		s = ctrl_getset(b, "연결", "identity",
				"논리적인 원격 호스트 이름");
		ctrl_editbox(s, label, 'm', 100,
			     HELPCTX(connection_loghost),
			     conf_editbox_handler, I(CONF_loghost), I(1));
	    }
	}

	/*
	 * A sub-panel Connection/Data, containing options that
	 * decide on data to send to the server.
	 */
	if (!midsession) {
	    ctrl_settitle(b, "연결/데이터", "서버로 보낼 데이터");

	    s = ctrl_getset(b, "연결/데이터", "login",
			    "로그인 상세 정보");
	    ctrl_editbox(s, "자동 로그인 사용자", 'u', 50,
			 HELPCTX(connection_username),
			 conf_editbox_handler, I(CONF_username), I(1));
	    {
		/* We assume the local username is sufficiently stable
		 * to include on the dialog box. */
		char *user = get_username();
		char *userlabel = dupprintf("시스템 사용자 이름 (%s)",
					    user ? user : "");
		sfree(user);
		ctrl_radiobuttons(s, "사용자 이름이 지정되지 않았을때:", 'n', 4,
				  HELPCTX(connection_username_from_env),
				  conf_radiobutton_handler,
				  I(CONF_username_from_env),
				  "물어보기", I(FALSE),
				  userlabel, I(TRUE),
				  NULL);
		sfree(userlabel);
	    }

	    s = ctrl_getset(b, "연결/데이터", "term",
			    "터미널 세부 사항");
	    ctrl_editbox(s, "터미널 형식 문자열", 't', 50,
			 HELPCTX(connection_termtype),
			 conf_editbox_handler, I(CONF_termtype), I(1));
	    ctrl_editbox(s, "터미널 속도", 's', 50,
			 HELPCTX(connection_termspeed),
			 conf_editbox_handler, I(CONF_termspeed), I(1));

	    s = ctrl_getset(b, "연결/데이터", "env",
			    "환경 변수");
	    ctrl_columns(s, 2, 80, 20);
	    ed = (struct environ_data *)
		ctrl_alloc(b, sizeof(struct environ_data));
	    ed->varbox = ctrl_editbox(s, "변수", 'v', 60,
				      HELPCTX(telnet_environ),
				      environ_handler, P(ed), P(NULL));
	    ed->varbox->generic.column = 0;
	    ed->valbox = ctrl_editbox(s, "값", 'l', 60,
				      HELPCTX(telnet_environ),
				      environ_handler, P(ed), P(NULL));
	    ed->valbox->generic.column = 0;
	    ed->addbutton = ctrl_pushbutton(s, "추가", 'd',
					    HELPCTX(telnet_environ),
					    environ_handler, P(ed));
	    ed->addbutton->generic.column = 1;
	    ed->rembutton = ctrl_pushbutton(s, "삭제", 'r',
					    HELPCTX(telnet_environ),
					    environ_handler, P(ed));
	    ed->rembutton->generic.column = 1;
	    ctrl_columns(s, 1, 100);
	    ed->listbox = ctrl_listbox(s, NULL, NO_SHORTCUT,
				       HELPCTX(telnet_environ),
				       environ_handler, P(ed));
	    ed->listbox->listbox.height = 3;
	    ed->listbox->listbox.ncols = 2;
	    ed->listbox->listbox.percentages = snewn(2, int);
	    ed->listbox->listbox.percentages[0] = 30;
	    ed->listbox->listbox.percentages[1] = 70;
	}

    }

    if (!midsession) {
	/*
	 * The Connection/Proxy panel.
	 */
	ctrl_settitle(b, "연결/프록시",
		      "프록시 설정");

	s = ctrl_getset(b, "연결/프록시", "basics", NULL);
	ctrl_radiobuttons(s, "프록시 종류:", 't', 3,
			  HELPCTX(proxy_type),
			  conf_radiobutton_handler,
			  I(CONF_proxy_type),
			  "None", I(PROXY_NONE),
			  "SOCKS 4", I(PROXY_SOCKS4),
			  "SOCKS 5", I(PROXY_SOCKS5),
			  "HTTP", I(PROXY_HTTP),
			  "Telnet", I(PROXY_TELNET),
			  NULL);
	ctrl_columns(s, 2, 80, 20);
	c = ctrl_editbox(s, "프록시 호스트", 'y', 100,
			 HELPCTX(proxy_main),
			 conf_editbox_handler,
			 I(CONF_proxy_host), I(1));
	c->generic.column = 0;
	c = ctrl_editbox(s, "포트", 'p', 100,
			 HELPCTX(proxy_main),
			 conf_editbox_handler,
			 I(CONF_proxy_port),
			 I(-1));
	c->generic.column = 1;
	ctrl_columns(s, 1, 100);
	ctrl_editbox(s, "프록시 사용하지 않을 호스트/IP 목록", 'e', 100,
		     HELPCTX(proxy_exclude),
		     conf_editbox_handler,
		     I(CONF_proxy_exclude_list), I(1));
	ctrl_checkbox(s, "로컬호스트 접속을 프락시에서 고려", 'x',
		      HELPCTX(proxy_exclude),
		      conf_checkbox_handler,
		      I(CONF_even_proxy_localhost));
	ctrl_radiobuttons(s, "프록시 서버에서 DNS 조회:", 'd', 3,
			  HELPCTX(proxy_dns),
			  conf_radiobutton_handler,
			  I(CONF_proxy_dns),
			  "아니오", I(FORCE_OFF),
			  "자동", I(AUTO),
			  "예", I(FORCE_ON), NULL);
	ctrl_editbox(s, "사용자 이름", 'u', 60,
		     HELPCTX(proxy_auth),
		     conf_editbox_handler,
		     I(CONF_proxy_username), I(1));
	c = ctrl_editbox(s, "암호", 'w', 60,
			 HELPCTX(proxy_auth),
			 conf_editbox_handler,
			 I(CONF_proxy_password), I(1));
	c->editbox.password = 1;
	ctrl_editbox(s, "Telnet 명령", 'm', 100,
		     HELPCTX(proxy_command),
		     conf_editbox_handler,
		     I(CONF_proxy_telnet_command), I(1));

	ctrl_radiobuttons(s, "터미널 창에 프록시 "
                          "진단 상태 출력", 'r', 5,
			  HELPCTX(proxy_logging),
			  conf_radiobutton_handler,
			  I(CONF_proxy_log_to_term),
			  "아니오", I(FORCE_OFF),
			  "예", I(FORCE_ON),
			  "세션 시작 전에만", I(AUTO), NULL);
    }

    /*
     * The Telnet panel exists in the base config box, and in a
     * mid-session reconfig box _if_ we're using Telnet.
     */
    if (!midsession || protocol == PROT_TELNET) {
	/*
	 * The Connection/Telnet panel.
	 */
	ctrl_settitle(b, "연결/Telnet",
		      "Telnet 연결 설정");

	s = ctrl_getset(b, "연결/Telnet", "protocol",
			"Telnet 프로토콜 옵션");

	if (!midsession) {
	    ctrl_radiobuttons(s, "OLD_ENVIRON 처리 방법:",
			      NO_SHORTCUT, 2,
			      HELPCTX(telnet_oldenviron),
			      conf_radiobutton_handler,
			      I(CONF_rfc_environ),
			      "BSD (통상)", 'b', I(0),
			      "RFC 1408 (안 쓰임)", 'f', I(1), NULL);
	    ctrl_radiobuttons(s, "Telnet 협상 방법:", 't', 2,
			      HELPCTX(telnet_passive),
			      conf_radiobutton_handler,
			      I(CONF_passive_telnet),
			      "수동", I(1), "능동", I(0), NULL);
	}
	ctrl_checkbox(s, "키보드로 텔넷 특수 명령 보냄", 'k',
		      HELPCTX(telnet_specialkeys),
		      conf_checkbox_handler,
		      I(CONF_telnet_keyboard));
	ctrl_checkbox(s, "개행 시 CR대신 텔넷 개행 문자(^M) 전송",
		      'm', HELPCTX(telnet_newline),
		      conf_checkbox_handler,
		      I(CONF_telnet_newline));
    }

    if (!midsession) {

	/*
	 * The Connection/Rlogin panel.
	 */
	ctrl_settitle(b, "연결/Rlogin",
		      "Rlogin 연결 설정");

	s = ctrl_getset(b, "연결/Rlogin", "data",
			"서버로 전송할 데이터");
	ctrl_editbox(s, "사용자 이름:", 'l', 50,
		     HELPCTX(rlogin_localuser),
		     conf_editbox_handler, I(CONF_localusername), I(1));

    }

    /*
     * All the SSH stuff is omitted in PuTTYtel, or in a reconfig
     * when we're not doing SSH.
     */

    if (backend_from_proto(PROT_SSH) && (!midsession || protocol == PROT_SSH)) {

	/*
	 * The Connection/SSH panel.
	 */
	ctrl_settitle(b, "연결/SSH",
		      "SSH 연결 설정");

	/* SSH-1 or connection-sharing downstream */
	if (midsession && (protcfginfo == 1 || protcfginfo == -1)) {
	    s = ctrl_getset(b, "연결/SSH", "disclaimer", NULL);
	    ctrl_text(s, "Nothing on this panel may be reconfigured in mid-"
		      "session; it is only here so that sub-panels of it can "
		      "exist without looking strange.", HELPCTX(no_help));
	}

	if (!midsession) {

	    s = ctrl_getset(b, "연결/SSH", "data",
			    "서버로 전송할 데이터");
	    ctrl_editbox(s, "원격 명령:", 'r', 100,
			 HELPCTX(ssh_command),
			 conf_editbox_handler, I(CONF_remote_cmd), I(1));

	    s = ctrl_getset(b, "연결/SSH", "protocol", "프로토콜 옵션");
	    ctrl_checkbox(s, "셀 또는 명령을 수행하지 않음", 'n',
			  HELPCTX(ssh_noshell),
			  conf_checkbox_handler,
			  I(CONF_ssh_no_shell));
	}

	if (!midsession || !(protcfginfo == 1 || protcfginfo == -1)) {
	    s = ctrl_getset(b, "연결/SSH", "protocol", "Protocol options");

	    ctrl_checkbox(s, "압축 허용", 'e',
			  HELPCTX(ssh_compress),
			  conf_checkbox_handler,
			  I(CONF_compression));
	}

	if (!midsession) {
	    s = ctrl_getset(b, "연결/SSH", "sharing", "PuTTY 도구들간 SSH 연결 공유");

	    ctrl_checkbox(s, "가능하면 SSH 연결 공유", 's',
			  HELPCTX(ssh_share),
			  conf_checkbox_handler,
			  I(CONF_ssh_connection_sharing));

            ctrl_text(s, "공유된 연결에서 허가된 역할:",
                      HELPCTX(ssh_share));
	    ctrl_checkbox(s, "Upstream (리얼 서버로의 연결)", 'u',
			  HELPCTX(ssh_share),
			  conf_checkbox_handler,
			  I(CONF_ssh_connection_sharing_upstream));
	    ctrl_checkbox(s, "Downstream (upstream 되어진 PuTTY로 연결)", 'd',
			  HELPCTX(ssh_share),
			  conf_checkbox_handler,
			  I(CONF_ssh_connection_sharing_downstream));
	}

	if (!midsession) {
	    s = ctrl_getset(b, "연결/SSH", "protocol", "프로토콜 옵션");

	    ctrl_radiobuttons(s, "SSH 프로토콜 버전:", NO_SHORTCUT, 2,
			      HELPCTX(ssh_protocol),
			      conf_radiobutton_handler,
			      I(CONF_sshprot),
			      "2", '2', I(3),
			      "1 (안전하지 않음)", '1', I(0), NULL);
	}

	/*
	 * The Connection/SSH/Kex panel. (Owing to repeat key
	 * exchange, much of this is meaningful in mid-session _if_
	 * we're using SSH-2 and are not a connection-sharing
	 * downstream, or haven't decided yet.)
	 */
	if (protcfginfo != 1 && protcfginfo != -1) {
	    ctrl_settitle(b, "연결/SSH/키교환",
			  "SSH 키 교환 설정");

	    s = ctrl_getset(b, "연결/SSH/키교환", "main",
			    "키 교환 알고리즘 옵션");
	    c = ctrl_draglist(s, "알고리즘 선택 정책:", 's',
			      HELPCTX(ssh_kexlist),
			      kexlist_handler, P(NULL));
	    c->listbox.height = 5;

	    s = ctrl_getset(b, "연결/SSH/키교환", "repeat",
			    "키 재교환 설정");

	    ctrl_editbox(s, "키 재교환 최대 시간 주기(분) (0-무제한)", 't', 20,
			 HELPCTX(ssh_kex_repeat),
			 conf_editbox_handler,
			 I(CONF_ssh_rekey_time),
			 I(-1));
	    ctrl_editbox(s, "키 재교환 최대 데이터 주기(0-무제한)", 'x', 20,
			 HELPCTX(ssh_kex_repeat),
			 conf_editbox_handler,
			 I(CONF_ssh_rekey_data),
			 I(16));
	    ctrl_text(s, "(1 메가바이트는 1M, 1 기가바이트는 1G로 사용)",
		      HELPCTX(ssh_kex_repeat));
	}

	/*
	 * The 'Connection/SSH/Host keys' panel.
	 */
	if (protcfginfo != 1 && protcfginfo != -1) {
	    ctrl_settitle(b, "연결/SSH/호스트키",
			  "SSH 호스트키 설정");

	    s = ctrl_getset(b, "연결/SSH/호스트키", "main",
			    "호스트키 알고리즘 설정");
	    c = ctrl_draglist(s, "알고리즘 선택 정책:", 's',
			      HELPCTX(ssh_hklist),
			      hklist_handler, P(NULL));
	    c->listbox.height = 5;
	}

	/*
	 * Manual host key configuration is irrelevant mid-session,
	 * as we enforce that the host key for rekeys is the
	 * same as that used at the start of the session.
	 */
	if (!midsession) {
	    s = ctrl_getset(b, "연결/SSH/호스트키", "hostkeys",
			    "이 연결에 대한 호스트키 수동 설정");

            ctrl_columns(s, 2, 75, 25);
            c = ctrl_text(s, "허가된 호스트키 또는 핑거프린트:",
                          HELPCTX(ssh_kex_manual_hostkeys));
            c->generic.column = 0;
            /* You want to select from the list, _then_ hit Remove. So
             * tab order should be that way round. */
            mh = (struct manual_hostkey_data *)
                ctrl_alloc(b,sizeof(struct manual_hostkey_data));
            mh->rembutton = ctrl_pushbutton(s, "삭제", 'r',
                                            HELPCTX(ssh_kex_manual_hostkeys),
                                            manual_hostkey_handler, P(mh));
            mh->rembutton->generic.column = 1;
            mh->rembutton->generic.tabdelay = 1;
            mh->listbox = ctrl_listbox(s, NULL, NO_SHORTCUT,
                                       HELPCTX(ssh_kex_manual_hostkeys),
                                       manual_hostkey_handler, P(mh));
            /* This list box can't be very tall, because there's not
             * much room in the pane on Windows at least. This makes
             * it become really unhelpful if a horizontal scrollbar
             * appears, so we suppress that. */
            mh->listbox->listbox.height = 2;
            mh->listbox->listbox.hscroll = FALSE;
            ctrl_tabdelay(s, mh->rembutton);
	    mh->keybox = ctrl_editbox(s, "키", 'k', 80,
                                      HELPCTX(ssh_kex_manual_hostkeys),
                                      manual_hostkey_handler, P(mh), P(NULL));
            mh->keybox->generic.column = 0;
            mh->addbutton = ctrl_pushbutton(s, "키 추가", 'y',
                                            HELPCTX(ssh_kex_manual_hostkeys),
                                            manual_hostkey_handler, P(mh));
            mh->addbutton->generic.column = 1;
            ctrl_columns(s, 1, 100);

	    s = ctrl_getset(b, "연결/SSH/호스트키", "hostkeychk",
			    "서버 호스트키 검사");

	    ctrl_checkbox(s, "로그인 시, 서버의 호스트키 검사하지 않음",
			  'c', HELPCTX(ssh_hostkey_check),
			  conf_checkbox_handler,
			  I(CONF_ssh_hostkey_check));
	}

	if (!midsession || !(protcfginfo == 1 || protcfginfo == -1)) {
	    /*
	     * The Connection/SSH/Cipher panel.
	     */
	    ctrl_settitle(b, "연결/SSH/Cipher",
			  "SSH 암호화 설정");

	    s = ctrl_getset(b, "연결/SSH/Cipher",
                            "encryption", "암호화 설정");
	    c = ctrl_draglist(s, "암호화 cipher 선택 정책:", 's',
			      HELPCTX(ssh_ciphers),
			      cipherlist_handler, P(NULL));
	    c->listbox.height = 6;

	    ctrl_checkbox(s, "SSH-2에서 단일-DES 하휘 호환성 사용", 'i',
			  HELPCTX(ssh_ciphers),
			  conf_checkbox_handler,
			  I(CONF_ssh2_des_cbc));
	}

	if (!midsession) {

	    /*
	     * The Connection/SSH/Auth panel.
	     */
	    ctrl_settitle(b, "연결/SSH/Auth",
			  "SSH 인증 설정");

	    s = ctrl_getset(b, "연결/SSH/Auth", "main", NULL);
	    ctrl_checkbox(s, "인증 전의 배너 표시 (SSH-2만)",
			  'd', HELPCTX(ssh_auth_banner),
			  conf_checkbox_handler,
			  I(CONF_ssh_show_banner));
	    ctrl_checkbox(s, "인증을 모두 건너 뜀 (SSH-2)", 'b',
			  HELPCTX(ssh_auth_bypass),
			  conf_checkbox_handler,
			  I(CONF_ssh_no_userauth));

	    s = ctrl_getset(b, "연결/SSH/Auth", "methods",
			    "인증 방법");
	    ctrl_checkbox(s, "Pageant를 이용하여 인증 시도", 'p',
			  HELPCTX(ssh_auth_pageant),
			  conf_checkbox_handler,
			  I(CONF_tryagent));
	    ctrl_checkbox(s, "SSH1에서 TIS 또는 CryptoCard 인증 시도", 'm',
			  HELPCTX(ssh_auth_tis),
			  conf_checkbox_handler,
			  I(CONF_try_tis_auth));
	    ctrl_checkbox(s, "SSH2에서 \"키보드로 직접 입력\" 인증 시도",
			  'i', HELPCTX(ssh_auth_ki),
			  conf_checkbox_handler,
			  I(CONF_try_ki_auth));

	    s = ctrl_getset(b, "연결/SSH/Auth", "params",
			    "인증 매개 변수");
	    ctrl_checkbox(s, "에이전트 포워딩 허용", 'f',
			  HELPCTX(ssh_auth_agentfwd),
			  conf_checkbox_handler, I(CONF_agentfwd));
	    ctrl_checkbox(s, "SSH2에서 사용자 이름 변경 시도 허용", NO_SHORTCUT,
			  HELPCTX(ssh_auth_changeuser),
			  conf_checkbox_handler,
			  I(CONF_change_username));
	    ctrl_filesel(s, "인증 개인키 파일:", 'k',
			 FILTER_KEY_FILES, FALSE, "개인키 파일 선택",
			 HELPCTX(ssh_auth_privkey),
			 conf_filesel_handler, I(CONF_keyfile));

#ifndef NO_GSSAPI
	    /*
	     * Connection/SSH/Auth/GSSAPI, which sadly won't fit on
	     * the main Auth panel.
	     */
	    ctrl_settitle(b, "연결/SSH/Auth/GSSAPI",
			  "GSSAPI 인증 설정");
	    s = ctrl_getset(b, "연결/SSH/Auth/GSSAPI", "gssapi", NULL);

	    ctrl_checkbox(s, "GSSAPI 인증 시도 (SSH-2만)",
			  't', HELPCTX(ssh_gssapi),
			  conf_checkbox_handler,
			  I(CONF_try_gssapi_auth));

	    ctrl_checkbox(s, "GSSAPI 자격 증명 위임 허용", 'l',
			  HELPCTX(ssh_gssapi_delegation),
			  conf_checkbox_handler,
			  I(CONF_gssapifwd));

	    /*
	     * GSSAPI library selection.
	     */
	    if (ngsslibs > 1) {
		c = ctrl_draglist(s, "GSSAPI 라이브러리 설정 순서:",
				  'p', HELPCTX(ssh_gssapi_libraries),
				  gsslist_handler, P(NULL));
		c->listbox.height = ngsslibs;

		/*
		 * I currently assume that if more than one GSS
		 * library option is available, then one of them is
		 * 'user-supplied' and so we should present the
		 * following file selector. This is at least half-
		 * reasonable, because if we're using statically
		 * linked GSSAPI then there will only be one option
		 * and no way to load from a user-supplied library,
		 * whereas if we're using dynamic libraries then
		 * there will almost certainly be some default
		 * option in addition to a user-supplied path. If
		 * anyone ever ports PuTTY to a system on which
		 * dynamic-library GSSAPI is available but there is
		 * absolutely no consensus on where to keep the
		 * libraries, there'll need to be a flag alongside
		 * ngsslibs to control whether the file selector is
		 * displayed. 
		 */

		ctrl_filesel(s, "사용자 제공 GSSAPI 라이브러리 경로:", 's',
			     FILTER_DYNLIB_FILES, FALSE, "라이브러리 파일 선택",
			     HELPCTX(ssh_gssapi_libraries),
			     conf_filesel_handler,
			     I(CONF_ssh_gss_custom));
	    }
#endif
	}

	if (!midsession) {
	    /*
	     * The Connection/SSH/TTY panel.
	     */
	    ctrl_settitle(b, "연결/SSH/TTY", "원격 터미널 설정");

	    s = ctrl_getset(b, "연결/SSH/TTY", "sshtty", NULL);
	    ctrl_checkbox(s, "가상(pseudo) 터미널 할당 안함", 'p',
			  HELPCTX(ssh_nopty),
			  conf_checkbox_handler,
			  I(CONF_nopty));

	    s = ctrl_getset(b, "연결/SSH/TTY", "ttymodes",
			    "터미널 모드");
	    td = (struct ttymodes_data *)
		ctrl_alloc(b, sizeof(struct ttymodes_data));
	    c = ctrl_text(s, "터미널 전송 모드:", HELPCTX(ssh_ttymodes));
	    td->listbox = ctrl_listbox(s, NULL, NO_SHORTCUT,
				       HELPCTX(ssh_ttymodes),
				       ttymodes_handler, P(td));
	    td->listbox->listbox.height = 8;
	    td->listbox->listbox.ncols = 2;
	    td->listbox->listbox.percentages = snewn(2, int);
	    td->listbox->listbox.percentages[0] = 40;
	    td->listbox->listbox.percentages[1] = 60;
	    ctrl_columns(s, 2, 75, 25);
	    c = ctrl_text(s, "선택한 전송 모드 사용 여부:", HELPCTX(ssh_ttymodes));
	    c->generic.column = 0;
	    td->setbutton = ctrl_pushbutton(s, "설정", 's',
					    HELPCTX(ssh_ttymodes),
					    ttymodes_handler, P(td));
	    td->setbutton->generic.column = 1;
	    td->setbutton->generic.tabdelay = 1;
	    ctrl_columns(s, 1, 100);	    /* column break */
	    /* Bit of a hack to get the value radio buttons and
	     * edit-box on the same row. */
	    ctrl_columns(s, 2, 75, 25);
	    td->valradio = ctrl_radiobuttons(s, NULL, NO_SHORTCUT, 3,
					     HELPCTX(ssh_ttymodes),
					     ttymodes_handler, P(td),
					     "자동", NO_SHORTCUT, P(NULL),
					     "없음", NO_SHORTCUT, P(NULL),
					     "지정값:", NO_SHORTCUT, P(NULL),
					     NULL);
	    td->valradio->generic.column = 0;
	    td->valbox = ctrl_editbox(s, NULL, NO_SHORTCUT, 100,
				      HELPCTX(ssh_ttymodes),
				      ttymodes_handler, P(td), P(NULL));
	    td->valbox->generic.column = 1;
	    ctrl_tabdelay(s, td->setbutton);
	}

	if (!midsession) {
	    /*
	     * The Connection/SSH/X11 panel.
	     */
	    ctrl_settitle(b, "연결/SSH/X11",
			  "SSH X11 포워딩 설정");

	    s = ctrl_getset(b, "연결/SSH/X11", "x11", "X11 포워딩");
	    ctrl_checkbox(s, "X11 포워딩 사용", 'e',
			  HELPCTX(ssh_tunnels_x11),
			  conf_checkbox_handler,I(CONF_x11_forward));
	    ctrl_editbox(s, "X 디스플레이 위치", 'x', 50,
			 HELPCTX(ssh_tunnels_x11),
			 conf_editbox_handler, I(CONF_x11_display), I(1));
	    ctrl_radiobuttons(s, "원격 X11 인증 프로토콜", 'u', 2,
			      HELPCTX(ssh_tunnels_x11auth),
			      conf_radiobutton_handler,
			      I(CONF_x11_auth),
			      "MIT-Magic-Cookie-1", I(X11_MIT),
			      "XDM-Authorization-1", I(X11_XDM), NULL);
	}

	/*
	 * The Tunnels panel _is_ still available in mid-session.
	 */
	ctrl_settitle(b, "연결/SSH/터널",
		      "SSH 포트 포워딩 설정");

	s = ctrl_getset(b, "연결/SSH/터널", "portfwd",
			"포트 포워딩");
	ctrl_checkbox(s, "다른 호스트에서 접속 허용",'t',
		      HELPCTX(ssh_tunnels_portfwd_localhost),
		      conf_checkbox_handler,
		      I(CONF_lport_acceptall));
	ctrl_checkbox(s, "원격 포트도 같은 방법으로 (SSH2만)", 'p',
		      HELPCTX(ssh_tunnels_portfwd_localhost),
		      conf_checkbox_handler,
		      I(CONF_rport_acceptall));

	ctrl_columns(s, 3, 55, 20, 25);
	c = ctrl_text(s, "포워딩된 포트 목록:", HELPCTX(ssh_tunnels_portfwd));
	c->generic.column = COLUMN_FIELD(0,2);
	/* You want to select from the list, _then_ hit Remove. So tab order
	 * should be that way round. */
	pfd = (struct portfwd_data *)ctrl_alloc(b,sizeof(struct portfwd_data));
	pfd->rembutton = ctrl_pushbutton(s, "제거", 'r',
					 HELPCTX(ssh_tunnels_portfwd),
					 portfwd_handler, P(pfd));
	pfd->rembutton->generic.column = 2;
	pfd->rembutton->generic.tabdelay = 1;
	pfd->listbox = ctrl_listbox(s, NULL, NO_SHORTCUT,
				    HELPCTX(ssh_tunnels_portfwd),
				    portfwd_handler, P(pfd));
	pfd->listbox->listbox.height = 3;
	pfd->listbox->listbox.ncols = 2;
	pfd->listbox->listbox.percentages = snewn(2, int);
	pfd->listbox->listbox.percentages[0] = 20;
	pfd->listbox->listbox.percentages[1] = 80;
	ctrl_tabdelay(s, pfd->rembutton);
	ctrl_text(s, "포트 포워딩 포트 추가:", HELPCTX(ssh_tunnels_portfwd));
	/* You want to enter source, destination and type, _then_ hit Add.
	 * Again, we adjust the tab order to reflect this. */
	pfd->addbutton = ctrl_pushbutton(s, "추가", 'd',
					 HELPCTX(ssh_tunnels_portfwd),
					 portfwd_handler, P(pfd));
	pfd->addbutton->generic.column = 2;
	pfd->addbutton->generic.tabdelay = 1;
	pfd->sourcebox = ctrl_editbox(s, "소스 포트", 's', 40,
				      HELPCTX(ssh_tunnels_portfwd),
				      portfwd_handler, P(pfd), P(NULL));
	pfd->sourcebox->generic.column = 0;
	pfd->destbox = ctrl_editbox(s, "목적지", 'i', 67,
				    HELPCTX(ssh_tunnels_portfwd),
				    portfwd_handler, P(pfd), P(NULL));
	pfd->direction = ctrl_radiobuttons(s, NULL, NO_SHORTCUT, 3,
					   HELPCTX(ssh_tunnels_portfwd),
					   portfwd_handler, P(pfd),
					   "로컬", 'l', P(NULL),
					   "원격", 'm', P(NULL),
					   "동적", 'y', P(NULL),
					   NULL);
#ifndef NO_IPV6
	pfd->addressfamily =
	    ctrl_radiobuttons(s, NULL, NO_SHORTCUT, 3,
			      HELPCTX(ssh_tunnels_portfwd_ipversion),
			      portfwd_handler, P(pfd),
			      "자동", 'u', I(ADDRTYPE_UNSPEC),
			      "IPv4", '4', I(ADDRTYPE_IPV4),
			      "IPv6", '6', I(ADDRTYPE_IPV6),
			      NULL);
#endif
	ctrl_tabdelay(s, pfd->addbutton);
	ctrl_columns(s, 1, 100);

	if (!midsession) {
	    /*
	     * The Connection/SSH/Bugs panels.
	     */
	    ctrl_settitle(b, "연결/SSH/Bugs",
			  "SSH 서버의 버그에 대한 해결 방법");

	    s = ctrl_getset(b, "연결/SSH/Bugs", "main",
			    "SSH 서버의 알려진 버그를 탐지");
	    ctrl_droplist(s, "SSH-2 무시 메시지 문제", '2', 20,
			  HELPCTX(ssh_bugs_ignore2),
			  sshbug_handler, I(CONF_sshbug_ignore2));
	    ctrl_droplist(s, "SSH-2 잘못된 키 재교환 허용", 'k', 20,
			  HELPCTX(ssh_bugs_rekey2),
			  sshbug_handler, I(CONF_sshbug_rekey2));
	    ctrl_droplist(s, "SSH-2 PuTTY의 'winadj' 요청 막기", 'j',
                          20, HELPCTX(ssh_bugs_winadj),
			  sshbug_handler, I(CONF_sshbug_winadj));
	    ctrl_droplist(s, "종료된 채널에 들어오는 요청에 대한 응답", 'q', 20,
			  HELPCTX(ssh_bugs_chanreq),
			  sshbug_handler, I(CONF_sshbug_chanreq));
	    ctrl_droplist(s, "SSH-2 최대 패킷 크기 무시", 'x', 20,
			  HELPCTX(ssh_bugs_maxpkt2),
			  sshbug_handler, I(CONF_sshbug_maxpkt2));

	    ctrl_settitle(b, "연결/SSH/More bugs",
			  "SSH 서버 버그에 대한 추가 해결 방법");

	    s = ctrl_getset(b, "연결/SSH/More bugs", "main",
			    "SSH 서버의 알려진 버그를 탐지");
	    ctrl_droplist(s, "SSH RSA 서명에 패딩을 요구", 'p', 20,
			  HELPCTX(ssh_bugs_rsapad2),
			  sshbug_handler, I(CONF_sshbug_rsapad2));
	    ctrl_droplist(s, "SSH-2 pre-RFC4419 DH GEX만 지원", 'd', 20,
			  HELPCTX(ssh_bugs_oldgex2),
			  sshbug_handler, I(CONF_sshbug_oldgex2));
	    ctrl_droplist(s, "SSH-2 HMAC 키를 잘못 계산함", 'm', 20,
			  HELPCTX(ssh_bugs_hmac2),
			  sshbug_handler, I(CONF_sshbug_hmac2));
	    ctrl_droplist(s, "SSH-2 세션 ID를 PK 인증에 오용함", 'n', 20,
			  HELPCTX(ssh_bugs_pksessid2),
			  sshbug_handler, I(CONF_sshbug_pksessid2));
	    ctrl_droplist(s, "SSH-2 암호화 키를 잘못 계산함", 'e', 20,
			  HELPCTX(ssh_bugs_derivekey2),
			  sshbug_handler, I(CONF_sshbug_derivekey2));
	    ctrl_droplist(s, "SSH-1 무시 메시지 문제", 'i', 20,
			  HELPCTX(ssh_bugs_ignore1),
			  sshbug_handler, I(CONF_sshbug_ignore1));
	    ctrl_droplist(s, "SSH-1 암호 위장을 거부함", 's', 20,
			  HELPCTX(ssh_bugs_plainpw1),
			  sshbug_handler, I(CONF_sshbug_plainpw1));
	    ctrl_droplist(s, "SSH-1 RSA 인증 문제", 'r', 20,
			  HELPCTX(ssh_bugs_rsa1),
			  sshbug_handler, I(CONF_sshbug_rsa1));
	}
    }
}

// vim: ts=8 sts=4 sw=4 noet cino=\:2\=2(0u0
