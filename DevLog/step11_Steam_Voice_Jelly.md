# 11단계 — Steam 세션·음성·젤리 애니메이션 (11.5단계 포함)

## 110. Steam OnlineSubsystem 적용 범위 정리

기존 IP 접속과 Steam Session 경로를 구분하고 Steam 온라인 로비를 위한 연결 구조를 정리했다. 포트폴리오용 2~4인 Listen Server라는 전제는 유지했다.

**구현 및 판단**

- Steam 세션 생성·검색·Join·Destroy·Update 경로 분리
- GameInstance와 LobbyGameMode의 책임 중복 검토
- 세션 생성과 원격 Client 접속을 서로 다른 단계로 다룸

**결과**

세션 객체 수명과 실제 NetworkDriver 연결 상태를 따로 로그로 확인하기로 했다.

## 111. Steam 플러그인·Build 모듈 충돌

`VoiceChat` 플러그인을 켠 상태에서 `VoiceChat is not a C++ module` 오류가 발생했다. 현재 UE5.5 환경에서는 해당 플러그인을 비활성화하자 빌드가 통과했다.

**구현 및 판단**

- 프로젝트 `.uproject` 플러그인 활성화 설정과 Build.cs 모듈 종속성 분리
- Steam OSS 음성 경로와 별도 VoiceChat 플러그인을 혼동하지 않음
- 변경 전후 빌드 로그로 직접 비교

**결과**

이 프로젝트는 EOS VoiceChat이 아니라 Steam `IOnlineVoice` 경로만 쓴다.

## 112. Steam 세션 생성과 Lobby Host 검수

Standalone Steam 실행에서 세션 생성과 Listen Server Lobby Host 성공 로그를 확인했다. 네트워크는 SteamNetDriver를 사용했다.

**구현 및 판단**

- Steam 초기화와 로컬 계정 유효성 점검
- Host Lobby 생성 후 NamedSession 확인
- 패키징 실행과 PIE 실행의 Steam 초기화 조건 구분

**결과**

호스트 쪽 Create와 SteamNetDriver 시작이 정상임을 확인한 뒤, 다른 계정의 검색과 참가를 다음 단계로 나눠 봤다.

## 113. Steam 검색 BuildUniqueId 비교 버그

Session 검색 과정에서 엔진의 `BuildUniqueId`와 게임 전용 `DM_NETWORK_BUILD=1` 값을 직접 비교했던 문제가 발견됐다. 두 값은 동일한 식별자 체계가 아니었다.

**구현 및 판단**

- 원본 검색 결과의 BuildUniqueId와 프로젝트 사용자 정의 Build 정보를 분리
- `Raw=1, ProjectMatched=1, Compatible=1` 진단으로 검색 필터 확인
- Presence/Lobbies 플래그 별도 검사

**결과**

엔진 BuildUniqueId 검사는 OSS에 맡기고, 게임 코드는 프로젝트 키만 검사하도록 나눴다.

## 114. Steam Presence와 JoinResult 검토

검색 결과가 유효해 보여도 `Presence=1`과 `UseLobbies=0` 불일치에서 Join 실패가 발생했다. Join 결과 값 5가 남은 로그를 확인했다.

**구현 및 판단**

- 세션 생성·검색·Join 설정 일치 여부 검토
- `UseLobbiesIfAvailable` 정책 확인
- 실제 로비 광고와 네트워크 연결 결과 구분

**결과**

참가 직전에 검색 결과의 Presence·Lobby 플래그를 맞추자 `Result=5` 없이 다음 단계로 넘어갔다.

## 115. ResolvedConnectString Invalid session info

NamedSession/SearchResult가 유효해도 `GetResolvedConnectString()`에서 `Invalid session info`가 반복됐다. SteamP2P 주소 로그와 일반 HostIP 값을 혼동하지 않도록 확인했다.

**구현 및 판단**

- `SteamP2P: ...:7777` 및 SteamNetDriver 포트 7777 로그 검토
- `HostIP: INVALID`만으로 Steam P2P 실패 확정하지 않음
- Named/Search 경로 각각에서 URL 생성 결과 확인

**결과**

NamedSession에서 주소를 얻지 못하면 검색 원본으로 다시 해석하도록 했다. 이후 남은 접속 실패는 잔여 세션, 검색 필터, 작업 타임아웃, NetDriver 설정으로 나눠 이어서 해결했다.

**관련 문서**

- [Steam 로비 참가 실패를 단계별로 분리해 추적한 과정](../Docs/Debugging/Network/01_Steam_Join_Stages.md)

## 116. Match Travel과 Steam 세션 광고 전환

Lobby→Match 및 종료 후 Lobby 복귀에서 세션 공개 상태와 실제 참가자 연결을 함께 다루도록 했다.

**구현 및 판단**

- 승인된 참가자 명단·Travel 처리 구분
- Host만 전체 서버 Travel 제어
- Result 30초 복귀와 Session 갱신 충돌 점검

**결과**

경기 시작 때 로비 광고 상태를 "시작 중"으로 바꾸고, 실패하면 되돌린 뒤 잠금을 풀도록 했다([로비 시작 잠금](../Docs/Refactoring/02_Lobby_StartLock.md)).

## 117. NetworkFailure와 Session 정리 경계

정상 퇴장, Host 종료, 접속 실패, Match 종료에서 Session 정리와 타이틀 복귀 경로가 섞이지 않도록 검토했다.

**구현 및 판단**

- GameInstance 종료 사유 캐시·안내
- DestroySession 콜백과 맵 이동 책임 분리
- Delegate 중복 등록 예방

**결과**

Host Migration은 설계 범위에 없고 Client는 안전한 복귀 화면을 사용한다.

## 118. Steam 로컬 Talker 초기화

Standalone 로그에서 Steam 로컬 Talker 등록과 Voice 정책 전환이 동작한 것을 확인했다.

**구현 및 판단**

- `IOnlineVoice` 로컬 초기화
- Owner 상태에 따른 송신 정책 전환 코드

**결과**

로컬 등록과 원격 수신은 다른 단계라, 원격 청취 정책은 121에서 따로 구현했다.

## 119. PTT 토글과 음성감지 모드 정리

처음의 V Hold 입력 방식을 V Toggle 방식으로 변경했다. PTT 모드에서는 V로 송신을 켜고 끄고, 음성 감지 모드에서는 M으로 마이크 음소거를 토글한다.

**구현 및 판단**

- `IA_PushToTalk` V, `IA_ToggleMicrophoneMute` M
- Lobby/Match Controller에서 Started 처리
- VoiceMode와 마이크 상태는 로컬 설정으로 보존

**결과**

엔진이 접속 초기화 때 송신을 강제로 끄는 경로는 이후 Controller에서 막았다([PTT 충돌](../Docs/Debugging/Network/11_SteamVoice_PTT_Conflict.md)).

## 120. Voice 설정 저장·UI 상태 분리

PTT/Voice Activation 선택값과 마이크 상태를 SaveGame으로 저장·복원하는 경로를 정리했다. UI는 Delegate를 통해 최신 상태를 표시한다.

**구현 및 판단**

- MainMenu·Lobby·Ingame Settings에서 공용 옵션
- 마이크/음소거 아이콘은 상태 구독

**결과**

음성 상태 위젯은 생존·관전과 관계없이 GameRoot에 항상 표시한다.

## 121. 생존자·사망자 음성 정책 경계

생존자는 근접 음성, 사망자는 전역 청취라는 음성 정책을 설계했다. 처음에는 모드 전환만 있고 수신 쪽 처리가 없어, 원격 플레이어마다 `UVOIPTalker`를 두는 청취 정책을 이어서 구현했다.

**구현 및 판단**

- AliveProximity/DeadGlobal 상태 구분
- PlayerState LifeState와 관전 경계
- 생존 중 위치를 모르는 원격 플레이어는 로컬 음소거

**결과**

300cm 안 최대 음량, 2,500cm 밖 무음의 근접 감쇠를 런타임에 만들고 값은 Config로 열어 두었다([음성 채팅 구현](../Docs/Technical/14_Voice_Chat.md)).

## 122. Steam Stage 11 통합 상태 점검

Steam 호스팅과 로컬 Talker가 정상인 것을 확인하고, 검색·참가·원격 음성·경기 왕복을 각각 두 계정으로 확인할 순서를 정했다.

**구현 및 판단**

- Host Session/SteamNetDriver 로그 성공
- 참가와 원격 음성은 서로 다른 PC·계정으로 테스트
- PTT·UI·Travel은 코드 수정과 에디터·패키지 작업을 나눠 진행

**결과**

두 계정 테스트 항목은 [회귀 테스트 시나리오](../Docs/Verification/03_Regression_Scenarios.md)에 정리했다.

## 123. NPC ×15 변경 검토와 40명 상한

6단계의 NPC 기본 배율 ×10을 ×15로 바꾸는 작업을 검토했다. 배율 제한과 NPC 최대 40명 상한이 함께 영향을 받는다.

**구현 및 판단**

- StartingPlayerCountSnapshot을 기준으로 수량 계산
- GameMode/DataAsset/레벨 타겟 포인트까지 변경 영향
- Director 허용 범위를 1~60으로 확장

**결과**

기본 배율 15, 최대 60명으로 바꿨다. 레벨에는 4인 기준 60개 이상의 스폰 지점이 필요하고, 성능은 60명 기준으로 다시 측정한다.

## 124. Lobby·Result 동적 Row 불일치

Lobby와 MatchResult에서 VerticalBox에 동적으로 추가되는 PlayerRow가 화면 설계와 맞지 않았다.

**구현 및 판단**

- Row WidgetClass 설정·BindWidget 타입·다중 Row 생성 확인
- 헤더와 Row 너비 정렬
- Host/Client 모두 자신의 데이터를 보게 되는지 확인

**결과**

Row 표시 문제는 Row 클래스 지정과 헤더 정렬 문제로 나눠 봤고, 목록이 아예 비는 문제는 GameState 바인딩 재시도 수정으로 해결했다.

## 125. 게임 시작 후 마우스 Capture 문제

Gameplay 진입 후 마우스 버튼을 누른 상태에서만 카메라가 회전했다. 로비의 `GameAndUI` 입력 모드가 경기 시작 후에도 남아 있는 것이 원인이었다.

**구현 및 판단**

- PlayerController에서 GameOnly 및 Mouse Capture 재설정 경로 확인
- UI Focus와 Mouse Lock 정책 검토
- 경기 진입·리스폰·메뉴 닫기마다 `RestoreGameplayMouseCapture()` 호출

**결과**

상자·대상 선택·관전·결과처럼 UI가 마우스를 써야 하는 상태에서는 복구하지 않도록 가드를 함께 넣었다.

**관련 문서**

- [경기 시작 후 마우스 버튼을 누르고 있어야 시점이 돌던 문제](../Docs/Debugging/UI/02_InputMode_Restore.md)

## 126. Blackout LightComponent 빌드 오류

10/21 빌드에서 `DMBlackoutController.cpp`의 C2039가 발생했다. `ULightComponentBase`에 호출한 `GetMobility()` 멤버가 없어 컴파일이 중단됐다.

**구현 및 판단**

- UE5.5 컴파일 오류는 Engine API 타입 불일치
- `DMQuestHUDWidget.cpp`는 해당 빌드에서 통과
- MSB3073 코드 6은 선행 컴파일 실패에 따른 결과

**결과**

MSB3073은 앞선 컴파일 실패의 결과로 나온 메시지였다. 빌드 로그는 맨 앞의 오류부터 읽는다.

## 127. 젤리 물리 P-0 에셋 구조 확인

기존 메시·Skeleton·Physics Asset·Animation Blueprint를 확인한 뒤 새 Physics Actor를 늘리지 않는 방향을 선택했다. Player와 NPC가 같은 시각 처리 경로를 사용한다.

**구현 및 판단**

- `riggedcapsuleman`과 `ABP_DMJelly`
- 기존 `SM_Locomotion→DefaultSlot→Pose Blend` 유지
- 이동·판정은 CharacterMovement가 계속 담당

**결과**

전신 물리 시뮬레이션 대신, 기존 애니메이션 위에 절차적 본 회전을 더하는 방식을 택했다.

## 128. 젤리 물리 P-1 성능 측정 계획

NPC 20~40개와 Player 최대 4명 상황에서 추가 애니메이션 비용을 측정할 조건을 정했다.

**구현 및 판단**

- 기본 44캐릭터 시나리오 설정
- CPU/GPU·GameThread·AnimThread 비용 측정 계획

**결과**

측정 조건은 [프로파일링 계획](../Docs/Performance/03_Profiling_Plan.md)으로 이어졌다.


## 129. 젤리 물리 P-2 공용 관성 데이터

`UDMJellyAnimInstance`에 이동 속도와 방향 변화 기반의 몸통 반응 데이터를 추가했다. 새로운 Gameplay 상태를 만들지 않고 CharacterMovement 결과를 읽도록 했다.

**구현 및 판단**

- 공통 플레이어/NPC AnimInstance
- Spine/Ribcage 회전값 계산
- 기존 CharacterState는 실제 행동 제한 판단에만 사용

**결과**

계산 결과는 AnimGraph에 넘기는 시각 파라미터로만 쓴다.

## 130. 젤리 물리 P-3 Idle·Walk·Sprint

정지·보행·달리기마다 몸통 흔들림과 기울기를 다르게 적용했다. 보행 위상은 원격 복제하지 않고 각 클라이언트에서 계산한다.

**구현 및 판단**

- Idle 좌우 ±2°, 전후 ±1° 초기 목표
- Walk 최대 5°/Sprint 최대 12° 전방 기울기
- 움직임에 따른 좌우 흔들림 별도 계산

**결과**

수치는 초기값으로 두고 DataAsset에서 조정할 수 있게 했다.

## 131. 젤리 물리 P-4 급정지·급회전 스프링

방향 전환 순간 몸통이 관성으로 반대 방향으로 기울었다가 되돌아오는 감쇠 스프링을 구성했다.

**구현 및 판단**

- 자연주파수 1.8Hz·감쇠비 0.55 초기값
- Semi-implicit Euler 적분
- 회전각 제한으로 과도한 자세 뒤집힘 방지

**결과**

스프링 회전 데이터는 Gameplay 판정이나 서버 복제 대상이 아니다.

## 132. 젤리 물리 P-5 점프·착지 압축

점프 상승·하강 상태에 몸통 반응을 추가하고 착지 충격을 Ribcage Translation으로 표현했다. 실제 Capsule 위치는 이동시키지 않는다.

**구현 및 판단**

- 상승 -5°·하강 +3° 목표
- 착지 수직 속도 기준 충격 감지
- `JellyLandingRibcageOffset`은 Ribcage 단독 적용

**결과**

작업 중 Spine1·Spine2에도 착지 Translation을 연결해 몸통이 과하게 눌리는 구성이 있었고, Ribcage에만 연결하도록 바로잡았다.

## 133. 젤리 물리 P-6 머리·양팔 지연 반응

몸통 회전에 대해 머리와 양팔이 늦게 따라오는 느낌을 별도 스프링으로 만들었다. 추가 Physics Body나 AnimDynamics는 사용하지 않는다.

**구현 및 판단**

- 몸통 3개+머리/팔 4개 총 7개 Spring
- Head·Upperarm·Forearm 회전 제한
- Forearm은 상완 반응을 일부 전달

**결과**

좌우 팔은 Bone Space의 회전 부호가 다를 수 있어 좌우 회전값을 따로 둔다.

## 134. 젤리 물리 P-7 전투·스턴·사망 통합

공격·방어·스턴 중에는 젤리 연출 강도를 낮추고 Montage 또는 사망 상태에서 즉시 꺼지도록 규칙을 만들었다. 실제 Ragdoll은 사망에만 사용한다.

**구현 및 판단**

- Attack/Defense/Stun 별 Alpha 감쇠
- Montage 재생 시 Alpha 우선 0
- Death 시 Spring 초기화

**결과**

사망 Ragdoll로 넘어갈 때 젤리 가중치를 0으로 만들어 두 효과가 겹치지 않게 했다.

## 135. 젤리 물리 P-8 원격 보정과 스턴 표시

큰 위치 보정이 발생하면 이전 프레임의 관성 데이터로 본이 심하게 꺾이지 않도록 스프링 상태를 초기화한다. 스턴은 관찰자에게 필요한 표시값만 복제하도록 했다.

**구현 및 판단**

- 위치 불연속 또는 0.25초 초과 갱신 간격에서 Reset
- 실제 StatusTags는 OwnerOnly 유지
- 별도 `bStunPresentationActive`만 복제

**결과**

같은 패턴을 이후 투명화 표현에도 적용했다([투명화 Relevancy](../Docs/Debugging/Gameplay/01_Invisibility_Relevancy.md)).

## 136. 젤리 물리 P-9 거리별 최적화 검토

최대 44개 캐릭터에서 거리별 애니메이션 갱신 빈도를 낮추는 방법을 검토했다.

**구현 및 판단**

- 가까운 NPC와 원거리 NPC 표현 차등 고려
- AnimBP Update·Bones 처리 비용 측정 계획
- 화면 밖 포즈 갱신 중단은 발소리 노티파이를 멈추게 해 제외

**결과**

측정 전에는 적용하지 않고, NPC 전용 Update Rate Optimization을 첫 후보로 정했다([NPC 성능 분석](../Docs/Performance/01_NPC_Performance_Analysis.md)).

## 137. 젤리 물리 P-10 AnimGraph·네트워크 검수

AnimGraph에 8개 Transform Bone을 배치하는 구조와 사망·스턴·이동 사이의 제어 규칙을 다시 확인했다.

**구현 및 판단**

- `Spine1`, `Spine2`, `Ribcage`, `Head`, 양쪽 `Upperarm/Forearm`
- Component/Local Space 변환 노드
- Ribcage에만 Landing Translation

**결과**

AnimGraph 연결 규칙은 [11.5단계 설계](../Docs/Design/Stages/11_5_JellyAnimationAndRagdoll.md)에 정리했다.
