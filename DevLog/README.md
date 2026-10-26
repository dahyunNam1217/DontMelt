# DevLog — 개발 일지

0단계 기획부터 11.5단계 젤리 애니메이션까지, 작업 순서와 그때의 판단을 단계별로 기록했다. 완성된 구조는 [Technical](../Docs/Technical/README.md), 문제의 원인과 해결은 [Debugging](../Docs/Debugging/README.md)에 따로 정리했다.

## 단계별 일지

| 단계 | 일지 | 작업 수 | 주요 내용 |
|---:|---|---:|---|
| 0 | [기획과 전체 책임 설계](step00_Planning.md) | 4 | 게임 규칙 확정, 단계별 개발 순서, Gameplay Framework 책임 분리 |
| 1 | [C++ 프로젝트 골격](step01_Cpp_Skeleton.md) | 2 | 클래스 골격, Tick 비활성화, Multiplayer PIE 검증 |
| 2 | [로비·경기 Framework](step02_Match_Framework.md) | 7 | Match Phase, Ready, 스폰 완료 조건, 테스트 콘솔 명령, Delegate 중복 등록 |
| 3 | [이동·카메라·자세](step03_Movement_Pose.md) | 8 | Enhanced Input, Sprint 예측, 시점별 회전 정책, Pose Context |
| 4 | [유물·상호작용](step04_Relic_Interaction.md) | 13 | 상태 전이, Request ID, Reservation, 완료 직전 재검증, 드롭·등록·탈취 |
| 5 | [전투·사망·리스폰](step05_Combat_Death_Respawn.md) | 10 | 서버 타격 판정, 방어, 오인 공격, 사망 파이프라인, 30초 리스폰 |
| 6 | [NPC 인구 관리와 AI](step06_NPC_AI.md) | 9 | 서버 전용 타이머 AI, 오인 공격 집단 반응, Panic 전파 |
| 7 | [퀘스트·랜덤 이벤트](step07_Quest_RandomEvent.md) | 12 | 두 Director 분리, 행동 이벤트 연결, 실패 사이렌, Overtime 정리 |
| 8 | [스킬](step08_Skills.md) | 15 | PlayerState 소유 쿨다운, 서버 대상 선택, 투명화·발소리·시야 교란 |
| 9 | [사망 후 관전](step09_Spectator.md) | 10 | 30초 관전 4단계, 서버 성공 후 Handoff, 정보 제한 |
| 10 | [UI·결과·설정](step10_UI_Result.md) | 19 | GameRoot 레이어, 결과표·ACK, 설정창, 접속 실패 안내 |
| 11 | [Steam 세션·음성·젤리 애니메이션](step11_Steam_Voice_Jelly.md) | 28 | Steam 참가 단계별 문제, 음성 정책, 젤리 관성 P-0~P-10 |

## 먼저 볼 기록

| 기록 | 내용 |
|---|---|
| [2단계 #11 GameState Replication 검증](step02_Match_Framework.md) | 서버 값만 보지 않고 각 PIE 인스턴스에서 `getall`로 복제 결과 확인 |
| [2단계 #12 Delegate 중복 호출](step02_Match_Framework.md) | Host 종료 1회당 콜백 1회를 기준으로 재접속 반복 테스트 |
| [4단계 #27 Reservation과 완료 직전 재검증](step04_Relic_Interaction.md) | 시작 조건과 완료 조건을 같은 것으로 보지 않는 구조 |
| [9단계 #88 서버 Respawn 성공 이후 Handoff](step09_Spectator.md) | 클라이언트 타이머가 아니라 서버 확정을 기준으로 화면 복귀 |
| [11단계 #113~115 Steam 검색·참가·주소](step11_Steam_Voice_Jelly.md) | BuildUniqueId 혼동, Presence 플래그, ConnectString 2단계 해석 |
| [11단계 #131 감쇠 스프링](step11_Steam_Voice_Jelly.md) | 1.8Hz·감쇠비 0.55 Semi-implicit Euler |

## 기술 학습 노트

구현하면서 정리한 Unreal Engine C++ 개념을 주제별로 묶었다. 개념마다 Don't Melt에서 어떻게 썼는지, 관련 소스와 문제 해결 문서를 함께 적었다.

| 주제 | 다루는 내용 |
|---|---|
| [네트워크·상태 동기화](Study/01_Network_State.md) | FSavedMove, RepNotify와 Multicast, Request ID, 종료 시각 기반 타이머, 조건부 복제, Match Phase, 결과 ACK, BaseSlot |
| [이동·전투·상호작용](Study/02_Movement_Combat_Interaction.md) | GetMaxSpeed, AnimNotify 서버 판정, 거리·시야 검증, Hold 세션, 예약과 Commit, Deferred Spawn, 스킬 대상 선택, Gameplay Tag |
| [UI·라이프사이클·안전성](Study/03_UI_Lifecycle_Safety.md) | Root Widget 레이어, 데이터 소스 재바인딩, 동적 Row, 관전 Pawn, SaveGame, GameInstanceSubsystem, TGuardValue, TWeakObjectPtr |
| [AI·이벤트](Study/04_AI_Event.md) | NPC 인구 관리, 타이머 상태 머신, MoveToLocation 완료 콜백, 행동 이벤트 라우팅, 시간형 퀘스트, 정전 복구 |
| [Steam·세션·음성](Study/05_Steam_Session_Voice.md) | Find·Join·Travel 구분, 접속 주소, 비동기 작업 수명, ServerTravel 실패 처리, Steam Voice·PTT, 근접 음성, 볼륨 설정 |
| [애니메이션·화면 효과](Study/06_Animation_ScreenEffect.md) | Bone 회전 보정, 감쇠 스프링과 Semi-implicit Euler, Ragdoll과 판정 분리, PostProcess MID |

[포트폴리오 첫 화면](../README.md)
