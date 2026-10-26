# 리스폰에 한 번 실패하면 관전 상태에 계속 남는 문제

| 항목 | 내용 |
|---|---|
| 문제 | 리스폰 시점에 유효한 지점이 없거나 Pawn 생성·Possess·SpawnProtection 설정에 실패하면, 그 플레이어는 경기 끝까지 관전 상태로 남았다 |
| 원인 | 실패 시 잘못된 Pawn을 남기지 않는 처리는 있었지만, **누가 다시 시도시키는가**가 빠져 있었다 |
| 해결 | 세 실패 분기에서 1초 뒤 재시도를 예약하고, 기존 타이머 자료구조를 재사용해 정리 경로도 그대로 쓰게 했다 |
| 관련 코드 | `UDMRespawnCoordinator::HandleRespawnTimerExpired`, `ScheduleRespawnRetry` |

## 원인 추적

리스폰 타이머 만료 함수는 시작 부분에서 기존 타이머 기록을 지운다. 이후 실패하는 세 분기는 Spectating으로 되돌리거나 함수를 끝내지만, 다시 호출할 타이머를 설정하지 않았다.

```text
Respawn Timer 만료 → Timer 기록 제거
  ├─ RespawnPoint 없음          → return
  ├─ Pawn 생성/Possess 실패      → Spectating, return
  └─ SpawnProtection 적용 실패   → Spectating, return
```

서버는 Alive로 잘못 전환하지 않았으니 "안전한 실패"처럼 보였다. 하지만 다음 기회가 없으므로 플레이어에게는 경기가 끝난 것과 같았다.

## 해결

```cpp
void UDMRespawnCoordinator::ScheduleRespawnRetry(ADMMatchPlayerState* MatchPlayerState)
{
    // 경기가 진행 중이고 Spectating 상태일 때만 재시도한다.
    if (!IsValid(MatchPlayerState) || MatchPlayerState->GetLifeState() != EDMPlayerLifeState::Spectating)
    {
        return;
    }

    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState) || MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None)
    {
        return;
    }

    FTimerHandle& RetryTimerHandle =
        RespawnTimerHandles.FindOrAdd(TWeakObjectPtr<ADMMatchPlayerState>(MatchPlayerState));

    FTimerDelegate RetryTimerDelegate;
    RetryTimerDelegate.BindUObject(this, &ThisClass::HandleRespawnTimerExpired, MatchPlayerState);

    GetWorldTimerManager().SetTimer(RetryTimerHandle, RetryTimerDelegate, RespawnRetryDelaySeconds, false);
}
```

- **기존 자료구조 재사용:** 재시도 타이머를 원래의 `RespawnTimerHandles`에 넣었다. 그래서 기존 `ClearRespawnTimer()`와 `Logout()` 정리 경로가 재시도 타이머까지 함께 지운다. 정리 코드를 새로 쓰지 않아도 됐다.
- **재시도하지 않는 경우:** 경기 종료 사유가 정해졌거나, 접속이 끊겨 Controller가 없으면 재시도하지 않는다.
- **무한 재시도 여부:** 경기 시작 전에 `ValidateRespawnPointsForStartingRoster()`가 모든 진영의 RespawnPoint를 검증한다. 그래서 1초 재시도가 계속 반복되는 경우는 경기 중 지점이 비활성화되는 상황으로 좁혀진다.

이 로직은 처음에 `ADMMatchGameMode` 안에 있었고, 이후 [GameMode 책임 분리 리팩토링](../../Refactoring/01_Match_Responsibility_Split.md)에서 동작을 유지한 채 `UDMRespawnCoordinator`로 옮겼다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| 리스폰 직전 해당 진영의 RespawnPoint 비활성화 | `Scheduled respawn retry` 로그, 지점 복구 후 다음 시도에 리스폰 |
| 재시도 대기 중 경기 종료 | 재시도 중단 |
| 재시도 대기 중 퇴장 | 타이머 정리 |

## 정리

- 실패 처리는 "잘못된 상태를 남기지 않는다"에서 끝나면 안 된다. **"그다음은 누가 책임지는가"** 까지 있어야 한다.
- 새 기능을 기존 자료구조에 얹으면 기존 정리 경로를 그대로 물려받는다.

---

[Debugging 목차](../README.md)
