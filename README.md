iPutty
===

## Abstract

***iPuTTY*** 는 ***PuTTY*** 의 한국어 로컬 버전이며, SSH 터미널 에뮬레이터 입니다. 현재 ***iPuTTY*** 의 주 타겟 플랫폼은 ***Microsoft Windows 7*** 이상 버전 입니다.

이 저장소는 Official ***iPuTTY*** 저장소가 아닙니다. Official site는 [iPuTTY Official site](https://bitbucket.org/daybreaker/iputty/)를 이용 하십시오. 하지만, 현재 공식 사이트는 2016/05/31 에 개발 중단을 선언한 상태이며, [HPuTTY]( https://github.com/teamnop/HPuTTY)를 이용하라고 권고하고 있습니다.

원 iPuTTY의 maintainer에게 project 포기 의사 확인 및 maintainer 권한 이양에 대하여 메일로 문의 및 답변을 기다리는 상태이며, 거절시에 프로젝트 이름이 변경될 수 있습니다. (https://bitbucket.org/daybreaker/iputty/issues/11/putty-064 참조)

이 저장소는 original iPuTTY에 PuTTY 업데이트를 반영하고 있으며, 원본 iPuTTy에서 제공하지 않는다음의 기능 개선이 있습니다.

 * psftp file listing (#1)
 * psftp(#2) / pscp(#11) UTF8 지원 
 * 한글 입력 모드에서 escape 키 눌렀을 경우 영문키보드 상태로 전환(like hanterm) (#12)

## Warning

Windwos GUI programing 경험이 없기 때문에, 이 곳에 버그를 등록하더라도 해결이 안될 수도 있습니다. 기능 수정이나 버그 픽스에 대한 패치는 환영 하지만, 문제 해결 요구에는 충분히 응대할 수 없습니다.

## Key Additions

 * ***On-the-spot IME support*** - 한글 음절의 구성을 커서에서 직접 구성 합니다. 단말기 외부에 무작위로 배치되는 IME 구성을 위한 못생긴(?) 회색 상자가 나타나지 않습니다.
 * ***CP949/UTF-8 quick switching menu*** - 기본으로 UTF-8 인코딩을 사용합니다. CP949(또는 EUC-KR)을 사용하는 서버를 위하여 빠른 전환을 위한 메뉴를 지원 합니다.
 * ***Separate ANSI/Unicode fonts*** - 한글 음절을 포함한 유니코드 문자 표시를 위하여 동일한 터미널에서 영문폰트와 한글 폰트를 동시에 사용할 수 있습니다.
 * ***반투명창 지원***
   * 투명성을 설정할 수 있습니다.
   * 투명도를 빠르게 변경하려면 Alt+[, Alt+](거의 투명, 반투명, 불투명)을 이용할 수 있습니다 - 2개 이상의 모니터를 사용하지 않는다면, 이 투명도 설정을 이용하여 웹페이지 및 문서를 볼 수 있습니다.
   * 투명도를 미세하게 조정하려면 Alt+Shift+[, Alt+Shift+] 를 이용하십시오.
 * ***New keyboard shortcuts***
   * Ctrl+Tab 을 이용하여 PuTTY 창간에 전환을 할 수 있습니다.
 * ***Version-aware default fonts on Windows*** (예를 들어, Vista 및 Windwos 7에서는 Consolas, XP 에서는 Courier New)
 * 이제 명시적으로 설정을 하지 않으면, ***UTF-8 이 기본 문자셋*** 입니다. 최근 대부분의 리눅스 배포본이 UTF-8을 기본값으로 사용하기 때문에 매우 편리할 것입니다.
 * ***Italics font support***: xterm-256color 터미널에서 이 기능을 사용하려면, terminfo 생성기와 관련된 vimrc, bashrc, tmux.conf 등의 설정 파일을 살펴 보십시오.

## Cygterm (Cygwin terminal)

이 곳에서 배포하는 ***iPuTTY*** 는 Cygterm patch가 포함이 되어 있습니다. ***Cygterm*** 기능을 사용하기 위해서는 ***cthelper.exe*** 가 필요 합니다. ***cthelper.exe*** 는 https://github.com/Joungkyun/iputty/releases/tag/0.68 에서 배포 합니다.

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

0.69 버전 부터는 대부분의 UI 메시지가 한글로 번역이 되어 있으며, 한글화가 되어 있는 소스 코드는 [***iputty-cygterm-hangulize*** branch](https://github.com/Joungkyun/iputty/tree/iputty-cygterm-hangulize)를 이용 하십시오.

