# 네트워크·상태 동기화

## FSavedMove로 Sprint 예측

CharacterMovement는 클라이언트가 먼저 움직이고, 서버 결과와 다르면 저장해 둔 이동을 다시 재생해 보정한다. 이동 속도에 영향을 주는 값을 별도 RPC나 Replicated bool로 보내면 이 재생 과정에 끼지 못해, 보정 때마다 캐릭터가 뒤로 당겨진다.

Don't Melt는 달리기 요청을 `FSavedMove_DMCharacter`에 저장하고 `FLAG_Custom_0`으로 압축해 보낸다. 서버는 `UpdateFromCompressedFlags()`에서 같은 의도를 복원한다.

| 함수 | 역할 |
|---|---|
| `SetMoveFor()` / `PrepMoveFor()` | 이동 기록에 달리기 요청 저장, 재생 시 복원 |
| `GetCompressedFlags()` | 플래그로 압축 |
| `CanCombineWith()` | 달리기 상태가 다른 이동끼리 합치지 않음 |

소스: [DMCharacterMovementComponent.cpp](../../Source/DontMelt/Private/Characters/Components/DMCharacterMovementComponent.cpp)

## RepNotify와 Multicast

| 구분 | 용도 | 예 |
|---|---|---|
| 복제 속성 + RepNotify | 늦게 들어오거나 다시 Relevant해진 클라이언트도 알아야 하는 **상태** | 투명화, 스턴 표현 |
| NetMulticast | 그 순간 재생하면 끝나는 **사건** | 타격 이펙트, Ragdoll 시작 |

Multicast는 호출 시점에 Relevant한 연결에만 간다. 투명화를 처음 Multicast로 만들었다가 멀리 있던 상대에게 투명화가 적용되지 않는 문제를 겪었고, 상태는 RepNotify로 옮겼다([Relevancy 문제](../../Docs/Debugging/Gameplay/01_Invisibility_Relevancy.md)).

소스: [DMCharacterStateComponent.h](../../Source/DontMelt/Public/Characters/Components/DMCharacterStateComponent.h) · [DMCombatComponent.h](../../Source/DontMelt/Public/Characters/Components/DMCombatComponent.h)

## Server RPC와 Request ID

클라이언트는 "이걸 하고 싶다"는 의도만 보내고, 서버가 거리·상태·대상 유효성을 다시 검사한다. `Reliable`은 전달을 보장할 뿐, 요청이 정당한지나 중복인지는 판단하지 않는다.

상호작용 요청에는 증가하는 `RequestId`를 붙였다. 서버는 이전 값 이하의 ID를 거절해, 재전송되거나 늦게 도착한 과거 요청이 현재 진행 세션을 건드리지 못하게 한다.

소스: [DMInteractionComponent.cpp](../../Source/DontMelt/Private/Characters/Components/DMInteractionComponent.cpp) · [DMInteractionTypes.h](../../Source/DontMelt/Public/Interaction/DMInteractionTypes.h)

## 종료 시각 기반 타이머

남은 시간을 매번 복제하지 않고, 서버가 정한 **종료 시각**을 한 번 복제한다. 클라이언트는 표시할 때마다 계산한다.

```cpp
const double Remaining = FMath::Max(0.0, EndServerTime - GameState->GetServerWorldTimeSeconds());
```

`GetServerWorldTimeSeconds()`는 클라이언트에서 추정값이다. 그래서 클라이언트 계산은 HUD 표시와 예측에만 쓰고, 실제 판정은 서버 시간으로 한다. 이 추정 오차 때문에 쿨다운 경계에서 거절이 나던 문제는 [원격 요청에만 0.1초 여유](../../Docs/Debugging/Gameplay/06_RemoteCooldown_Tolerance.md)를 둬서 해결했다.

쿨다운, 상호작용 진행도, 퀘스트 제한시간, Overtime 카운트다운이 모두 이 방식이다. HUD는 시간이 지났다고 게임 상태를 바꾸지 않는다.

소스: [DMMatchGameState.cpp](../../Source/DontMelt/Private/Framework/Match/DMMatchGameState.cpp) · [DMSkillComponent.cpp](../../Source/DontMelt/Private/Skills/DMSkillComponent.cpp) · [DMCombatHUDWidget.cpp](../../Source/DontMelt/Private/UI/DMCombatHUDWidget.cpp)

## 조건부 복제

`DOREPLIFETIME_CONDITION`으로 받는 대상을 줄일 수 있다.

```cpp
DOREPLIFETIME_CONDITION(UDMCombatComponent, AttackCooldownEndServerTime, COND_OwnerOnly);
```

정체를 숨기는 게임이라 이 구분이 중요했다. 쿨다운·스킬 상태·개인 퀘스트 진행도는 본인 HUD에만 필요하므로 `COND_OwnerOnly`로 보낸다. 다른 사람에게 보내지 않으면 클라이언트를 뜯어봐도 알 수 없다.

소스: [DMCombatComponent.cpp](../../Source/DontMelt/Private/Characters/Components/DMCombatComponent.cpp)

## Match Phase 상태 머신

`EDMMatchPhase`는 `Waiting → Countdown → Playing → OvertimeIntro → Overtime → Finished`이고, 참가자 도착 실패 같은 내부 오류는 `Aborted`로 끝낸다. Phase는 GameMode만 바꾸고, GameState가 복제한다.

Phase를 나눈 효과는 요청 거절 기준이 한 줄로 정리된다는 점이다. 공격·상호작용·스킬 요청은 모두 "지금 `Playing` 또는 `Overtime`인가"부터 검사한다.

소스: [DMMatchGameMode.cpp](../../Source/DontMelt/Private/Framework/Match/DMMatchGameMode.cpp) · [DMMatchTypes.h](../../Source/DontMelt/Public/Core/DMMatchTypes.h)

## 결과 스냅샷과 ACK

경기가 끝나면 플레이어가 나가거나 맵이 바뀌어 PlayerState를 계속 참조할 수 없다. 그래서 종료 시점에 승패·순위·통계를 결과 데이터로 확정해 GameState에 복제하고, 클라이언트는 받은 결과를 GameInstance에 캐시한 뒤 ACK를 보낸다. 서버는 전원 ACK 또는 5초 타임아웃 뒤 로비로 이동한다.

결과마다 `ResultId`를 붙여 ACK가 어느 결과에 대한 것인지 구분한다. 설계 과정은 [결과 ACK 설계](../../Docs/Debugging/Network/07_MatchResult_Travel_Race.md)에 있다.

소스: [DMMatchGameMode.cpp](../../Source/DontMelt/Private/Framework/Match/DMMatchGameMode.cpp) · [DMMatchPlayerController.cpp](../../Source/DontMelt/Private/Framework/Match/DMMatchPlayerController.cpp)

## BaseSlot

플레이어 배열 순서는 접속·퇴장에 따라 바뀌므로, 상자·스폰 위치·결과 색상은 배열 인덱스가 아니라 `BaseSlot`으로 연결한다. `UpdatePlayerStartSpot`에서 부모 함수보다 먼저 `TryAssignBaseSlot`을 호출해 아직 쓰지 않은 가장 작은 번호를 주고, 슬롯을 받지 못한 플레이어는 `PostLogin`에서 내보낸다.

소스: [DMMatchGameMode.cpp](../../Source/DontMelt/Private/Framework/Match/DMMatchGameMode.cpp) · [DMRelicManager.cpp](../../Source/DontMelt/Private/Relic/DMRelicManager.cpp)

[DevLog 목차](../README.md)
