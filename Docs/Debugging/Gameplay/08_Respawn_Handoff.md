# 리스폰이 끝나기 전에 관전 화면이 먼저 닫히던 문제

| 항목 | 내용 |
|---|---|
| 문제 | 서버의 리스폰 처리가 중간에 실패해도, 클라이언트 화면은 먼저 게임플레이로 돌아갈 수 있었다 |
| 원인 | 관전 종료 시점을 `PawnClientRestart` 콜백으로 잡았는데, 이 콜백은 서버 리스폰 후처리 전체의 완료를 보장하지 않는다 |
| 해결 | 서버가 리스폰 성공을 확정한 뒤 Client RPC로 Handoff를 알리고, 클라이언트는 그때 관전 상태를 정리하며 Fade를 마지막에 해제하게 했다 |
| 관련 코드 | `UDMRespawnCoordinator`, `ADMMatchPlayerController::ClientFinalizeSpectatorRespawnHandoff`, `CompleteSpectatorRespawnHandoff` |

## 현상

사망 후 30초 관전이 끝나면 새 Pawn으로 돌아온다. 처음에는 `PawnClientRestart` 시점에 관전 상태와 암전 화면을 정리했다. 그런데 새 Pawn 생성이나 Possess가 중간에 실패해도 로컬 화면부터 게임플레이로 돌아가는 경로가 있었다.

## 원인 추적

서버의 리스폰 처리와 클라이언트의 화면 정리를 따로 나열해 보니, 두 흐름 사이에 "전환할 준비가 끝났다"는 신호가 없었다.

| 쪽 | 처리 순서 |
|---|---|
| 서버 | LifeState 검증 → RespawnPoint 선택 → `RestartPlayerAtPlayerStart()` → 새 Pawn 확인 → SpawnProtection 부여 → `LifeState = Alive` |
| 클라이언트 | ViewTarget, 관전 입력, DeathCamera, Fade, Countdown 위젯 정리 |

`PawnClientRestart`는 Possess 직후에 오므로, 서버가 아직 SpawnProtection을 붙이기 전이거나 실패로 되돌리는 중일 수 있다. 성능 문제가 아니라 **서버 생명주기와 클라이언트 표현 계층의 완료 시점이 달랐던 것**이 핵심이었다.

## 해결

로컬 전환 기준을 콜백 호출 여부가 아니라 **서버의 리스폰 성공**으로 옮겼다.

```text
서버 Respawn 성공 (Alive 확정)
  -> ClientFinalizeSpectatorRespawnHandoff()
  -> CompleteSpectatorRespawnHandoff()
  -> 관전 타이머/입력/카메라 정리
  -> 새 Pawn을 ViewTarget으로 지정
  -> 마지막에 Black Fade 해제
```

- 관전 중 남아 있던 Skill Targeting, Chest Snapshot, 시야 효과는 관전 **진입** 시점에 따로 정리한다.
- Fade를 가장 마지막에 해제해, 정리 중인 화면이 잠깐 보이는 일을 막았다.
- 서버 리스폰이 실패하면 Handoff RPC가 오지 않으므로 클라이언트는 관전 화면을 유지하고, [리스폰 재시도](07_Respawn_Failure_Retry.md)가 성공할 때 전환한다.

### 같은 작업에서 찾은 호출 누락

관전 시작을 알리는 `ClientBeginSpectatorSequence()`는 선언·구현되어 있었지만, 실제 사망 처리 경로에서 호출이 빠져 있었다. 사망 처리 → PlayerState의 LifeState 변경 → PlayerController의 클라이언트 함수 순서로 추적해 **누가, 어떤 서버 상태에서, 어느 Owning Client에 보내는지** 확인한 뒤 호출을 복구했다. 서버가 사망과 관전 전환에 필요한 Context(사망 위치, 공격자 Pawn, 시작 서버 시각)를 확정한 다음 보낸다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| 정상 리스폰 | Fade가 한 번만 해제되고 새 Pawn 시점으로 전환 |
| 서버 리스폰 실패 주입 | 클라이언트가 관전 화면 유지, 재시도 성공 시 전환 |
| Host와 원격 Client가 동시에 사망·리스폰 | 양쪽 모두 같은 순서로 전환 |

## 정리

- 엔진 콜백이 **무엇의 완료를 의미하는지** 확인하고 나서 그 콜백에 기능을 묶는다.
- 서버 판정과 클라이언트 표현 사이에 완료 신호가 필요하면, 서버가 확정한 뒤 명시적으로 보낸다.
- 함수가 존재한다는 것과 호출된다는 것은 다르다.

---

[Debugging 목차](../README.md)
