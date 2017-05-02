iPutty-Cygterm
===

## Abstract

***iPuTTY*** 는 ***PuTTY*** 의 한국어 로컬 버전이며, SSH 터미널 에뮬레이터 입니다. 현재 ***iPuTTY*** 의 주 타겟 플랫폼은 ***Microsoft Windows 7*** 이상 버전 입니다.

***iPutty-Cygterm*** branch는 ***iPuTTY***에 PuTTyCyg 의 기능을 추가하여, ***iPutty***를 Cygwin termianl로 사용할 수 있게 해 줍니다.

## Warning

Windwos GUI programing 경험이 없기 때문에, 이 곳에 버그를 등록하더라도 해결이 안될 수도 있습니다. 기능 수정이나 버그 픽스에 대한 패치는 환영 하지만, 문제 해결 요구에는 충분히 응대할 수 없습니다.

## Key Additions

 * ***On-the-spot IME support*** - The composition of Hangul syllables happens directly at the cursor. No more ugly gray boxes for IME composition randomly positioned outside the terminal.
 * ***CP949/UTF-8 quick switching menu*** - It provides a quick-fix for legacy Linux servers.
 * ***Separate ANSI/Unicode fonts*** - You can use Consolas for ANSI characters and 돋움 for Unicode characters including Hangul syllables in the same terminal.
Semi-transparent windows
   * Transparency can be configured.
   * Alt+[, Alt+] for quick-change of transparency (among almost transparent, semi-transparent, and completely opaque) - It is great to see webpages and documents through the terminal when you do not have multiple monitors.
   * Alt+Shift+[, Alt+Shift+] for fine adjustment of transparency
 * ***New keyboard shortcuts***
   * Ctrl+Tab to switch to other PuTTY windows.
 * ***Version-aware default fonts on Windows*** (Consolas for Windows 7 and Vista, Courier New for Windows XP, for example)
 * Now, ***UTF-8 is the default encoding*** if not configured explicitly. This will be a huge convenience since most Linux servers uses it by default recently.
 * ***Italics font support***: To use it on xterm-256color terminals, have a look at my terminfo generator and related configurations such as vimrc, bashrc, and tmux.conf in the same location.

## Cygterm (Cygwin terminal)

이 곳에서 빌드한 ***iPuTTY*** 는 Cygterm patch가 포함이 되어 있습니다. ***Cygterm*** 기능을 사용하기 위해서는 ***cthelper.exe*** 가 필요 합니다. ***cthelper.exe*** 는 https://github.com/Joungkyun/iputty/releases/tag/0.68 에서 배포 합니다.

***cthelper-bin.zip*** 을 다운로드 받은 후에 압축을 해제 합니다.

***Cytwin 64bit*** 의 경우:
  1. 압축 파일에 포함된 ***cthelper64.exe*** 파일을 ***cygwin*** ***/bin*** directory에 저장 합니다.
  2. iPutty를 실행한 다음, 설정의 ***Connection*** > ***Cygterm*** 에서 ***Use Cygwin64*** 를 선택 합니다.

***Cytwin 32bit*** 의 경우:
	압축 파일에 포함된 ***cthelper.exe*** 파일을 ***cygwin*** ***/bin*** directory에 저장 합니다.

***cthelper.exe*** 복사가 완료 되었으면, ***iPuTTY*** 를 실행한 후에, connection 정보를 아래와 같이 하여 termanal을 ***iPuTTy*** 로 띄웁니다.

 * Connection Type: Cygterm
 * Host address   : -

Cygterm 기능이 포함된 소스코드는 [***iputty-cygterm*** branch](https://github.com/Joungkyun/iputty/tree/iputty-cygterm)를 이용하십시오.

## 한글 번역 버전

한글화가 되어 있는 소스 코드는 [***iputty-cygterm-hangulize*** branch](https://github.com/Joungkyun/iputty/tree/iputty-cygterm-hangulize)를 이용 하십시오.

현재 배포중인 실행파일들은 [***iputty-cygterm*** branch](https://github.com/Joungkyun/iputty/tree/iputty-cygterm)를 빌드한 것으로, 다음 버전은 [***iputty-cygterm-hangulize*** branch](https://github.com/Joungkyun/iputty/tree/iputty-cygterm-hangulize)로 배포할 예정입니다.
