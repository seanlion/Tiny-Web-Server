## 프로젝트 기간
2021.01 ~ 2021.02

## 설명
나만의 웹서버를 만드는 프로젝트입니다.
HTTP 요청-응답을 구현했으며, GET,HEAD,POST 메소드를 지원합니다.
동적 콘텐츠, POST 메소드는 CGI 프로그램을 이용해 구현했습니다.

1. webserver.c : 주요 로직이 담긴 파일
    - I/O 함수 : 데이터를 읽고 쓰기 위한 함수로서 Linux I/O 함수를 기반으로 가공한 wrapper 함수
    - rio_readinitb : read 버퍼를 초기화 하는 함수
    - rio_readlineb : 파일에서 텍스트 라인을 읽어 버퍼에 담는 함수
    - rio_readnb : 파일에서 지정한 바이트 크기를 버퍼에 담는다.
    - rio_writen : 버퍼에서 파일로 지정한 바이트를 전송하는 함수
    - main  : 웹 서버 메인 로직
        - open_listenfd : 위에서 설명
        - op_transaction : 한 개의 HTTP 트랜잭션을 처리하는 함수
    - read_requesthdrs : request header를 읽는 함수
    - parse_uri : 클라이언트가 요청한 URI를 파싱하는 함수
    - static_serve : 정적 콘텐츠를 제공하는 함수
    - dynamic_serve : 동적 콘텐츠를 제공하는 함수
    - get_filetype : 받아야 하는 filetype을 명시하는 함수
    - client_error : 에러 처리를 위한 함수

2. cgi-bin : cgi 프로그램이 담긴 폴더

3. 컴파일은 gcc를 활용해주세요.
    - webserver.c와 cgi-bin/adder.c 를 빌드하면 실행 가능합니다.

4. 본 프로그램은 AWS EC2(ubuntu 18.04) 환경에서 제작되었습니다. 
    - 해당 환경을 세팅 후 2가지 방법으로 확인이 가능합니다.
    - 터미널에서 `telnet` 을 활용해 서버를 실행한 컴퓨터의 ip와 포트를 입력하세요.
    - 브라우저에서 localhost:port number(EC2에서 열어놓은 포트)로 접속하세요.

## 참여자
- 허승
- 한정서
