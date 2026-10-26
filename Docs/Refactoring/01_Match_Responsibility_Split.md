# 거대해진 GameMode·PlayerController의 책임 분리

| 항목 | 내용 |
|---|---|
| 문제 | 기능을 단계별로 쌓으면서 `ADMMatchGameMode`와 `ADMMatchPlayerController`가 서로 다른 수명과 책임을 한 클래스에 모두 갖게 됐다 |
| 접근 | 동작을 바꾸지 않는 것을 원칙으로, **수명과 실행 위치가 다른 코드**부터 별도 객체로 옮겼다 |
| 결과 | 리스폰 수명주기, 관전 연출, 승패 규칙, 공통 조회 함수, 매치 상수를 분리했다. 승패 규칙은 월드 없이 Automation Test로 검증할 수 있게 됐다 |

## 분리 전 상태

11단계까지 기능을 붙이면서 두 클래스에 책임이 몰렸다.

| 클래스 | 갖고 있던 책임 |
|---|---|
| `ADMMatchGameMode` | 참가자 확정, Phase 전환, 사망 확정, **30초 리스폰 타이머·지점 선택·재시도**, **승패 판정 계산**, 결과 작성, 결과 ACK와 Travel |
| `ADMMatchPlayerController` | 입력 모드, HUD, 개인 RPC 응답, **관전 4단계 카메라·Fade·카운트다운·제한 관전 Pawn** |

문제는 크기보다 **성격이 다른 코드가 섞여 있다는 점**이었다.

- 리스폰은 사망 확정 뒤 30초짜리 독립 수명주기를 갖는다. 승패 판정과 같은 클래스에 있을 이유가 없다.
- 관전 연출은 **소유 클라이언트에서만** 실행된다. 서버 요청을 처리하는 Controller 코드와 섞여 있어, 어떤 함수가 어디서 실행되는지 읽기 어려웠다.
- 승패 판정은 PlayerState·RelicManager에서 값을 모아 계산하는 순수한 규칙인데, GameMode 안에 있어 월드 없이는 테스트할 수 없었다.

## 분리한 구조

```text
ADMMatchGameMode (서버)
├─ 사망 확정, Phase, 결과 작성, Travel        ← 남은 책임
├─ UDMRespawnCoordinator (서버 전용 UObject)
│    30초 대기 타이머, RespawnPoint 선택, 새 Pawn 생성, 실패 시 롤백·재시도
└─ DMMatchResultRules (순수 함수 namespace)
     유물 승리 · 전멸 승리 · 연장전 승자 · 결과표 정렬/순위

ADMMatchPlayerController (소유 클라이언트 + 서버 RPC)
├─ 입력 모드, HUD, 개인 RPC 응답              ← 남은 책임
└─ UDMSpectatorPresentationComponent (소유 클라이언트 전용)
     Corpse → KillerView → FreeCamera → Fade, 제한 관전 Pawn, 카운트다운

공통
├─ DMGameplay:: GetServerTimeSeconds / GetMatchPlayerStates / GetLocalInputSubsystem
└─ DMMatch::    MinPlayers / MaxPlayers / MaxBaseSlots / MaxRelicCount ...
```

### 리스폰 — `UDMRespawnCoordinator`

```cpp
/**
 * 사망 후 리스폰 수명주기의 서버 전용 담당자.
 *
 * ADMMatchGameMode가 Outer이자 소유자이며 Client에는 존재하지 않는다.
 * 사망 확정과 승패 판정은 GameMode에 남고, 이 객체는 그 이후의
 * 30초 대기 타이머, RespawnPoint 선택, 새 Pawn 생성과 실패 시 롤백·재시도만 맡는다.
 *
 * 생존 상태의 원본은 계속 ADMMatchPlayerState의 LifeState다.
 */
UCLASS()
class DONTMELT_API UDMRespawnCoordinator : public UObject
```

- Actor가 아니라 GameMode를 Outer로 둔 `UObject`라서 복제·Tick·월드 배치 비용이 없다.
- 상태의 원본(LifeState)은 옮기지 않았다. Coordinator는 **절차**만 갖고, 데이터는 계속 PlayerState가 소유한다.
- 이미 고친 [리스폰 재시도](../Debugging/Gameplay/07_Respawn_Failure_Retry.md)와 [Handoff](../Debugging/Gameplay/08_Respawn_Handoff.md) 동작은 그대로 옮겼다.

### 관전 연출 — `UDMSpectatorPresentationComponent`

```cpp
/**
 * 소유 Client에서만 동작한다. 실제 사망·리스폰 판정은 서버가 하고,
 * Controller의 Client RPC가 이 컴포넌트의 시작·종료 함수를 호출한다.
 */
UCLASS(ClassGroup = (DontMelt))
class DONTMELT_API UDMSpectatorPresentationComponent : public UActorComponent
```

Controller에는 RPC 진입점(`ClientBeginSpectatorSequence`, `ClientFinalizeSpectatorRespawnHandoff`)만 남기고, 실제 카메라 단계 전환과 정리는 컴포넌트가 맡는다. "서버가 판정하고 클라이언트가 표현한다"는 경계가 클래스 단위로 드러난다.

### 승패 규칙 — `DMMatchResultRules`

GameMode가 PlayerState와 RelicManager에서 값을 모아 **값 복사본**(`FDMMatchParticipantSnapshot`)을 만들고, 규칙 함수는 그 값만으로 판정한다.

```cpp
struct FDMMatchParticipantSnapshot
{
    bool bHasAssignedBaseSlot = false;
    int32 BaseSlot = INDEX_NONE;
    EDMPlayerLifeState LifeState = EDMPlayerLifeState::Alive;
    int32 RegisteredRelicCount = 0;   // 판정 순간 자기 진영에 등록된 유물 수
};

namespace DMMatchResultRules
{
    bool FindRelicVictoryWinner(TConstArrayView<FDMMatchParticipantSnapshot> Participants, int32 RequiredRelicCount,
                                int32& OutWinningBaseSlot, int32& OutWinningRegisteredRelicCount);
    bool FindEliminationWinner(TConstArrayView<FDMMatchParticipantSnapshot> Participants, int32& OutWinningBaseSlot);
    // 연장전 승자, 결과표 정렬·순위 ...
}
```

월드가 필요 없으므로 Automation Test 4개(`DontMelt.Match.ResultRules.RelicVictory / EliminationVictory / OvertimeWinner / Ranking`)를 붙였다. 이후 [전멸 승리 순위 규칙](../Debugging/Gameplay/09_EliminationVictory_Ranking.md)을 추가할 때 이 테스트에 경우를 더하는 방식으로 검증할 수 있었다.

### 공통 함수와 상수

여러 클래스에서 같은 조회 코드를 반복하고 있었다.

| 추가 | 대체한 것 |
|---|---|
| `DMGameplay::GetServerTimeSeconds()` | GameState 유무를 확인하고 서버 시간을 읽는 반복 코드 (3개 파일, 20곳에서 사용) |
| `DMGameplay::GetMatchPlayerStates()` | `PlayerArray`를 순회하며 캐스팅하는 반복 코드. 4인 이하라 `TInlineAllocator`로 힙 할당 없이 반환 |
| `DMMatch::MaxPlayers` 등 | 로비·매치·UI에 흩어진 `2`, `4`, `5` 같은 숫자 |

`UPROPERTY`의 `ClampMin/ClampMax`는 문자열이라 이 상수를 쓸 수 없다. 이 제약은 헤더 주석에 적어 두고 값을 함께 맞췄다.

## 진행 방식

| 원칙 | 적용 |
|---|---|
| 동작 보존 | 옮기는 작업과 동작을 바꾸는 작업을 섞지 않았다. 버그 수정은 분리 전후에 별도로 진행했다 |
| 원본 데이터는 그대로 | LifeState, ActionState 같은 상태 원본의 소유자는 바꾸지 않고 절차만 옮겼다 |
| 수명 기준으로 자르기 | "이 코드는 어디서, 언제부터 언제까지 실행되는가"를 기준으로 경계를 정했다 |
| 형식 정리 동시 적용 | `.clang-format`(120자)을 도입해 전체 소스 형식을 맞췄다 |

결과적으로 `DMMatchGameMode.cpp`는 8,380줄에서 4,337줄, `DMMatchPlayerController.cpp`는 5,451줄에서 2,060줄이 됐다. 이 감소분에는 책임 분리와 함께 `.clang-format` 적용으로 여러 줄에 걸쳐 있던 호출이 합쳐진 효과도 포함된다. 분리 후 소스 전체를 다시 검수해, 앞서 고친 수정 사항이 모두 유지된 것을 확인했다.

## 정리

- 큰 클래스를 나눌 때 기준은 줄 수가 아니라 **실행 위치(서버 / 소유 클라이언트)와 수명**이다.
- 상태의 원본은 옮기지 않고 절차만 옮기면, 동작을 바꾸지 않고 구조를 바꿀 수 있다.
- 규칙을 순수 함수로 분리하면 엔진 없이 테스트할 수 있고, 이후 규칙 변경도 테스트로 확인할 수 있다.

---

[Refactoring 목차](README.md)
