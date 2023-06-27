Brand new pintos for Operating Systems and Lab (CS330), KAIST, by Youngjin Kwon.

The manual is available at https://casys-kaist.github.io/pintos-kaist/.

### 추가 참고 자료

- [KAIST Pintos Slide Set](https://oslab.kaist.ac.kr/pintosslides/)
- [한양대 핀토스 강의 자료](http://broadpeak.kaist.ac.kr/wiki/dmcweb/download/%EC%9A%B4%EC%98%81%EC%B2%B4%EC%A0%9C_%EC%8B%A4%EC%8A%B5_%EA%B0%95%EC%9D%98%EC%9E%90%EB%A3%8C.pdf)

### 개발 및 테스트 환경

AWS EC2 Ubuntu 18.04 (x86_64)

    $ sudo apt update
    $ sudo apt install -y gcc make qemu-system-x86 python3

## PJ1. Threads

제대로 동작되는지 테스트 결과 확인

    $ source ./activate
    $ cd threads
    $ make check

### 1-1) Alarm clock

- alarm : 호출한 프로세스를 정해진 시간 후에 다시 시작하는 커널 내부 함수
- 핀토스에는 알람 기능이 Busy waiting을 이용하여 구현되어 있습니다.
- busy waiting은 스레드가 CPU를 점유하면서 대기하고 있는 상태로, 권한을 얻을 때까지 확인하는 것을 의미합니다. 이 경우 CPU의 자원을 불필요하게 낭비하기 때문에 좋지 않은 쓰레드 동기화 방식입니다.
- 이 알람 기능을 sleep/wake up을 이용하여 재구현 하는 것이 목표입니다.

<img width="692" alt="스크린샷 2023-06-05 오전 10 58 48" src="https://github.com/Riudiu/pintos/assets/86466976/9b74d673-0334-4c96-8272-fd3f0abdf9c3">

<img width="927" alt="스크린샷 2023-06-05 오전 11 01 05" src="https://github.com/Riudiu/pintos/assets/86466976/4bff3c57-77e0-49db-b572-02d5888cf58b">


### 1-2) Priority Scheduling

- 핀토스의 스케줄러는 라운드 로빈으로 구현되어 있습니다. 
- 라운드 로빈 스케줄링은 시분할 시스템을 위해 설계된 선점형 스케줄링의 하나로서, 우선순위와 관련 없이 시간 순서대로 CPU를 할당하는 방식입니다.
- 즉, 기존 핀토스에서는 ready list에 삽입된 순으로 thread가 CPU를 점유하게 됩니다. 이 경우 우선순위가 낮거나 하나의 CPU 사용 시간이 길어지면 자원 낭비와 성능이 저하될 우려가 있습니다. 
- 따라서 이를 우선순위를 고려하여 스케줄링 하도록 수정하는 것이 목표입니다.

![스크린샷 2023-06-05 오전 11 13 56](https://github.com/Riudiu/pintos/assets/86466976/1877700d-2692-4bdd-9634-a93b4bf97bc9)


### 1-3) Priority Inversion Problem

> 우선순위가 높은 쓰레드가 우선순위 낮은 쓰레드를 기다리는 현상

- 우선 순위 스케줄링의 문제점 중 하나는 'priority inversion'입니다. 
- H, M, L 을 각각 높은, 중간, 낮은 우선 순위의 thread라 해봅시다. H가 L을 기다려야 하고(예를 들면, L이 lock을 걸은 경우) M은 ready list에 있다면, L이 CPU 이용 시간을 얻지 못하기 때문에, H도 절대 CPU를 사용하지 못하게 될 것입니다.
- Priority donation을 구현하여 lock을 보유한 낮은 우선순위의 thread에게 자신의 우선순위를 기부하도록 합니다. 그리고 lock이 풀리면 기부한 우선순위를 다시 가져오도록 구현하는 것이 목표입니다.

![스크린샷 2023-06-05 오전 11 23 59](https://github.com/Riudiu/pintos/assets/86466976/993b2e64-4ddc-4b04-a514-ad12890639a7)


## PJ2. User Programs

제대로 동작되는지 테스트 결과 확인

    $ source ./activate
    $ cd userprog
    $ make check

### 2-1) Argument Passing

- 핀토스는 프로그램과 인자(argument)를 구분하지 못하는 구조를 가지고 있습니다. 즉 적은 명령어 전체를 하나의 프로그램으로 인식하게 구현되어 있습니다.
- 프로그램 이름과 인자를 구분하여(받은 문자열을 parsing후 argv, argc에 저장) 스택에 저장하고, 인자를 응용 프로그램에 전달하는 기능을 구현하는 것이 목표입니다. 

![image](https://github.com/Riudiu/pintos/assets/86466976/6e7faaf8-fe14-49d1-b242-748cdb9c08a3)


### 2-2) System Call

- 핀토스는 일반적인 os와 달리 스레드가 곧 user_program인 간단한 모델을 사용하고 있습니다. 
- 시스템 콜(System Call)은 컴퓨터 운영체제(OS)의 커널에 제공되는 인터페이스로, 응용 프로그램은 시스템 콜을 통해 운영체제에게 서비스를 요청하고 운영체제는 이러한 요청을 수행하여 응용 프로그램에 필요한 작업을 수행합니다.
- 시스템 콜(halt, exit, create, remove 등등)을 구현하고, 시스템 콜 핸들러를 통해 호출하도록 하는 것이 목표입니다.

<img width="923" alt="스크린샷 2023-06-12 오전 4 39 22" src="https://github.com/Riudiu/pintos/assets/86466976/ddddd311-5739-41b3-bfa5-c3311024d867">

<img width="957" alt="스크린샷 2023-06-12 오전 4 42 36" src="https://github.com/Riudiu/pintos/assets/86466976/c22f697d-fecc-458a-abda-34b7887249cb">


## PJ3. Virtual Memory

제대로 동작되는지 테스트 결과 확인

    $ source ./activate
    $ cd vm
    $ make check

### 3-1) Memory Management

- Pintos는 가상 및 물리적 메모리 매핑을 관리하기 위한 페이지 테이블(pml4)을 가지고 있습니다. pml4 이외에 추가로, page fault 및 리소스 관리를 처리하기 위해 supplementary page table이 필요합니다.
- 그리고 모든 page는 메모리가 구성될 때 메모리에 대한 메타 데이터를 보유하고 있지 않습니다. 따라서 물리적 메모리를 관리하기 위해서는 다른 체계가 필요합니다.
- 해시 테이블을 이용해서 supplementary page table을 구현하고, 물리 메모리 내 각 프레임 정보를 갖고 있는 frame table을 구현하여 메모리 관리 인터페이스를 만드는 것이 목표입니다. 

![스크린샷 2023-06-17 오후 1 51 29](https://github.com/Riudiu/pintos/assets/86466976/6bfbac0e-6eec-459b-aa1e-0491aac4943e)

구현 후 대략적인 layout (vm_entry -> page)

<img width="935" alt="스크린샷 2023-06-17 오후 4 50 49" src="https://github.com/Riudiu/pintos/assets/86466976/0e35dbad-7a96-4e48-b919-08779f06605a">


### 3-2) Anonymous Page

- Anonymous page는 프로세스의 heap 또는 stack에 할당되었지만 disk의 특정 파일과 연결되지 않은 메모리 페이지를 나타냅니다. 즉 file-backed page와 달리 contents를 가져올 file이나 device가 없는 page를 말합니다. 또한 디스크에 저장된 데이터를 갖고 있지 않으므로, 해당 페이지를 물리적 메모리에 로드할 필요가 없습니다.
- project 2까지의 Pintos는 프로세스가 실행될 때 segment를 physical memory에 직접 load하는 방식을 사용합니다. 처음에 물리 frame부터 할당을 받고 파일을 해당 frame에 load한 다음, 페이지 테이블에서 가상 주소와 물리 주소를 맵핑하는 방식입니다. 즉 프로그램의 모든 segment에 대해 physical page를 할당하고 있습니다. 그래서 page fault가 발생하면 강제 종료가 됩니다. 
- 따라서 앞서 구현한 spt에 필요한 정보를 넣어서 page fault가 발생했을 때(페이지가 필요한 경우에만) 메모리에 load하는 방식인 lazy load 방식으로 구현하는 것이 이번 목표입니다. 

![151002575-fd39dad3-3a60-454a-b801-d4e3bb96b14c](https://github.com/Riudiu/pintos/assets/86466976/0cc5512b-1396-4e75-8ff7-2c7441407bd5)


### 3-3) Stack Growth

- 현재 핀토스에서 사용하는 스택은 USER_STACK을 시작으로 하는 단일 페이지였으며, 프로그램의 실행은 이 크기로 제한되어 있습니다.
- Stack Growth를 구현하여 page fault가 발생했을 때 접근한 가상 주소가 stack 안에 있는지 판별하고, stack에 접근한 경우 stack이 현재 크기를 초과하면 추가 페이지를 할당하도록 합니다.

![스크린샷 2023-06-21 오후 5 17 32](https://github.com/Riudiu/pintos/assets/86466976/2ddf411d-673c-44c3-b666-b2691b3fcc4c)


### 3-4) Memory Mapped Files

- 이번에는 memory-mapped page를 구현합니다. Anonymous Page와 달리 memory-mapped page는 파일 기반 매핑입니다.
- page fault가 발생하면 물리적 프레임이 즉시 할당되고, 내용이 파일에서 메모리로 복사됩니다. memory-mapped page가 매핑 해제되거나 교체(스왑아웃)되면 콘텐츠의 모든 변경 사항이 파일에 반영됩니다.

- 파일 매핑 메커니즘
<img width="958" alt="스크린샷 2023-06-22 오후 5 23 26" src="https://github.com/Riudiu/pintos/assets/86466976/771868ef-6521-41da-8bba-4318ed2d245f">

- 매핑된 파일 관리
<img width="958" alt="스크린샷 2023-06-22 오후 5 26 35" src="https://github.com/Riudiu/pintos/assets/86466976/dfce081d-82de-4efd-80f4-00eff0d361f8">

- munmap() 동작
<img width="955" alt="스크린샷 2023-06-22 오후 5 29 47" src="https://github.com/Riudiu/pintos/assets/86466976/31963054-244f-4156-8c87-766e8dfeace1">


### 3-5) Swap In/Out

- 메모리 swapping은 메모리 내용을 디스크로 방출 및 메모리에 탑재하는 기법입니다. 물리 메모리의 사용을 최대화하는 메모리 회수 기술이기도 하며 OS에 의해 이뤄집니다. 
- 시스템이 run out of memory(메모리 부족)을 감지한 상황에서 메모리 할당 요청을 받았을 때, 시스템은 디스크로 swap시키기 위해 제거할 페이지를 선택합니다. 
- 그리고 메모리 프레임의 정확한 상태가 디스크에 복사됩니다. 프로세스가 swapped out된 페이지에 접근하려고 시도할 때, OS는 정확한 내용을 메모리로 다시 가져와 페이지를 복구합니다.
- 제거를 위해 선택한 페이지는 anonymous 혹은 file-backed page일 수 있습니다. 각 케이스별로 메모리 swapping을 구현하는 것이 이번 목표입니다. 

![스크린샷 2023-06-23 오후 4 31 52](https://github.com/Riudiu/pintos/assets/86466976/e1ebb84b-e53c-4b36-ab6e-477839741f27)


## PJ4. File System

제대로 동작되는지 테스트 결과 확인

    $ source ./activate
    $ cd filesys
    $ make check

### 4-1) Indexed and Extensible Files

- 기존의 파일 시스템은 파일을 단일 범위(연속 할당)로 파일을 할당하는데, 이는 외부 단편화에 취약합니다. On-disk inode 구조를 수정하여 이 문제를 해결해야 합니다. 
- 그리고 기본 파일 시스템에선 파일 크기는 파일이 생성될 때 정해집니다. 하지만 대부분 현대 파일 시스템에서는 파일은 처음에 사이즈 0으로 생성되고 매 시간 write가 완료될 때마다 크기가 확장됩니다.
- 파일 시스템 할당을 더 쉽게 구현하기 위해 주어진 스켈레톤 코드로 FAT(File Allocation Table)를 구현하고, 파일에 쓰기 동작을 수행할 때에 디스크 블록을 할당 받아 사용하도록 구현하여 파일 크기가 생성 시에 고정되지 않고, 확장 가능하도록 만드는 것이 이번 목표입니다. 

- 수정 전
<img width="924" alt="스크린샷 2023-06-26 오후 8 28 40" src="https://github.com/Riudiu/pintos/assets/86466976/0a6ab225-9c71-4908-bb9b-09bebb28c636">

- 수정 후
<img width="924" alt="스크린샷 2023-06-26 오후 8 28 53" src="https://github.com/Riudiu/pintos/assets/86466976/0b42fc67-b0e5-4ce8-b048-fc2f453c22e6">

