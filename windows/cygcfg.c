/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Cyril Dupont
 *
 * This code is based on medgar123's puttycyg project
 *   - https://code.google.com/p/puttycyg/
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "putty.h"
#include "dialog.h"

static int CygTermFlag = 1 ;

void cygterm_set_flag( int flag ) {
	if( flag >= 1 ) CygTermFlag = 1 ;
	else CygTermFlag = 0 ;
}

int cygterm_get_flag( void ) {
	return CygTermFlag ;
}

extern void config_protocolbuttons_handler(union control *, void *, void *, int);

static int is64Bits() {
	return (NULL != GetProcAddress(GetModuleHandle("kernel32"), "IsWow64Process")) ? 1 : 0;
}

void cygterm_setup_config_box(struct controlbox *b, int midsession)
{
	union control *c;
	int i;

	if( !CygTermFlag ) return ;

	struct controlset *s;
	s = ctrl_getset(b, "세션", "hostport",
			"연결할 대상 지정");
	for (i = 0; i < s->ncontrols; i++) {
		c = s->ctrls[i];
		if (c->generic.type == CTRL_RADIO &&
				c->generic.handler == config_protocolbuttons_handler) {
			c->radio.nbuttons++;
			/* c->radio.ncolumns++; */
			c->radio.buttons =
				sresize(c->radio.buttons, c->radio.nbuttons, char *);
			c->radio.buttons[c->radio.nbuttons-1] = dupstr("Cygterm");
			c->radio.buttondata =
				sresize(c->radio.buttondata, c->radio.nbuttons, intorptr);
			c->radio.buttondata[c->radio.nbuttons-1] = I(PROT_CYGTERM);
			if (c->radio.shortcuts) {
				c->radio.shortcuts =
					sresize(c->radio.shortcuts, c->radio.nbuttons, char);
				c->radio.shortcuts[c->radio.nbuttons-1] = NO_SHORTCUT;
			}
		}
	}
	if (!midsession) {
		ctrl_settitle(b, "Connection/Cygterm",
				"Cygterm 세션 제어 옵션");
		s = ctrl_getset(b, "Connection/Cygterm", "cygterm",
				"Cygwin 경로 설정");
		ctrl_checkbox(s, "Cygwin 설치 자동 감지", 'd',
			HELPCTX(no_help),
			conf_checkbox_handler/*dlg_stdcheckbox_handler*/,
			I(CONF_cygautopath)
		);
		if( is64Bits() )
		{
			ctrl_checkbox(s, "Cygwin64 사용", 'u',
					HELPCTX(no_help),
					conf_checkbox_handler,
					I(CONF_cygterm64));
		}
	}
}
