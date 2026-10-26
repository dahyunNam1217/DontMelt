# Debugging — 문제 추적과 해결

멀티플레이 동기화, Steam 세션과 맵 이동, 상태 수명주기, UI 입력, 빌드 환경에서 만난 문제를 정리했다. 모든 문서는 같은 순서로 쓴다.

> **문제 요약 → 현상 → 원인 추적 → 해결(검토한 안과 채택 이유) → 검증 방법 → 정리**

## 먼저 볼 문서

| 문서 | 보여 주는 역량 |
|---|---|
| [Steam 로비 참가 실패를 단계별로 분리](Network/01_Steam_Join_Stages.md) | 하나의 증상을 OSS 초기화·검색·참가·주소 해석·NetDriver 단계로 나눠 원인 5개를 각각 해결 |
| [참가 실패 뒤 남은 Steam 세션](Network/02_Steam_Stale_Session.md) | 증상(방 생성 실패)의 원인을 앞선 작업(참가 실패)에서 찾고, 불변 조건 하나로 모든 실패 경로를 정리 |
| [결과 수신 전 로비 이동과 ACK 설계](Network/07_MatchResult_Travel_Race.md) | 서로 다른 Actor의 복제·RPC 순서 문제, 타임아웃 폴백에 가려진 호출 누락을 호출 0건 검색으로 발견 |
| [서버 이동 중 연결 끊김 오탐](Network/05_ServerTravel_ConnectionLost.md) | 엔진 `TickWorldTravel` 흐름을 추적해 `TravelURL`/`PendingNetGame`으로 정상 이동을 판별 |
| [투명화 Relevancy 동기화 누락](Gameplay/01_Invisibility_Relevancy.md) | Multicast(사건)와 복제 변수(상태)의 차이, 늦게 Relevant해진 관찰자 처리 |
| [상호작용 LOS와 1차 수정안 폐기](Gameplay/03_Interaction_LOS_Pawn.md) | 채널 트레이스와 오브젝트 타입 트레이스의 규칙 차이로 생기는 회귀를 반영 전에 발견 |
| [전멸 승리 결과표 순위 규칙](Gameplay/09_EliminationVictory_Ranking.md) | 규칙 충돌을 선택지로 정리해 결정받고, 순수 함수 + Automation Test로 반영 |

## Network — Steam 세션·맵 이동·결과 동기화

| 문서 | 핵심 |
|---|---|
| [01. Steam 로비 참가 실패를 단계별로 분리](Network/01_Steam_Join_Stages.md) | PIE/Standalone, BuildUniqueId, Presence/Lobby 플래그, ConnectString, NetDriver |
| [02. 참가 실패 뒤 남은 Steam 세션](Network/02_Steam_Stale_Session.md) | 실패가 남긴 상태, 불변 조건 기반 정리 |
| [03. 공유 AppID 480 검색 필터](Network/03_Steam_Lobby_Search_Filter.md) | 서버 쪽 필터와 클라이언트 필터 |
| [04. 세션 비동기 작업 타임아웃](Network/04_Steam_Session_Timeout.md) | 오지 않는 콜백, 상태 소유자 안의 타임아웃 |
| [05. ServerTravel 중 연결 끊김 오탐](Network/05_ServerTravel_ConnectionLost.md) | non-seamless travel, `FWorldContext` |
| [06. PendingNetGame 실패 처리](Network/06_PendingNetGame_Failure.md) | World 없는 실패 이벤트, PIE 인스턴스 격리 |
| [07. 결과 수신 전 로비 이동과 ACK](Network/07_MatchResult_Travel_Race.md) | ResultId, 캐시 후 ACK, 폴백에 가려진 결함 |
| [08. ResultId 검사 회귀](Network/08_MatchResult_ResultId_Regression.md) | 필수 필드를 만드는 모든 경로 확인 |
| [09. 로비 플레이어 목록 비어 있음](Network/09_Lobby_PlayerList_Binding.md) | 재시도 로직의 재개 조건, 주석과 동작의 불일치 |
| [10. 경기 Waiting 영구 정지](Network/10_Match_Waiting_Timeout.md) | 끝나지 않는 대기, 기존 종료 경로 재사용 |
| [11. 엔진 PTT 처리와 음성 송신 충돌](Network/11_SteamVoice_PTT_Conflict.md) | 엔진이 대신 부르는 RPC, 상태 캐시의 전제 |

## Gameplay — 상태 동기화·상호작용·규칙

| 문서 | 핵심 |
|---|---|
| [01. 투명화 Relevancy](Gameplay/01_Invisibility_Relevancy.md) | RPC는 사건, 복제 변수는 상태 |
| [02. 퀘스트 사이렌 Relevancy](Gameplay/02_QuestSiren_Relevancy.md) | 지속 효과를 상태 하나로 복제 |
| [03. 상호작용 LOS와 Pawn 차단](Gameplay/03_Interaction_LOS_Pawn.md) | 1차안 폐기, 판정 의미 유지 |
| [04. 연장전 전환 시 등록 유물 Drop](Gameplay/04_Interaction_PhaseChange.md) | 실패와 취소의 구분 |
| [05. 벽 앞 드롭 유물 소실](Gameplay/05_Relic_Drop_Wall.md) | 배치 검증과 상호작용 검증의 기준 일치 |
| [06. 쿨다운 서버 시간 추정 오차](Gameplay/06_RemoteCooldown_Tolerance.md) | 클라이언트 예측, 범위를 좁힌 보정 |
| [07. 리스폰 실패 후 고립](Gameplay/07_Respawn_Failure_Retry.md) | 실패 경로의 복구 책임 |
| [08. 리스폰 Handoff 경합](Gameplay/08_Respawn_Handoff.md) | 엔진 콜백의 의미, 서버 확정 후 통지 |
| [09. 전멸 승리 결과표 순위](Gameplay/09_EliminationVictory_Ranking.md) | 기획 결정, 기본값 매개변수, Automation Test |
| [10. 정규 시간 단독 1위 승리 누락](Gameplay/10_RegularTime_Winner.md) | 설계 문서와 코드 대조 |
| [11. 감정표현 퀘스트 입력 경로 누락](Gameplay/11_Quest_Emote_Input.md) | 서버 이벤트까지 입력 경로 역추적 |
| [12. NPC 스폰 지점 편향](Gameplay/12_NPC_Spawn_Shuffle.md) | 순회 순서, Fisher-Yates, 정보 노출 |

## UI

| 문서 | 핵심 |
|---|---|
| [01. 설정창 변경값 유실](UI/01_Settings_Unsaved_Changes.md) | 요청과 승인 분리, 기준값 비교 |
| [02. 경기 시작 후 마우스 캡처](UI/02_InputMode_Restore.md) | 증상의 모양으로 원인 계층 찾기, 복구 가드 |

## Build

| 문서 | 핵심 |
|---|---|
| [01. 한글 경로 UBT 빌드 실패](Build/01_UBT_Git_Korean_Path.md) | Adaptive Non-Unity, `git status`, `core.quotepath` |
| [02. 빌드·컴파일·Cook 오류 모음](Build/02_Compile_Link_Errors.md) | UHT, C3668, LNK2019/1104, BindWidget Cook 실패 |

[Docs 목차](../README.md)
