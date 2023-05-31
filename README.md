Brand new pintos for Operating Systems and Lab (CS330), KAIST, by Youngjin Kwon.

The manual is available at https://casys-kaist.github.io/pintos-kaist/.

### 추가 참고 자료

- [KAIST Pintos Slide Set](https://oslab.kaist.ac.kr/pintosslides/)
- [한양대 핀토스 강의 자료](http://broadpeak.kaist.ac.kr/wiki/dmcweb/download/%EC%9A%B4%EC%98%81%EC%B2%B4%EC%A0%9C_%EC%8B%A4%EC%8A%B5_%EA%B0%95%EC%9D%98%EC%9E%90%EB%A3%8C.pdf)

### 테스트 환경

AWS EC2 Ubuntu 18.04 (x86_64)

    $ sudo apt update
    $ sudo apt install -y gcc make qemu-system-x86 python3

pintos가 제대로 동작되는지 확인

    $ source ./activate
    $ cd threads
    $ make check