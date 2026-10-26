# Don't Melt — Enhanced Input 설계

## 1. Mapping Context

| Context | 활성 상황 | 주요 입력 |
|---|---|---|
| `IMC_Gameplay` | 평상시 | 이동·전투·유물·스킬·메뉴 |
| `IMC_Pose` | 자세 모드 | 자세 순환·이동·점프 |
| `IMC_Interaction` | 상자 패널 열림 | 등록·탈취·닫기 |
| `IMC_SkillTargeting` | 스킬 대상 선택 | 랜덤·선두 대상·취소 |
| `IMC_RadialMenu` | 감정표현 휠 | 항목 선택·취소 |
| `IMC_Spectator` | 사망 후 관전 | 자유 카메라 이동·회전 |
| `IMC_Voice` | 로비·경기 공통 | PTT·마이크 음소거 |

입력 상태는 로컬 Controller와 Character가 관리한다. UI가 서버 Gameplay 상태를 직접 바꾸지 않는다. Context를 바꿀 때는 눌린 키가 새 Context로 넘어가 한 번 더 처리되지 않도록 `bIgnoreAllPressedKeysUntilRelease`를 쓴다.

## 2. 기본 입력

| Action | 키 | 동작 |
|---|---|---|
| `IA_Move` | WASD | 이동 |
| `IA_Look` | Mouse | 카메라 회전 |
| `IA_View` | RMB | 1인칭 / 3인칭 전환 |
| `IA_Jump` | Space | 점프 |
| `IA_Sprint` | Shift | 누르는 동안 달리기 |
| `IA_Interact` | F | 유물 습득(길게) 또는 상자 열기 |
| `IA_DropRelic` | Q | 운반 중인 유물 내려놓기 |
| `IA_Attack` | LMB | 서버 공격 요청 |
| `IA_Defense` | E | 방어 요청 |
| `IA_Pose` | R | 자세 모드 진입·해제 |
| `IA_EmoteWheel` | T | 감정표현 휠 |
| `IA_Skill_Invisibility` | 1 | 투명화 |
| `IA_Skill_FootstepAmplify` | 2 | 발소리 증폭 대상 선택 |
| `IA_Skill_VisionDistort` | 3 | 시야 교란 대상 선택 |
| `IA_PushToTalk` | V | 누르는 동안 음성 송신 (PTT 모드) |
| `IA_ToggleMicrophoneMute` | M | 마이크 음소거 |
| `IA_Pause` | Esc | 로컬 메뉴 (Listen Server 전체를 멈추지 않음) |

음성 입력은 별도 `IMC_Voice`에 두어, 로비(UI 입력 모드)와 경기 모두에서 같은 키로 동작하게 했다. 초기 설계에서는 `M`을 PTT로 잡았지만, 음소거 키와 함께 정리하면서 PTT를 `V`로 옮겼다.

## 3. 자세 모드

기본 → 납작 → 웅크리기 → 대자 → 기본 순서로 바뀐다. `R`로 끝내면 들어갈 때의 카메라 시점을 유지한 채 일반 입력으로 돌아간다.

| 키 | 동작 |
|---|---|
| WASD / Mouse / Space | 자세를 유지한 채 이동·시점 변경·점프 |
| LMB | 다음 자세 |
| RMB | 카메라 시점 변경 |
| R | 자세 해제 |

자세 상태에서는 공격·방어·달리기·스킬·상호작용·감정표현을 쓸 수 없다.

## 4. 상자 상호작용

| 입력 | 처리 |
|---|---|
| `F` | 상자 열기(일반 모드) / 닫기(상자 모드) |
| `Space` 길게 | 자기 상자에 등록 또는 상대 상자에서 탈취 |
| `Space` 떼기 | 진행 중인 동작 취소 |

바닥 유물은 `IMC_Interaction`에 들어가지 않고, `IMC_Gameplay`에서 `F`를 길게 누른다. 상자를 여는 `F`가 닫기 `F`로 바로 인식되지 않도록 Context 전환 시 눌린 키를 무시한다.

## 5. 스킬 대상 선택

| 키 | 처리 |
|---|---|
| LMB | 서버가 유효 상대 중 무작위 1명 선택 |
| RMB | 서버가 등록 유물이 가장 많은 상대 선택 |
| Cancel | 사용 취소, 쿨다운 미소모, 기본 입력 복귀 |

1번(투명화)은 대상 선택 없이 바로 요청한다. 클라이언트는 대상 Actor를 보내지 않고 선택 정책만 보낸다.

## 6. 감정표현

`T`로 휠을 열어 Dance, Laugh, Cheer, FakeDeath 등을 고른다. 새 감정표현을 추가할 때 입력 Action을 늘리지 않고 휠 항목 데이터만 늘린다. 감정표현 유지 퀘스트를 위해 누르고 있는 동안 지속되는 `IA_Emote`(Started → 시작, Completed·Canceled → 종료)를 둔다. 가짜 죽음과 실제 사망은 서로 다른 상태다.

## 7. 상태 복귀 규칙

- 상자·스킬·자세·감정표현이 끝나면 일반 입력으로 돌아오되 중복 등록하지 않는다.
- 사망 시 일반 입력을 내려놓고 관전 입력만 받는다. 리스폰 후 `PawnClientRestart`에서 Mapping을 다시 등록한다.
- 일반 조작으로 돌아오는 지점(경기 진입, 리스폰, 메뉴 닫기)마다 `GameOnly` 입력 모드와 마우스 캡처를 다시 맞춘다.

관련 문서: [3단계 이동·자세](Stages/03_MovementPose.md) · [8단계 스킬](Stages/08_Skills.md) · [입력과 이동 구현](../Technical/05_Input_Movement.md)
