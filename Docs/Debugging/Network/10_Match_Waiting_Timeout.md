# 참가자 한 명이 도착하지 않으면 경기가 대기 상태에서 멈추는 문제

## P3-9 정정 — 2026-11-17

기준 HEAD `001a08241ecd01dfa1ef565f11c7791eaf73eaeb`. 아래 원문은 개발 이력으로 보존한다. 최신 상태는 이 절과 [구현 현황](../../../CodexContext/ImplementationStatus.md)을 따른다. 소스 경로는 `Source/DontMelt/` 기준이며 빌드·PIE·Steam·에셋 검증은 미실행이다.

- `Public/Framework/Match/DMMatchGameMode.h::StartingRosterTimeoutSeconds` 기본값은 **90.0f**다. 서버 `StartPlay`에서 Waiting·종료 사유 None·미확정 명단·비활성 Timer 조건으로 설정한다. 클라이언트별 로딩 시작 시각 기준이 아니다.
- `IsStartingRosterComplete`는 기대 인원, 유효 PlayerState·고유 BaseSlot·Alive·Pawn/Controller/PlayerState 양방향 연결을 검사한다. `HandleStartingNewPlayer_Implementation`도 Spawn/Possess 및 PlayerState 연결을 확인한다.
- `HandleStartingRosterTimeout`은 단발 Timer를 정리하고 ArrivalOrRosterIncomplete / ParticipantPreparationFailed / RequiredInitializationIncomplete를 로그로 구분한 뒤 기존 **InternalError** 중단 경로를 호출한다. 미도착을 이탈로 확정하지 않는다.
- `TryFinalizeStartingRoster`는 리스폰 지점, NPC·유물·퀘스트·이벤트 초기화를 확인한 후 Timer 해제와 튜토리얼 게이트로 진행한다. 실패는 `FailStartingRosterFinalization`의 역순 정리 및 중단 경로다. 튜토리얼 기본300초는 별도다. BP 제한값·사용자 안내·실제 실패 재현은 미검증이다.
아래의 과거 중단 사유·진단 설명보다 최신 InternalError 분류와 초기화 정리 경로를 우선한다.

| 항목 | 내용 |
|---|---|
| 문제 | 승인된 참가자 중 한 명이라도 경기 맵에 오지 못하면, 나머지 전원이 Waiting 화면에 갇혔다 |
| 원인 | Waiting 단계에 기한이 없었고, 시작 판정은 "누군가 입장했을 때"만 다시 실행됐다 |
| 해결 | 90초 기한을 두고, 초과하면 기존 중단 → 결과 화면 → 로비 복귀 경로에 합류시켰다 |
| 관련 코드 | `ADMMatchGameMode::StartPlay`, `HandleStartingRosterTimeout`, `TryAbortMatch` |

## 현상

경기 맵은 로비에서 승인된 인원(`ExpectedPlayers`)이 **전원 도착하고 스폰까지 끝나야** 카운트다운을 시작한다.

| 조건 | 결과 |
|---|---|
| 로비→경기 이동 중 한 클라이언트의 Steam 재접속 실패 | 그 클라이언트만 메인 메뉴로 가고 나머지는 대기 |
| 이동 중 클라이언트 강제 종료·크래시 | 동일 |
| 초기 스폰 검증 실패(PlayerStart 누락 등) | 그 플레이어가 Alive가 되지 않아 시작 조건 불충족 |
| NPC·유물·퀘스트·이벤트 초기화 실패 | 롤백 후 다시 시도하지 않아 대기 |

Steam 패키지 테스트에서 **가장 먼저 마주칠 가능성이 높은 정지 상태**였다.

## 원인 추적

```text
StartPlay → (타이머 없음)
PostLogin / HandleStartingNewPlayer
  └ TryFinalizeStartingRoster()
       ├ IsStartingRosterComplete() == false → return (다음 입장 때 재검사)
       └ 초기화 실패 → 롤백 → return
도착하지 않은 참가자 → Logout도 일어나지 않음(한 번도 들어온 적 없음)
  → Waiting 탈출 경로 없음
```

- 이미 들어온 참가자가 나가면 Logout 처리가 경기를 중단한다(Waiting → Aborted 허용).
- **한 번도 들어오지 않은 참가자**는 서버가 알 방법이 없었다.
- 시작 판정은 입장 이벤트에만 묶여 있어, 입장이 멈추면 판정도 멈췄다.

## 해결

| 안 | 내용 | 판단 |
|---|---|---|
| A. 도착한 인원만으로 시작 | 기한 후 있는 사람끼리 진행 | 유물 수·NPC 수·기지 활성화가 시작 인원 기준으로 고정돼 규칙이 어긋난다. 승인 명단 정책과도 충돌한다 |
| B. 시작 판정을 주기적으로 재실행 | 타이머로 계속 검사 | 도착하지 않은 사람은 계속 없으므로 해결되지 않는다 |
| **C. 기한 후 경기 중단 → 로비 복귀** | 기존 Abort와 결과 화면, 로비 복귀 흐름을 재사용 | **채택.** 새 흐름을 만들지 않고 이미 있는 경로를 탄다 |

C에서 재사용한 경로는 다음과 같다.

- `TryAbortMatch(PlayerDisconnect)`: Waiting → Aborted 전이가 이미 허용돼 있다.
- `StartResultAutoReturnTimer`: 결과 화면을 30초 보여 준다.
- `HandleResultAutoReturnExpired`: **실제로 도착한 인원만** `ReturningPlayers` 명단으로 묶어 로비로 이동한다.
- 로비의 `ReturningPlayers` 재오픈 로직이 그대로 이어받는다.

```cpp
// DMMatchGameMode.h
UPROPERTY(EditDefaultsOnly, Category = "DM|Match|Flow", meta = (ClampMin = "10.0"))
float StartingRosterTimeoutSeconds = 90.0f;
```

```cpp
// DMMatchGameMode.cpp
void ADMMatchGameMode::StartPlay()
{
    Super::StartPlay();

    if (bMatchInitializationFailed)
    {
        AbortMatchForInternalFailure(TEXT("Match initialization failed."));
        return;
    }

    // 승인된 참가자가 끝내 도착하지 않으면 Waiting에 머물지 않도록 기한을 둔다.
    GetWorldTimerManager().SetTimer(
        StartingRosterTimeoutHandle, this, &ThisClass::HandleStartingRosterTimeout,
        StartingRosterTimeoutSeconds, false);
}

void ADMMatchGameMode::HandleStartingRosterTimeout()
{
    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    // 이미 Countdown 이후로 진행했다면 처리하지 않는다.
    if (!IsValid(MatchGameState) || MatchGameState->GetCurrentPhase() != EDMMatchPhase::Waiting)
    {
        return;
    }

    // 도착하지 못한 참가자는 접속 실패로 보고 중단한다. 결과 화면 후 로비 복귀 경로를 그대로 사용한다.
    if (!TryAbortMatch(EDMMatchEndReason::PlayerDisconnect))
    {
        AbortMatchForInternalFailure(TEXT("참가자 도착 대기 시간이 초과되었습니다."));
    }
}
```

카운트다운 진입에 성공하면 타이머를 해제한다. 핸들러도 Phase를 다시 검사하므로 해제 시점이 어긋나도 안전하다. 기한 90초는 Steam P2P 재접속, 경기 맵 로딩, NPC 생성 시간을 고려한 값이고, BP에서 조정할 수 있게 `EditDefaultsOnly`로 열어 두었다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| 3인 경기 이동 중 클라이언트 1명 강제 종료 | 90초 후 `Starting roster timed out` → 결과 화면 → 2명 로비 복귀 |
| 정상 시작 | 90초가 지나도 아무 일 없음 |
| 빠른 확인 | BP에서 기한을 20초로 줄여 같은 시나리오 실행 |

## 정리

- 모든 대기 상태에는 **"끝내 오지 않으면?"** 에 대한 답이 있어야 한다. [리스폰 고립](../Gameplay/07_Respawn_Failure_Retry.md), [로비 시작 잠금](../../Refactoring/02_Lobby_StartLock.md)과 같은 계열의 결함이다.
- 비정상 종료는 새로 만들기보다 **이미 있는 종료 경로에 합류**시키는 편이 안전하다.

---

[Debugging 목차](../README.md)
