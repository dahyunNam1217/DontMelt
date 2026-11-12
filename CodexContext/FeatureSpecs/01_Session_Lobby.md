# 01. Steam 세션·로비

## 목적과 경계

2~4인이 Steam Listen Server 로비에 들어와 전원 Ready 후 Host가 시작한다. 검색→Join→실제 연결→로비 로딩을 서로 다른 성공 단계로 판정. Host migration과 경기 도중 Join은 지원하지 않는다.

## 클래스 책임

| 클래스 | 책임 |
|---|---|
| `UDMOnlineSessionSubsystem` | Create/Find/Join/Destroy, 로비 광고 갱신, 단일 비동기 작업 제어와 타임아웃 |
| `ADMFrontendPlayerController` | 메뉴 요청, 참가 후 실패/다음 작업 전 잔여 세션 정리 |
| `ADMLobbyGameMode` | Host 검증, Ready·인원·맵 검증, Travel 잠금·해제 |
| `ADMLobbyGameState`/`ADMLobbyPlayerState` | 인원·Host·Ready 등 공개 정보 |
| `ADMLobbyPlayerController` | 로컬 로비 UI, 서버 Ready/Start RPC |
| `ADMMatchGameMode` | 경기 맵 PreLogin/승인된 참가자 및 시작 인원 검사 |

## 확정 동작

1. 생성: `bIsDedicated=false`, Presence·Lobby 광고, 연결 제한 2~4인, 프로젝트/상태/빌드 구분 키 기록.
2. 검색: `SEARCH_LOBBIES` 및 프로젝트/상태 키를 서버 필터링하고, 반환 목록을 `IsCompatibleOpenLobby`에서 다시 검사.
3. Join: Presence·Lobby 설정 일치, NamedSession 우선 ConnectString 확보, 실패 시 원래 검색결과로 재시도.
4. `CurrentOperation`으로 작업 중첩 거부. 작업별 제한 **30초**(GameInstance 타이머); 늦은 Callback은 Delegate 해제/무효화.
5. Ready·Host·최대 인원·맵 상태 확인 후 `bTravelCommitted` 잠금 → 로비 '시작 중' 광고 → `ServerTravel ?listen?ExpectedPlayers=N`.
6. 경기 맵의 `PreLogin`에서 승인 참가자, 중복 입장, 시작 인원 고정 검사. 승인 인원이 **90초** 내 도착하지 않으면 `Aborted`.
7. 경기 이후 로비 복귀 직후에는 복귀 참가자를 기다리며 즉시 재시작 차단. 문서에는 **30초**까지 대기 후 도착 인원 기준 재개로 기록.

## 실패·수명

- 세션 검색 성공 ≠ Join 성공 ≠ IP/Steam 연결 성공. 각 실패 단계마다 UI에 일반 사유 노출 및 자원 정리.
- Host/Join 직전, 실패 복구 후, 메인 메뉴 진입 시 stale 세션 정리. Disconnect를 경기 규칙 위반 판정과 혼동하지 않음.
- 로비 상태 광고 실패나 Travel 실패는 잠금과 광고 상태를 모두 되돌려 새 시도가 가능해야 함.
- 의도적인 Leave와 NetworkFailure는 중복 팝업·중복 Travel을 만들지 않도록 구분.

## 확인할 구현 후보

`Source/DontMelt/Private/Online/DMOnlineSessionSubsystem.cpp`의 `CreateLobbySession`, `FindLobbySessions`, `JoinLobbySession`, `HandleJoinComplete`, `StartOperationTimeout`; `.../Framework/Lobby/DMLobbyGameMode.cpp`의 `CanStartMatch`, `CommitMatchTravel`, `ReleaseMatchStartLock`.

## 완료 기준

- Host 생성/Client 검색·Join, 2~4인 Ready·시작, Host 아닌 사용자 시작 거부.
- 검색 결과가 여러 게임 로비와 섞여도 필터 적용; 공유 AppID를 쓰는 테스트에서 프로젝트 구분.
- 검색/Join 타임아웃, 실패한 Join 후 재참가, `ServerTravel` 실패와 복귀 후 재시작 가능.
- 90초 누락자 중단, 임의 새 클라이언트의 경기 PreLogin 거부, 클라이언트 일반 로비 탈퇴 후 잔여 세션 정리.

[설계 근거](../../Docs/Technical/02_Steam_Session_Lobby.md) · [로비 단계](../../Docs/Design/Stages/02_MatchFramework.md) · [장애 기록](../../Docs/Debugging/Network/01_Steam_Join_Stages.md)
