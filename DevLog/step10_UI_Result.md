# 10단계 — UI·결과·설정

## 91. UI 화면 목록과 레이어 분리

Match 중 필요한 HUD를 한 위젯에 모두 넣지 않고 GameRoot 아래 역할별 레이어로 나눴다. 메뉴·상자·관전·결과 등 입력을 빼앗는 UI가 겹칠 때 우선순위를 정했다.

**구현 및 판단**

- 상시 HUD/암전/상호작용/퀘스트·이벤트/관전·리스폰/결과 화면 레이어
- GameRoot는 Presenter, Gameplay 원본은 각 시스템에 유지
- UI에서 승리·사망 상태를 직접 확정하지 않음

**결과**

C++ 위젯은 `BindWidget`으로 WBP와 연결하고, 필수 바인딩 이름과 타입은 WBP 컴파일·Cook에서 검사된다.

## 92. GameplayHUD와 CombatHUD 구성

Match Phase와 남은 시간, 전투 관련 알림을 항상 표시하는 범위를 정리했다. 상호작용 화면이 열려도 서버 시간은 계속 흐른다.

**구현 및 판단**

- `WBP_DMGameplayHUD`에서 시간·상태·전투·상호작용 안내 연결
- `TXT_RemainingTime`은 Match 잔여 시간
- 표시값은 GameState/PlayerState를 읽어 계산

**결과**

남은 시간은 서버 종료 시각과의 차이로 계산해, 서버에 매초 묻지 않는다.

## 93. 상자와 유물 HUD 중복 제거

유물 운반 안내와 상자 등록·탈취 Modal의 텍스트가 겹치지 않도록 표시 책임을 줄였다.

**구현 및 판단**

- RelicHUD는 운반·Q 안내 위주
- InteractionPrompt에서 F 동작·Hold Progress 표시
- ChestPanel의 불필요한 Count/Action 텍스트 제거 방향

**결과**

진행 상태는 상자 패널 한 곳에만 표시해, 유물 HUD와 같은 값을 각자 갱신하지 않게 했다.

## 94. SkillSlot과 Targeting UI 정리

SkillSlot은 스킬명·설명·상태 TextBlock을 삭제하고 아이콘·입력키·쿨다운 표시를 중심으로 간소화했다.

**구현 및 판단**

- 스킬 대상 선택 중 Input Context 전환
- Slot은 SkillComponent 정보를 구독
- 이름·설명 제거 후 필수 BindWidget 변경 확인

**결과**

C++에서 지운 필드는 WBP에서도 지워야 쓰이지 않는 텍스트가 남지 않는다.

## 95. StatusHUD와 EventHUD 책임 조정

스턴·부활 보호·투명화 등 지속 상태를 StatusHUD로 모으고 RandomEventHUD는 현재 이벤트 이름·설명·진행 바로 한정했다.

**구현 및 판단**

- `TXT_InputReversed` 제거 방향
- 발소리 경고 중복 메시지 제거
- RandomEventHUD의 시간·State TextBlock 삭제

**결과**

C++에서 바꾸는 텍스트와 디자인 폰트의 글리프가 맞지 않으면 글자가 깨질 수 있어, 런타임 문자열을 따로 정리했다(107 참고).

## 96. Quest 실패 UI와 3D 사이렌

개인 퀘스트 성공 보상은 없애고 시간 초과 페널티를 3D 루프 사이렌으로 변경했다.

**구현 및 판단**

- QuestHUD는 진행도·마감·결과 표시
- Character 주변 AudioComponent가 사이렌 표현
- 사망·매치 종료 시 종료 경로 점검

**결과**

사이렌은 캐릭터마다 AudioComponent 하나를 켜고 끄는 구조로, 이후 복제 상태로 바꿨다([퀘스트 사이렌](../Docs/Debugging/Gameplay/02_QuestSiren_Relevancy.md)).

## 97. Relic Reveal과 Overtime Intro UI

정규시간 막바지 유물 위치 공개와 연장전 진입 메시지를 별도 HUD로 분리했다.

**구현 및 판단**

- 정규 종료 60초 전 3초 유물 Reveal 정책
- `WBP_DMRelicRevealHUD`, `WBP_DMOvertimeIntro` 구성
- 서버가 허용한 시간에만 공개 데이터를 만들고 종료 즉시 비움

**결과**

월드 좌표를 지도 좌표로 바꿀 때는 레벨 맵의 실제 Min/Max 범위를 기준으로 한다.

## 98. DeathCam·Spectator Countdown UI 연결

9단계에서 구현한 Local 관전 상태가 GameRoot의 카메라 레이어와 동기화되도록 연결했다.

**구현 및 판단**

- Corpse/Killer/FreeCamera/Fade 표시 위젯을 9단계 Context 기반으로 갱신
- Respawn Handoff 이후 위젯 해제
- Gameplay HUD 복귀

**결과**

기존 카운트다운 위젯을 다시 만들지 않고 GameRoot의 관전 레이어에서 그대로 쓴다.

## 99. MatchResult 결과표와 공동 순위

결과 화면에서 승리/패배, 플레이어 이름·등수·유물·킬 정보를 표시하도록 정리했다. 최종 승패와 등수는 서버가 확정한 상자 등록 유물 수를 기준으로 한다.

**구현 및 판단**

- 공동 순위 허용
- 등록 수와 누적 획득 통계는 다른 값
- `TXT_EndReason` 삭제하고 비정상 종료 사유는 `TXT_ResultDetail` 단독 표시

**결과**

10/18 코드 검수에서 동률 순위가 공란이 되고 유물 열이 등록 수를 쓰는 문제를 찾아, 등록 수 기준 공동 순위와 누적 획득 수 열로 나눴다([결과표 데이터 분리](../Docs/Refactoring/03_MatchResult_Data.md)).

## 100. Result ACK와 30초 자동 로비 복귀

결과를 받기 전에 Host가 Travel해서 Client의 결과 화면이 사라지는 상황을 막으려 했다. 서버 마감 시점부터 30초 후 전체 Lobby 복귀 정책을 도입했다.

**구현 및 판단**

- GameState에서 결과 복제·Client 캐시
- Client ACK 서버 전달
- 최대 대기와 Travel 경합 방지

**결과**

10/18 검수에서 ACK를 보내는 함수의 호출부가 없다는 것을 호출 0건 검색으로 찾았고, 결과 캐시 직후 ACK를 보내도록 연결했다.

**관련 문서**

- [경기 결과를 받기 전에 로비로 이동하던 문제와 ACK 설계](../Docs/Debugging/Network/07_MatchResult_Travel_Race.md)

## 101. Host·Client 재경기 버튼 정책

Host는 수동 Lobby 복귀와 전원 Ready 이후 Restart를 실행할 수 있고 Client는 Ready 상태만 변경하도록 구분했다.

**구현 및 판단**

- Host 이외의 Lobby Return 버튼이 서버 전체 Travel을 호출하지 않도록 제한
- Client 매치 종료 버튼은 타이틀 복귀
- Rematch Ready는 PlayerState RepNotify

**결과**

Host가 종료되면 Client는 GameInstance의 실패 안내를 거쳐 타이틀로 돌아간다(105 참고).

## 102. MainMenu·Lobby·IngameMenu 입력 경로

Menu와 Gameplay의 Input Mode가 충돌하지 않도록 Controller에서 모드 전환을 관리했다.

**구현 및 판단**

- Host/Join·Ready·Start·Esc 메뉴 구성
- Join 주소 입력 필드 `EDT_JoinAddress` 경로 확인
- 게임으로 돌아올 때 마우스 Capture/Focus 복원

**결과**

10/21에 클릭을 계속 눌러야 시점이 도는 문제가 나왔고, 11단계에서 일반 조작 진입 지점마다 입력 모드를 복구해 해결했다(125 참고).

## 103. Settings 공용 위젯과 로컬 저장

MainMenu, Lobby, IngameMenu에서 같은 Settings 위젯을 사용하도록 했다. 네트워크 상태와 관계없는 개인 설정은 로컬에 저장한다.

**구현 및 판단**

- Dirty 상태 및 탭/종료 시 변경 확인
- 저장·초기화·취소 분리
- 음량·화면·입력 관련 에셋 참조

**결과**

이후 변경 감지와 닫기 경로를 하나로 모으는 작업을 이어서 했다([설정창 변경값 유실](../Docs/Debugging/UI/01_Settings_Unsaved_Changes.md)).

## 104. 화면 설정 15초 복원 처리

잘못된 해상도·창 모드 적용 후 UI가 보이지 않는 경우를 막기 위해 영상 설정 확인 타이머를 구성했다.

**구현 및 판단**

- 15초 안에 유지 여부 확인
- 시간 초과나 취소 시 복원
- 저장 중 실패·맵 이동 상황 회귀 검사

**결과**

창 모드·해상도 변경은 에디터 PIE와 실제 창에서 동작이 달라, Standalone 실행에서 확인한다.

## 105. 접속 실패와 Host 종료 안내

Listen Server Host가 종료되면 일반 Client는 GameInstance에 원인을 기록하고 기본 화면에서 안내받도록 했다.

**구현 및 판단**

- NetworkFailure/TravelFailure 경로
- 중복 Delegate 바인딩 방지
- 안내용 `WBP_DMConnectionLost`
- 안전한 타이틀 복구

**결과**

Host 종료 안내와 접속 실패 안내의 문구를 나눠, 플레이어가 원인을 구분할 수 있게 했다.

## 106. Lobby Row와 참가자 동기화 이슈

Host/Client 두 창에서 참가자 인원이 `0 / ?`로 남고 목록이 비어 있는 증상을 확인했다.

**구현 및 판단**

- `DMLobbyWidget::TryBindGameState()`와 Lobby Mode/State/PS 연결 점검
- `WBP_DMLobbyPlayerRow` 동적 Row 타입 확인
- 수동 Join/PIE 자동 접속 경로 혼용 여부 점검

**결과**

처음에는 원인 후보가 많아, 데이터가 비었는지 UI만 비었는지부터 가르는 로그를 넣었다. 이후 GameState 바인딩 재시도가 약 10초 뒤 멈춘 다음 다시 시작되지 않는 것이 원인임을 찾아 고쳤다.

**관련 문서**

- [로비에 들어왔는데 플레이어 목록이 비어 있던 문제](../Docs/Debugging/Network/09_Lobby_PlayerList_Binding.md)

## 107. UI 한글·영문 문자열 전수 검수

C++에서 동적으로 바꾸는 Widget TextBlock이 디자인 폰트와 맞지 않아 깨지는 문제를 점검하기 시작했다.

**구현 및 판단**

- WBP별 바인딩 텍스트/접속 실패 문구/상태 메시지 정리
- 텍스트 자산의 한영 글리프 지원 확인
- Blueprint 지정 기본 문자열과 런타임 문자열 구분

**결과**

한글 폰트와 영문 폰트를 나눠 사용하는 디자인에서는 C++ 문구 수정도 UI 변경으로 취급한다.

## 108. Match 종료 로직과 결과 캐시 재검토

정규시간·연장전 만료 종료 코드와 결과 복제 흐름을 검수했다.

**구현 및 판단**

- 결과 생성·PlayerResultRow·승패 표기 비교
- 결과 확정 뒤 GameInstance 캐시
- Winner/Draw/비정상 종료 경계 점검

**결과**

여기서 찾은 Result ACK 호출 누락, 공동 순위, 로비 데이터 문제는 이후 각각 수정했다(99, 100, 106 참고).

## 109. 10단계 UI 변경사항 통합 검수

GameRoot, SkillSlot, StatusHUD, RandomEventHUD, Settings, Result 등 변경된 C++ 위젯 구조를 통합했다.

**구현 및 판단**

- 삭제된 BindWidget 참조 검색
- Blueprint 변수명/타입 검토
- 네트워크 PIE 2~4인 통합 테스트 계획

**결과**

C++에서 지운 바인딩을 WBP에서도 정리하고, 2~4인 PIE 통합 테스트 순서를 정했다.
