# 02 — 로비·경기 Framework

## 1. 범위

로비에서 준비 상태를 모으고 서버가 경기 시작을 승인한다. 참가 인원을 확정해 경기 맵으로 이동한 뒤 BaseSlot과 PlayerStart를 맞춘다. Countdown → Playing → Finished 또는 연장전으로 이어지는 Phase 전환과 연결 종료 처리를 만들었다.

## 2. 로비

| 대상 | 역할 |
|---|---|
| `ADMLobbyGameMode` | 참가 인원 검증, Host 확정, Ready·Start 요청 검사, ServerTravel |
| `ADMLobbyPlayerState` | `bIsReady`, `bIsHost` 복제와 변경 알림 |
| `ADMLobbyPlayerController` | Ready·시작 요청 RPC, 로비 UI |
| `ADMLobbyGameState` | 공용 로비 상태 |

`ServerSetReady(bool)`와 `ServerRequestStartMatch()`는 Controller의 저빈도 요청이다. Host 여부와 Ready는 클라이언트가 직접 쓰지 않고 GameMode가 검증한다. 시작이 확정되면 중복 ServerTravel을 막는 Commit 잠금을 건다.

## 3. Match Phase

```text
Waiting → Countdown → Playing → Finished
                         └→ OvertimeIntro → Overtime → Finished
Waiting / Countdown / Playing / OvertimeIntro / Overtime → Aborted
```

Phase 전환은 `TryTransitionPhase()`를 거친다. 정상 종료는 `TryFinishMatch()`, 중단은 `TryAbortMatch()`가 맡는다. `Finished`나 `Aborted`에서 다시 진행 Phase로 돌아가지 않는다. GameState는 Phase와 `PhaseEndServerTime`을 복제하고, 남은 시간은 클라이언트가 서버 시간과의 차이로 구한다. 초 단위로 복제하지 않는다.

## 4. 참가자 확정과 스폰

로비가 `ExpectedPlayers`를 이동 URL 옵션으로 넘기고, Match GameMode가 실제 입장 수와 비교한다. BaseSlot은 0~3 중 서버가 중복 없이 배정한다. 시작 위치는 `DM_Base_0`~`DM_Base_3` 태그의 PlayerStart다.

`Alive`는 Pawn 생성, Possess, 유효한 PlayerState·Controller 연결을 모두 확인한 뒤 확정한다. 접속했다고 살아 있다고 보지 않는다. 시작 인원은 첫 스폰이 끝난 뒤 스냅샷으로 고정한다.

## 5. 실패 처리 규칙

- 정상 종료(`Finished`)와 비정상 중단(`Aborted`)은 다른 함수에서 처리한다. 승리 결과를 `Aborted`에 넣지 않고, 참가자 이탈을 승리로 보지 않는다.
- 해당 Phase에서 허용되지 않는 종료 사유는 서버가 거절한다. 종료 뒤에는 Phase 변경과 새 결과 작성을 잠근다.
- `UDMGameInstance`가 NetworkFailure와 PostLoadMap의 수명 경계를 맡고, 의도적 종료와 비정상 종료를 따로 다뤄 중복 복귀를 막는다.
- `LifeState`와 `BaseSlot`은 소유 클라이언트에만 보낸다.

## 6. 이후 변경

| 단계 | 변경 |
|---|---|
| 10단계 | 결과 복제, ResultId ACK, 30초 자동 로비 복귀 |
| 11단계 | Steam 세션과 Listen Server 연결 상태 분리, 경기 시작 시 로비 광고 상태 갱신 |
| 이후 | 승인 참가자 도착 대기 90초 기한, 로비 시작 잠금 실패 경로 정리, 복귀 로비 30초 기한 |

[Stages 목차](README.md)
