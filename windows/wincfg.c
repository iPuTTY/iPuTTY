/*
 * wincfg.c - the Windows-specific parts of the PuTTY configuration
 * box.
 */

#include <assert.h>
#include <stdlib.h>

#include "putty.h"
#include "dialog.h"
#include "storage.h"

static void about_handler(union control *ctrl, void *dlg,
			  void *data, int event)
{
    HWND *hwndp = (HWND *)ctrl->generic.context.p;

    if (event == EVENT_ACTION) {
	modal_about_box(*hwndp);
    }
}

static void help_handler(union control *ctrl, void *dlg,
			 void *data, int event)
{
    HWND *hwndp = (HWND *)ctrl->generic.context.p;

    if (event == EVENT_ACTION) {
	show_help(*hwndp);
    }
}

/*
 * HACK: PuttyTray / Session Icon
 */ 
static void window_icon_handler(union control *ctrl, void *dlg, void *data, int event)
{
    Conf *conf = (Conf *) data;

    if (event == EVENT_ACTION) {
	char buf[512], iname[512], *ipointer;
	int iindex;

	memset(&iname, 0, sizeof(iname));
	memset(&buf, 0, sizeof(buf));
	iindex = 0;
	ipointer = iname;
	if (dlg_pick_icon(dlg, &ipointer, sizeof(iname), &iindex) /*&& iname[0]*/) {
	    if (iname[0]) {
		sprintf(buf, "%s,%d", iname, iindex);
	    } else {
		sprintf(buf, "%s", iname);
	    }
	    dlg_icon_set((union control *) ctrl->button.context.p, dlg, buf);
	    conf_set_str(conf, CONF_win_icon, buf);
	};
    };
};

static void variable_pitch_handler(union control *ctrl, void *dlg,
                                   void *data, int event)
{
    if (event == EVENT_REFRESH) {
	dlg_checkbox_set(ctrl, dlg, !dlg_get_fixed_pitch_flag(dlg));
    } else if (event == EVENT_VALCHANGE) {
	dlg_set_fixed_pitch_flag(dlg, !dlg_checkbox_get(ctrl, dlg));
    }
}

void win_setup_config_box(struct controlbox *b, HWND *hwndp, int has_help,
			  int midsession, int protocol)
{
    struct controlset *s;
    union control *c;
    char *str;

    if (!midsession) {
	/*
	 * Add the About and Help buttons to the standard panel.
	 */
	s = ctrl_getset(b, "", "", "");
	c = ctrl_pushbutton(s, "정보", 'a', HELPCTX(no_help),
			    about_handler, P(hwndp));
	c->generic.column = 0;
	if (has_help) {
	    c = ctrl_pushbutton(s, "도움말", 'h', HELPCTX(no_help),
				help_handler, P(hwndp));
	    c->generic.column = 1;
	}
    }

    /*
     * Full-screen mode is a Windows peculiarity; hence
     * scrollbar_in_fullscreen is as well.
     */
    s = ctrl_getset(b, "창", "scrollback",
		    "창 이전 내용 올려보기 설정");
    ctrl_checkbox(s, "전체 화면 모드에서 스크롤 보이기", 'i',
		  HELPCTX(window_scrollback),
		  conf_checkbox_handler,
		  I(CONF_scrollbar_in_fullscreen));
    /*
     * Really this wants to go just after `Display scrollbar'. See
     * if we can find that control, and do some shuffling.
     */
    {
        int i;
        for (i = 0; i < s->ncontrols; i++) {
            c = s->ctrls[i];
            if (c->generic.type == CTRL_CHECKBOX &&
                c->generic.context.i == CONF_scrollbar) {
                /*
                 * Control i is the scrollbar checkbox.
                 * Control s->ncontrols-1 is the scrollbar-in-FS one.
                 */
                if (i < s->ncontrols-2) {
                    c = s->ctrls[s->ncontrols-1];
                    memmove(s->ctrls+i+2, s->ctrls+i+1,
                            (s->ncontrols-i-2)*sizeof(union control *));
                    s->ctrls[i+1] = c;
                }
                break;
            }
        }
    }

    /*
     * Windows has the AltGr key, which has various Windows-
     * specific options.
     */
    s = ctrl_getset(b, "터미널/키보드", "features",
		    "키보드 부가 기능:");
    ctrl_checkbox(s, "회색 Alt(AltGr)키를 조합 키로 사용", 't',
		  HELPCTX(keyboard_compose),
		  conf_checkbox_handler, I(CONF_compose_key));
    ctrl_checkbox(s, "Ctrl-Alt를 회색 Alt(AltGr)키와 구분", 'd',
		  HELPCTX(keyboard_ctrlalt),
		  conf_checkbox_handler, I(CONF_ctrlaltkeys));
#ifdef SUPPORT_CYGTERM
    ctrl_checkbox(s, "(Escape 키 대신) Alt키로 meta bit을 설정", NO_SHORTCUT,
		  HELPCTX(no_help),
		  conf_checkbox_handler, I(CONF_alt_metabit));
#endif

    /*
     * Windows allows an arbitrary .WAV to be played as a bell, and
     * also the use of the PC speaker. For this we must search the
     * existing controlset for the radio-button set controlling the
     * `beep' option, and add extra buttons to it.
     * 
     * Note that although this _looks_ like a hideous hack, it's
     * actually all above board. The well-defined interface to the
     * per-platform dialog box code is the _data structures_ `union
     * control', `struct controlset' and so on; so code like this
     * that reaches into those data structures and changes bits of
     * them is perfectly legitimate and crosses no boundaries. All
     * the ctrl_* routines that create most of the controls are
     * convenient shortcuts provided on the cross-platform side of
     * the interface, and template creation code is under no actual
     * obligation to use them.
     */
    s = ctrl_getset(b, "터미널/벨", "style", "벨 스타일 설정");
    {
	int i;
	for (i = 0; i < s->ncontrols; i++) {
	    c = s->ctrls[i];
	    if (c->generic.type == CTRL_RADIO &&
		c->generic.context.i == CONF_beep) {
		assert(c->generic.handler == conf_radiobutton_handler);
		c->radio.nbuttons += 2;
		c->radio.buttons =
		    sresize(c->radio.buttons, c->radio.nbuttons, char *);
		c->radio.buttons[c->radio.nbuttons-1] =
		    dupstr("다른 사운드 파일 사용");
		c->radio.buttons[c->radio.nbuttons-2] =
		    dupstr("PC 스피커 비프음 사용");
		c->radio.buttondata =
		    sresize(c->radio.buttondata, c->radio.nbuttons, intorptr);
		c->radio.buttondata[c->radio.nbuttons-1] = I(BELL_WAVEFILE);
		c->radio.buttondata[c->radio.nbuttons-2] = I(BELL_PCSPEAKER);
		if (c->radio.shortcuts) {
		    c->radio.shortcuts =
			sresize(c->radio.shortcuts, c->radio.nbuttons, char);
		    c->radio.shortcuts[c->radio.nbuttons-1] = NO_SHORTCUT;
		    c->radio.shortcuts[c->radio.nbuttons-2] = NO_SHORTCUT;
		}
		break;
	    }
	}
    }
    ctrl_filesel(s, "벨 소리로 사용할 사운드 파일:", NO_SHORTCUT,
		 FILTER_WAVE_FILES, FALSE, "벨 소리 파일 선택",
		 HELPCTX(bell_style),
		 conf_filesel_handler, I(CONF_bell_wavefile));

    /*
     * While we've got this box open, taskbar flashing on a bell is
     * also Windows-specific.
     */
    ctrl_radiobuttons(s, "벨이 울리면 작업표시줄에 표시:", 'i', 3,
		      HELPCTX(bell_taskbar),
		      conf_radiobutton_handler,
		      I(CONF_beep_ind),
		      "사용 안 함", I(B_IND_DISABLED),
		      "깜빡임", I(B_IND_FLASH),
		      "계속", I(B_IND_STEADY), NULL);

    /*
     * The sunken-edge border is a Windows GUI feature.
     */
    s = ctrl_getset(b, "창/모양", "border",
		    "창 테두리 설정");
    ctrl_checkbox(s, "튀어나온 창틀(좀 더 두터움)", 's',
		  HELPCTX(appearance_border),
		  conf_checkbox_handler, I(CONF_sunken_edge));

    s = ctrl_getset(b, "창/모양", "font",
		    "폰트 설정");
    /*
     * HACK: iPuTTY
     */
    ctrl_checkbox(s, "유니코드는 별도의 글꼴 사용", 'f',
		  HELPCTX(no_help),
		  conf_checkbox_handler, I(CONF_use_font_unicode));
    ctrl_fontsel(s, "유니코드용 글꼴", 's',
		 HELPCTX(no_help),
		 conf_fontsel_handler, I(CONF_font_unicode));
    ctrl_editbox(s, "유니코드 폰트 조정(px)", 'a', 20,
		 HELPCTX(no_help),
		 conf_editbox_handler, I(CONF_font_unicode_adj), I(-1));
    ctrl_checkbox(s, "가변폭 글꼴 선택 허용", NO_SHORTCUT,
                  HELPCTX(appearance_font), variable_pitch_handler, I(0));
    /*
     * Configurable font quality settings for Windows.
     */
    ctrl_radiobuttons(s, "글꼴 품질:", 'q', 2,
		      HELPCTX(appearance_font),
		      conf_radiobutton_handler,
		      I(CONF_font_quality),
		      "Antialiased", I(FQ_ANTIALIASED),
		      "Non-Antialiased", I(FQ_NONANTIALIASED),
		      "ClearType", I(FQ_CLEARTYPE),
		      "기본", I(FQ_DEFAULT), NULL);

    /*
     * Cyrillic Lock is a horrid misfeature even on Windows, and
     * the least we can do is ensure it never makes it to any other
     * platform (at least unless someone fixes it!).
     */
    s = ctrl_getset(b, "창/변환", "tweaks", NULL);
    ctrl_checkbox(s, "CapsLock을 라틴/키릴 전환키로 사용", 's',
		  HELPCTX(translation_cyrillic),
		  conf_checkbox_handler,
		  I(CONF_xlat_capslockcyr));

    /*
     * On Windows we can use but not enumerate translation tables
     * from the operating system. Briefly document this.
     */
    s = ctrl_getset(b, "창/변환", "trans",
		    "수신한 데이터의 문자셋");
    ctrl_text(s, "(Windows에서 지원하는 코드페이지 이지만 목록에 없을 수 있습니다. "
	      "예를 들어 CP866과 같이 직접 입력하여 사용할 수 있습니다.)",
	      HELPCTX(translation_codepage));

    /*
     * Windows has the weird OEM font mode, which gives us some
     * additional options when working with line-drawing
     * characters.
     */
    str = dupprintf("%s 선 그리기 처리 방법", appname);
    s = ctrl_getset(b, "창/변환", "linedraw", str);
    sfree(str);
    {
	int i;
	for (i = 0; i < s->ncontrols; i++) {
	    c = s->ctrls[i];
	    if (c->generic.type == CTRL_RADIO &&
		c->generic.context.i == CONF_vtmode) {
		assert(c->generic.handler == conf_radiobutton_handler);
		c->radio.nbuttons += 3;
		c->radio.buttons =
		    sresize(c->radio.buttons, c->radio.nbuttons, char *);
		c->radio.buttons[c->radio.nbuttons-3] =
		    dupstr("X윈도우 인코딩 사용");
		c->radio.buttons[c->radio.nbuttons-2] =
		    dupstr("ANSI와 OEM 모드 모두 사용");
		c->radio.buttons[c->radio.nbuttons-1] =
		    dupstr("OEM 모드 글꼴만 사용");
		c->radio.buttondata =
		    sresize(c->radio.buttondata, c->radio.nbuttons, intorptr);
		c->radio.buttondata[c->radio.nbuttons-3] = I(VT_XWINDOWS);
		c->radio.buttondata[c->radio.nbuttons-2] = I(VT_OEMANSI);
		c->radio.buttondata[c->radio.nbuttons-1] = I(VT_OEMONLY);
		if (!c->radio.shortcuts) {
		    int j;
		    c->radio.shortcuts = snewn(c->radio.nbuttons, char);
		    for (j = 0; j < c->radio.nbuttons; j++)
			c->radio.shortcuts[j] = NO_SHORTCUT;
		} else {
		    c->radio.shortcuts = sresize(c->radio.shortcuts,
						 c->radio.nbuttons, char);
		}
		c->radio.shortcuts[c->radio.nbuttons-3] = 'x';
		c->radio.shortcuts[c->radio.nbuttons-2] = 'b';
		c->radio.shortcuts[c->radio.nbuttons-1] = 'e';
		break;
	    }
	}
    }

    /*
     * RTF paste is Windows-specific.
     */
    s = ctrl_getset(b, "창/선택", "format",
		    "붙여넣기 형식");
    ctrl_checkbox(s, "클립보드로 텍스트와 RTF를 동시에 복사", 'f',
		  HELPCTX(selection_rtf),
		  conf_checkbox_handler, I(CONF_rtf_paste));

    /*
     * Windows often has no middle button, so we supply a selection
     * mode in which the more critical Paste action is available on
     * the right button instead.
     */
    s = ctrl_getset(b, "창/선택", "mouse",
		    "마우스 설정");
    ctrl_radiobuttons(s, "마우스 버튼 설정:", 'm', 1,
		      HELPCTX(selection_buttons),
		      conf_radiobutton_handler,
		      I(CONF_mouse_is_xterm),
		      "윈도우 (휠 확장, 우측 메뉴 띄움)", I(2),
		      "타혐 (휠 확장, 우측 붙여넣기)", I(0),
		      "xterm (우측 확장, 휠 붙여넣기)", I(1), NULL);
    /*
     * This really ought to go at the _top_ of its box, not the
     * bottom, so we'll just do some shuffling now we've set it
     * up...
     */
    c = s->ctrls[s->ncontrols-1];      /* this should be the new control */
    memmove(s->ctrls+1, s->ctrls, (s->ncontrols-1)*sizeof(union control *));
    s->ctrls[0] = c;

    /*
     * Logical palettes don't even make sense anywhere except Windows.
     */
    s = ctrl_getset(b, "창/색상", "general",
		    "색상 관련 옵션");
    ctrl_checkbox(s, "논리 파레트 사용 시도", 'l',
		  HELPCTX(colours_logpal),
		  conf_checkbox_handler, I(CONF_try_palette));
    ctrl_checkbox(s, "시스템 색상 사용", 's',
                  HELPCTX(colours_system),
                  conf_checkbox_handler, I(CONF_system_colour));


    /*
     * Resize-by-changing-font is a Windows insanity.
     */
    s = ctrl_getset(b, "창", "size", "Set the size of the window");
    ctrl_radiobuttons(s, "창 크기 변경 시:", 'z', 1,
		      HELPCTX(window_resize),
		      conf_radiobutton_handler,
		      I(CONF_resize_action),
		      "가로 세로 크기 조절", I(RESIZE_TERM),
		      "글꼴 크기 조절", I(RESIZE_FONT),
		      "최대화 되었을 때만 글꼴 크리 조정", I(RESIZE_EITHER),
		      "크기 재조정 안 함", I(RESIZE_DISABLED), NULL);

    /*
     * Most of the Window/Behaviour stuff is there to mimic Windows
     * conventions which PuTTY can optionally disregard. Hence,
     * most of these options are Windows-specific.
     */
    s = ctrl_getset(b, "창/특성", "main", NULL);
    ctrl_checkbox(s, "Alt-F4를 누르면 창을 닫음", '4',
		  HELPCTX(behaviour_altf4),
		  conf_checkbox_handler, I(CONF_alt_f4));
    ctrl_checkbox(s, "Alt-Space를 누르면 시스템 메뉴 나옴", 'm',  /* HACK: PuttyTray: Changed shortcut key */
		  HELPCTX(behaviour_altspace),
		  conf_checkbox_handler, I(CONF_alt_space));
    ctrl_checkbox(s, "Alt만 누르면 시스템 메뉴 나옴", 'l',
		  HELPCTX(behaviour_altonly),
		  conf_checkbox_handler, I(CONF_alt_only));
    ctrl_checkbox(s, "창 항상 맨 위에", 'e',
		  HELPCTX(behaviour_alwaysontop),
		  conf_checkbox_handler, I(CONF_alwaysontop));
    ctrl_checkbox(s, "Alt-Enter를 누르면  전체 화면으로 전환", 'f',
		  HELPCTX(behaviour_altenter),
		  conf_checkbox_handler,
		  I(CONF_fullscreenonaltenter));

    /*
     * HACK: PuttyTray
     */
    ctrl_radiobuttons(s, "트레이 아이콘 보이기:", NO_SHORTCUT, 4,
		      HELPCTX(no_help),
		      conf_radiobutton_handler,
		      I(CONF_tray),
		      "보통", 'n', I(TRAY_NORMAL),
		      "항상", 'y', I(TRAY_ALWAYS),
		      "숨김", 'r', I(TRAY_NEVER),
		      "시작 시", 's', I(TRAY_START), NULL);
    ctrl_checkbox(s, "트레이 아이콘 클릭 시 창 전환", 'm',
		  HELPCTX(no_help),
		  conf_checkbox_handler, I(CONF_tray_restore));

    /*
     * HACK: PuttyTray / Session Icon
     */
    s = ctrl_getset(b, "창/특성", "icon", "아이콘 설정");
    ctrl_columns(s, 3, 40, 20, 40);
    c = ctrl_text(s, "창 / 트레이 아이콘:", HELPCTX(appearance_title));
    c->generic.column = 0;
    c = ctrl_icon(s, HELPCTX(appearance_title),
		  I(CONF_win_icon));
    c->generic.column = 1;
    c = ctrl_pushbutton(s, "아이콘 변경...", 'h', HELPCTX(appearance_title),
			window_icon_handler, P(c));
    c->generic.column = 2;
    ctrl_columns(s, 1, 100);

    /*
     * HACK: PuttyTray / Transparency
     */
    s = ctrl_getset(b, "창", "main", "창 투명도 옵션");
    ctrl_editbox(s, "투명도 (50-255)", 't', 30, HELPCTX(no_help), conf_editbox_handler, I(CONF_transparency), I(-1));

    /*
     * HACK: PuttyTray / Reconnect
     */
    s = ctrl_getset(b, "연결", "reconnect", "재접속 옵션");
    ctrl_checkbox(s, "절전 모드에서 깨어날 시에 재접속", 'w', HELPCTX(no_help), conf_checkbox_handler, I(CONF_wakeup_reconnect));
    ctrl_checkbox(s, "접속 실패시 재접속", 'w', HELPCTX(no_help), conf_checkbox_handler, I(CONF_failure_reconnect));

    /*
     * HACK: PuttyTray / Nutty
     * Hyperlink stuff: The Window/Hyperlinks panel.
     */
    ctrl_settitle(b, "창/하이퍼링크", "하이퍼링크 관련 설정");
    s = ctrl_getset(b, "창/하이퍼링크", "general", "하이퍼링크 설정");

    ctrl_radiobuttons(s, "하이퍼링크:", NO_SHORTCUT, 1,
		      HELPCTX(no_help),
		      conf_radiobutton_handler,
		      I(CONF_url_underline),
		      "항상", NO_SHORTCUT, I(URLHACK_UNDERLINE_ALWAYS),
		      "마우스 오버시", NO_SHORTCUT, I(URLHACK_UNDERLINE_HOVER),
		      "사용하지 않음", NO_SHORTCUT, I(URLHACK_UNDERLINE_NEVER),
		      NULL);

    ctrl_checkbox(s, "ctrl+click 으로 하이퍼링크 실행", 'l',
		  HELPCTX(no_help),
		  conf_checkbox_handler, I(CONF_url_ctrl_click));

    s = ctrl_getset(b, "창/하이퍼링크", "browser", "브라우저 선택");

    ctrl_checkbox(s, "기본 브라우저 사용", 'b',
		  HELPCTX(no_help),
		  conf_checkbox_handler, I(CONF_url_defbrowser));

    ctrl_filesel(s, "또는 다른 어플리케이션으로 하이퍼링크 실행:", 's',
		 "응용프로그램 (*.exe)\0*.exe\0모든 파일 (*.*)\0*.*\0\0", TRUE,
		 "하이퍼링크를 열기위한 실행파일 선택", HELPCTX(no_help),
		 conf_filesel_handler, I(CONF_url_browser));

    s = ctrl_getset(b, "창/하이퍼링크", "regexp", "정규 표현식");

    ctrl_checkbox(s, "기본 정규 표현식 사용", 'r',
		  HELPCTX(no_help),
		  conf_checkbox_handler, I(CONF_url_defregex));

    ctrl_editbox(s, "또는 사용자 정의:", NO_SHORTCUT, 100,
		 HELPCTX(no_help),
		 conf_editbox_handler, I(CONF_url_regex),
		 I(1));

    ctrl_text(s, "하나의 공백문자가 링크 앞에 존재할 경우 제거됩니다.",
	      HELPCTX(no_help));

    /*
     * Windows supports a local-command proxy. This also means we
     * must adjust the text on the `Telnet command' control.
     */
    if (!midsession) {
	int i;
        s = ctrl_getset(b, "연결/프록시", "basics", NULL);
	for (i = 0; i < s->ncontrols; i++) {
	    c = s->ctrls[i];
	    if (c->generic.type == CTRL_RADIO &&
		c->generic.context.i == CONF_proxy_type) {
		assert(c->generic.handler == conf_radiobutton_handler);
		c->radio.nbuttons++;
		c->radio.buttons =
		    sresize(c->radio.buttons, c->radio.nbuttons, char *);
		c->radio.buttons[c->radio.nbuttons-1] =
		    dupstr("Local");
		c->radio.buttondata =
		    sresize(c->radio.buttondata, c->radio.nbuttons, intorptr);
		c->radio.buttondata[c->radio.nbuttons-1] = I(PROXY_CMD);
		break;
	    }
	}

	for (i = 0; i < s->ncontrols; i++) {
	    c = s->ctrls[i];
	    if (c->generic.type == CTRL_EDITBOX &&
		c->generic.context.i == CONF_proxy_telnet_command) {
		assert(c->generic.handler == conf_editbox_handler);
		sfree(c->generic.label);
		c->generic.label = dupstr("Telnet 명령 또는 로컬"
					  " 프록시 명령");
		break;
	    }
	}
    }

    /*
     * Serial back end is available on Windows.
     */
    if (!midsession || (protocol == PROT_SERIAL))
        ser_setup_config_box(b, midsession, 0x1F, 0x0F);

    /*
     * $XAUTHORITY is not reliable on Windows, so we provide a
     * means to override it.
     */
    if (!midsession && backend_from_proto(PROT_SSH)) {
	s = ctrl_getset(b, "연결/SSH/X11", "x11", "X11 포워딩");
	ctrl_filesel(s, "로컬 출력을 위한 X 권한 파일", 't',
		     NULL, FALSE, "X 권한 파일 선택",
		     HELPCTX(ssh_tunnels_xauthority),
		     conf_filesel_handler, I(CONF_xauthfile));
    }
#ifdef SUPPORT_CYGTERM
    /*
     * cygterm back end is available on Windows.
     */
    if (!midsession || (protocol == PROT_CYGTERM))
	cygterm_setup_config_box(b, midsession);
#endif
}

// vim: ts=8 sts=4 sw=4 noet cino=\:2\=2(0u0
