# 3단계 — 이동·카메라·자세

## 14. 이동·자세 기능 범위 정리

CharacterMovement를 새로 대체하지 않고 UE의 이동 예측·보정을 사용하기로 했다. CharacterBase에는 공통 동작, PlayerCharacter에는 입력만 남기도록 구분했다.

**구현 및 판단**

- 기본 이동, 점프, Sprint, 카메라, Pose 순환을 이번 단계에 포함
- 유물·전투·스킬의 상태는 미리 추가하지 않음
- 이동 상태와 행동 상태를 중복 저장하지 않는 기준 수립

**결과**

이동 시스템을 확장하더라도 기존 CharacterMovement의 예측 경로를 유지한다.

## 15. Enhanced Input 설정과 이동 입력 연결

Input Action과 Mapping Context를 C++ 바인딩으로 연결했다. DataAsset이 에셋 참조를 제공하고 Character가 실제 입력 함수를 처리하는 구조를 정리했다.

**구현 및 판단**

- Move/Look/Jump/Sprint 입력 경로 구분
- Blueprint는 Input Action과 Mapping Context 에셋 참조 연결
- Local 입력 처리를 권한이 필요한 Gameplay 변화와 분리

**결과**

에셋만 생성했을 때 입력이 동작하지 않는 문제를 피하기 위해 Mapping Context 등록과 BindAction을 따로 확인했다.

## 16. Sprint 서버 권한과 예측 점검

Sprint 요청의 서버 경로를 구성하고 속도 변경 위치를 CharacterMovement로 집중시켰다. Visual Studio IntelliSense가 서버 RPC 구현 함수에 오류를 표시했지만 UHT/빌드 결과와 구분했다.

**구현 및 판단**

- `ServerRequestSprint(bool)` 선언과 `_Implementation` 정의 관계 확인
- 빠른 이동 요청이 Pose 제약과 충돌하지 않는지 확인
- 걷기 복귀시 저장된 기본 이동 속도를 재사용

**결과**

편집기의 IntelliSense 진단만으로 컴파일 실패로 판정하지 않았다. 실제 빌드 결과를 기준으로 확인했다. 이후 Sprint는 `FSavedMove`의 `FLAG_Custom_0`으로 옮겨 예측·보정에 참여시켰고, 지금은 별도 Sprint RPC가 없다([입력과 이동](../Docs/Technical/05_Input_Movement.md)).

## 17. 1인칭·3인칭 시점과 회전 정책

1인칭과 3인칭에서 카메라와 캐릭터가 회전하는 규칙을 나눴다. Pose 상태에서도 카메라 입력은 별도로 처리하도록 했다.

**구현 및 판단**

- Local Camera와 서버에 의미가 있는 Character Rotation 구분
- 입력 View와 PoseType을 별도 상태로 관리
- 시점 전환 뒤 마우스 Look 동작 확인

**결과**

Pose 때문에 시점이 고정되거나 캐릭터 몸체가 의도치 않게 따라도는 문제를 집중 점검했다.

## 18. Pose 순환과 입력 Context 전환

Pose1→Pose2→Pose3→Pose1 순환을 만들고 Default 복귀 규칙을 정리했다. Pose 진입과 종료에서는 Mapping Context가 바뀌어도 눌린 키가 남지 않도록 했다.

**구현 및 판단**

- `IMC_Gameplay`와 `IMC_Pose` 분리
- Pose 중 이동·Sprint 제한, 시점 독립 정책
- 상태의 원본은 PoseComponent에서 관리

**결과**

빠른 연속 입력, Pose 취소, 이동 중 진입 시나리오를 통해 입력 상태가 꼬이는지 확인했다.

## 19. 젤리 캐릭터 메시와 AnimBP 연결

공통 Skeletal Mesh, Skeleton, Physics Asset을 기반으로 Player와 NPC가 같은 외형을 사용하는 방향으로 진행했다. 이동 애니메이션과 특수 Pose 재생 위치를 구분했다.

**구현 및 판단**

- FBX Skeleton과 애니메이션 Skeleton 일치 점검
- Locomotion State Machine과 Slot/Pose Blend 구분
- Animation Blueprint에서 C++ 상태를 읽는 구조 사용

**결과**

C++은 이동·자세 상태만 제공하고, 메시·AnimBP 연결은 에디터 에셋에서 관리하도록 경계를 나눴다.

## 20. Multiplayer PIE 이동·Pose 검수

2인 Listen Server 환경에서 이동, 점프, Sprint 제한, Pose 복제, 카메라와 몸체 회전 분리, 빠른 입력을 검수했다. 3인 로비에서 Match Travel·BaseSlot 할당 로그도 확인했다.

**구현 및 판단**

- 2인 로그: Pose1→2→3→1, Default 종료, Sprint 제약 확인
- 3인 로그: ExpectedPlayers=3, BaseSlots 0/1/2, Countdown→Playing 확인

**결과**

2인 기준으로 이동·Pose 단계를 마무리하고, 3인 원격 Pose 교차 복제는 이후 통합 테스트에서 함께 보기로 했다.

## 21. Source 인코딩과 파일 배치 정리

C++ 주석과 문자열이 섞인 소스에서 UTF-8/CP949 인코딩 차이를 발견했다. 클래스 책임 변경 없이 함수·region 배치와 소스 인코딩 정리를 검토했다.

**구현 및 판단**

- `DMJellyAnimInstance.cpp`, `DMPoseComponent.cpp`, `DMPlayerCharacter.cpp` 인코딩 불일치 확인
- 소스 인코딩 기준을 UTF-8 BOM + CRLF로 통일
- 파일 전체 교체 전 Git Diff로 실제 변경 범위를 확인

**결과**

인코딩 변환과 코드 구조 정리는 게임 기능 수정과 별도 작업으로 기록했다.
