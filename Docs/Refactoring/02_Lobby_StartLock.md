# 로비 시작 잠금이 풀리지 않던 실패 경로 정리

| 항목 | 내용 |
|---|---|
| 문제 | 경기 시작이 실패하면 Ready 변경·재시작·신규 입장이 모두 막히고, 로비를 새로 만들어야 했다 |
| 원인 | 중복 시작을 막는 잠금(`bTravelCommitted`)과 복귀 로비 대기(`ExpectedReturningPlayers`)에 해제 경로가 빠진 실패 분기가 세 군데 있었다 |
| 해결 | 해제를 `ReleaseMatchStartLock()` 한 곳으로 모으고, 재시도 상한·Travel 실패 순서·복귀 대기 타임아웃을 정리했다 |
| 관련 코드 | `ADMLobbyGameMode::ReleaseMatchStartLock`, `RetryOrReleaseLobbyStatusRestore`, `CommitMatchTravel`, `HandleReturningPlayersTimeout` |

## 잠금 구조

로비의 경기 시작은 Steam 로비 광고 상태 갱신이라는 **비동기 작업**과 묶여 있다.

```text
[시작 요청] → bTravelCommitted = true
   → Steam UpdateLobbyMatchStarting(true) (비동기)
      ├ 성공 → 승인 명단 확정 → ServerTravel
      └ 실패 → ScheduleLobbyStatusRestore() → RestoreLobbyStatus()
                 → Steam UpdateLobbyMatchStarting(false) (비동기)
                    ├ 성공 → bTravelCommitted = false (잠금 해제)
                    └ 실패 → (해제 경로 없음)

bTravelCommitted == true이면 거부되는 요청:
  시작 요청 · Ready 변경 · 신규 입장(PreLogin) · 복구 재예약
```

잠금 플래그를 검토할 때 설정 경로보다 **해제 경로를 먼저 셌다.** 해제가 없는 분기가 세 개 나왔다.

## 문제 1 — 복구가 실패하면 잠금이 풀리지 않음

| 실패 지점 | 기존 처리 | 결과 |
|---|---|---|
| 세션이 Busy라 복구를 시작하지 못함 | 로그 후 return (`bRestoringLobbyStatus`도 true로 남음) | 영구 잠금 |
| 복구 요청이 시작되지 않음 | 로그 후 종료 | 영구 잠금 |
| 복구 콜백이 실패를 반환 | 로그 후 종료 | 영구 잠금 |

기존 코드에는 **"상태가 불확실하므로 시작 잠금을 유지한다"** 는 주석이 있었다. Steam에는 아직 "경기 시작 중"으로 광고되어 있을 수 있으니, 잠금 유지는 의도된 안전 장치였다. 다만 **재시도하거나 해제할 경로가 없어서** 안전 장치가 그대로 장애가 됐다.

그래서 의도를 버리지 않고 상한을 붙였다. 실패하자마자 풀지 않고 1초 간격으로 최대 5회 복구를 다시 시도한 뒤 해제한다.

```cpp
void ADMLobbyGameMode::RetryOrReleaseLobbyStatusRestore(const TCHAR* Reason)
{
    ++LobbyStatusRestoreAttemptCount;

    if (LobbyStatusRestoreAttemptCount < MaxLobbyStatusRestoreAttempts)   // 5
    {
        // 복구 상태(bRestoringLobbyStatus/bTravelCommitted)는 유지한 채 1초 후 다시 복구한다.
        bRestoringLobbyStatus = true;
        GetWorldTimerManager().SetTimer(LobbyStatusRestoreRetryTimer, this,
            &ADMLobbyGameMode::RestoreLobbyStatus, 1.0f, false);
        return;
    }

    // Steam 광고 상태는 '시작 중'으로 남을 수 있으나,
    // 호스트가 다시 시작하거나 로비를 나갈 수 있도록 로컬 잠금은 해제한다.
    ReleaseMatchStartLock(Reason);
}

// 단일 해제 지점
void ADMLobbyGameMode::ReleaseMatchStartLock(const TCHAR* Reason)
{
    GetWorldTimerManager().ClearTimer(LobbyStatusRestoreRetryTimer);
    bRestoringLobbyStatus = false;
    bTravelCommitted = false;
    PendingMatchPlayerCount = 0;
    LobbyStatusRestoreAttemptCount = 0;
}
```

## 문제 2 — Travel 실패 시 복구가 실행되지 않는 순서 버그

```cpp
// CommitMatchTravel()
if (!World->ServerTravel(...)) { bTravelCommitted = false; }   // ① 먼저 잠금 해제
return false;

// 호출자
if (!CommitMatchTravel(...)) { ScheduleLobbyStatusRestore(); } // ② 복구 시도

// ScheduleLobbyStatusRestore()
if (!bTravelCommitted || bRestoringLobbyStatus) { return; }    // ③ 잠금이 이미 풀려 즉시 return
```

①이 ②보다 먼저 상태를 바꿔 버려서, ③ 때문에 **복구가 실행되지 않았다.** 로컬 잠금은 풀리지만 Steam 로비는 "경기 시작 중(입장 불가)"으로 남고, 승인 명단도 지워지지 않았다.

상태를 바꾸는 책임이 두 함수에 나뉘어 있어 생긴 문제였다. `CommitMatchTravel()`은 잠금을 건드리지 않게 하고, 실패 후 처리는 호출자의 복구 루틴이 맡게 했다.

```cpp
/*
 * 실패 시 잠금 해제는 호출자가 ScheduleLobbyStatusRestore()로
 * Steam 광고 상태를 되돌린 뒤 처리한다.
 */
return World->ServerTravel(TravelURL, false, false);
```

## 문제 3 — 경기 후 복귀 로비가 한 명 미도착으로 잠김

경기가 끝나면 참가자들이 `ReturningPlayers=N` 옵션과 함께 로비로 돌아온다. N명이 모두 도착할 때까지 시작이 거부되고, 승인 명단에 없는 사람은 들어올 수 없다. 그런데 다음 처리가 없었다.

- 한 명이 게임을 종료해 끝내 도착하지 않는 경우의 타임아웃
- Steam 로비 재개 요청이 실패했을 때의 재시도

```cpp
void ADMLobbyGameMode::HandleReturningPlayersTimeout()
{
    if (!HasAuthority() || ExpectedReturningPlayers <= 0 || bTravelCommitted) { return; }

    // Steam 재개 응답을 기다리는 중이면 잠시 후 다시 확인한다.
    if (bReopeningReturningLobby) { /* 5초 후 재확인 */ return; }

    // 도착한 인원 기준으로 복귀를 마무리한다(호스트는 항상 포함되므로 최소 1).
    ExpectedReturningPlayers = FMath::Max(1, GetNumPlayers());
    TryReopenReturningLobby();

    // 재개 요청을 시작하지 못했다면(세션 Busy 등) 일정 시간 후 다시 시도한다.
    if (ExpectedReturningPlayers > 0 && !bReopeningReturningLobby) { /* 5초 후 재시도 */ }
}
```

`StartPlay`에서 복귀 타임아웃(기본 30초)을 예약하고, 재개에 성공하면 해제한다. `EndPlay`에서는 이 GameMode가 만든 타이머를 모두 정리한다.

## 검증 방법

실패 주입은 테스트용 사본 프로젝트에서 진행한다.

| 시나리오 | 주입 방법 | 확인할 결과 |
|---|---|---|
| 복구 실패 | `UpdateLobbyMatchStarting(false)`가 false를 반환하게 함 | 재시도 4회 후 잠금 해제, Ready와 재시작 가능 |
| Travel 실패 | `MatchMap`을 잘못된 경로로 설정 | 복구 루틴 실행, Steam 로비 재입장 가능 |
| 복귀 미도착 | 3인 경기 결과 화면에서 1명 종료 | 30초 후 복귀 대기 종료, 2인 시작 가능, 신규 입장 가능 |

## 정리

- 잠금 플래그는 **해제 경로를 먼저 센다.** 모든 실패 분기에 해제나 재시도가 있어야 한다.
- 이유가 있는 안전 장치는 없애지 않고, **재시도와 상한**을 붙여 의도를 살린다.
- 상태 변경 책임이 두 함수에 나뉘면 호출 순서에 따라 결과가 달라진다. 책임을 한 곳으로 모아 순서 의존성을 없앴다.
- 비동기 외부 서비스(Steam)를 기다리는 상태에는 언제나 시간 상한이 필요하다.

---

[Refactoring 목차](README.md)
