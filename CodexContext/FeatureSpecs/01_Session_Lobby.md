# 01. Steam 세션·로비

## P3-9 최신 기준 — 2026-11-17

기준 HEAD: `001a08241ecd01dfa1ef565f11c7791eaf73eaeb`. [최신 구현 대조와 검증 한계](../ImplementationStatus.md#p3-9--최신-아키텍처-결정-및-구현-대조-2026-11-17)를 함께 읽는다. 아래 최신 절과 충돌하는 P2/기존 본문은 당시 설계·조사 이력으로 보존하며 현재 완료 근거로 사용하지 않는다.

- `Public/Framework/Match/DMMatchGameMode.h::StartingRosterTimeoutSeconds` 기본값은 **90.0f**다. 서버 `StartPlay`에서 Waiting·종료 사유 None·미확정 명단·비활성 Timer 조건으로 설정한다. 클라이언트별 로딩 시작 시각 기준이 아니다.
- `IsStartingRosterComplete`는 기대 인원, 유효 PlayerState·고유 BaseSlot·Alive·Pawn/Controller/PlayerState 양방향 연결을 검사한다. `HandleStartingNewPlayer_Implementation`도 Spawn/Possess 및 PlayerState 연결을 확인한다.
- `HandleStartingRosterTimeout`은 단발 Timer를 정리하고 ArrivalOrRosterIncomplete / ParticipantPreparationFailed / RequiredInitializationIncomplete를 로그로 구분한 뒤 기존 **InternalError** 중단 경로를 호출한다. 미도착을 이탈로 확정하지 않는다.
- `TryFinalizeStartingRoster`는 리스폰 지점, NPC·유물·퀘스트·이벤트 초기화를 확인한 후 Timer 해제와 튜토리얼 게이트로 진행한다. 실패는 `FailStartingRosterFinalization`의 역순 정리 및 중단 경로다. 튜토리얼 기본300초는 별도다. BP 제한값·사용자 안내·실제 실패 재현은 미검증이다.
- `Private/Framework/Lobby/DMLobbyGameMode.cpp`의 `HandleStartMatchRequest`, `TryReopenReturningLobby`, `RestoreLobbyStatus`, `HandleReturningPlayersTimeout`에서 세션 복구 차단을 확인한다.
- `HandleLobbyStatusUpdated`는 유효 구독·진행 문맥·세션 유효성·복구 차단·Busy를 검사한다. 등록 전 기존 Delegate를 해제하고 요청 직후 Handle 유효성도 확인하여 동기 완료의 중복 처리를 방지하는 코드다.
- `StopLobbyStatusRecovery`는 소유 완료 Delegate와 복구/복귀 Timer를 정리하고 `bTravelCommitted=true`로 시작 잠금을 유지한다. `RetryOrReleaseLobbyStatusRestore`라는 기존 이름과 달리 상한 소진 시 잠금을 해제하지 않는다. 상한 상수는5이며 실패 카운트가5에 도달하면 중지한다. 반환 로비 재시도5초/일반 복구1초는 현재 구현값이다.
- `ReleaseMatchStartLock` 호출은 광고 복원 성공 경로에 있다. 종료 시 `EndPlay` 정리도 유지한다. Busy/응답 대기5초 확인 분기까지 모든 반복이 동일 실패 카운트로 제한된다고 단정하지 않는다.
- 기존 OnlineSessionSubsystem 상태 머신을 유지한다. 로그의 나가기·게임/Steam 재시작 안내는 확인했으나 UI 전달과 실제 Steam 정상/실패/지연/중복 콜백은 미검증이다.

## P2 승인 동기화 — 2026-11-13 (당시 기록; 최신 기준은 상단 P3-9)

이번 사용자 P2 요청의 명시 범위만 **설계 승인**으로 채택한다. 과거 승인으로 소급하지 않는다. 기준 HEAD는 `914173a`이며 구현 사실·설계 승인·에셋 적용·실행 검증은 별개다. 상세 근거와 미결정 사항은 [ImplementationStatus 10절](../ImplementationStatus.md#10-p2-승인-설계-및-현재-구현--2026-11-13)을 따른다.

**아래 기존 본문과의 관계:** 이 절 뒤의 기존 본문은 삭제하지 않고 이전 설계·조사 기록으로 보존한다. E1~E10과 충돌하는 값·완료/승인/미확인 표현은 이 절과 중앙10절이 대체한다. 특히90초·Walk300·NPC×10/40·NPC 발소리·3요소 이벤트 HUD·타임아웃 즉시 재요청은 최신 목표가 아니다. E1~E10 밖의 항목을 이번 승인으로 일괄 확정하지 않는다.

### E3 — 참가자 도착 대기 120초

**설계 승인:** 기본 도착 제한은120초다. 경기 맵 서버 GameMode가 승인 참가자의 준비와 시스템 초기화를 검증한다. PC 존재만으로 완료하지 않는다. 도착 제한과 튜토리얼 제한을 분리하고 무한 대기를 방지한다. 미도착·명시적 이탈·Spawn/Possess 실패는 가능한 범위에서 진단·사용자 안내를 구분한다.

**구현 확인:** `ADMMatchGameMode::StartPlay`가 아직 명단이 확정되지 않은 Waiting에서 타이머를 설정한다. 클라이언트 로딩 시작 시점 기준이 아니다. `HandleStartingNewPlayer_Implementation`은 부모 Spawn/Possess 후 Pawn/Controller를 확인한다. `IsStartingRosterComplete`는 기대 인원, 유효 PlayerState, 유효·고유 BaseSlot, Alive, Pawn·Controller·PlayerState 연결을 확인한다. `TryFinalizeStartingRoster`는 스폰 지점 및 NPC/유물/퀘스트/이벤트 초기화 후 도착 타이머를 해제하고 튜토리얼 게이트로 이동한다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E3 절을 함께 읽는다.

### E8 — Steam 안전 복구 상태 머신 유지

**설계 승인:** 기존 상태 머신을 유지하며 전면 재작성하지 않는다. Create/Find/Join/Destroy/Update별 비동기 결과를 구분하고 기본 요청30초를 유지한다. 타임아웃은 플랫폼 취소 완료가 아니다. 작업 중 중복 요청을 거절하고 원 요청 완료·필요한 잔여 세션 정리·안전한 재시도 조건 확인 후 다음 요청을 허용한다. 불확실하면 차단·재시작 안내한다. 영구 차단 후 무의미한 로비 재시도를 반복하지 않는다.

**구현 확인:** `UDMOnlineSessionSubsystem::HandleOperationTimeout`은 지연 완료를 기다리고 Find 취소를 시도한다. Create/Join 지연·실패는 `BeginRecoveryDestroy`로 잔여 세션을 처리한다. Destroy 완료는 성공 플래그와 NamedSession 잔존을 확인한다. Find는 취소/완료 결과에 따라 종료하며 Update 지연 완료는 실패 처리한다. `CompleteOperation`은 Delegate·Timer·상태를 정리한 뒤 원 작업 결과를 알린다. `FailClosedRecovery`의 bRecoveryBlocked와 `RejectBlockedOperation`은 불확실한 상태에서 새 요청을 막는다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E8 절을 함께 읽는다.

**검증 상태:** BP/DataAsset/Animation 실제 적용은 에셋 미검증, 빌드·PIE·Steam은 실행 미검증이다. 아래 완료 기준은 향후 검증 시나리오이며 통과 기록이 아니다. C++·WBP·Config 변경은 이번 문서 승인에 포함되지 않는다. [공통 게임 규칙](../GameplayRules.md) · [네트워크 정책](../NetworkPolicy.md)를 상호 참조한다.

---

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
