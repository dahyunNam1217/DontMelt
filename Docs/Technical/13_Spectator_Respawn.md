# 13. 사망 관전 — 서버 생명주기와 로컬 카메라 단계

## 책임을 나눈 이유

사망과 리스폰을 클라이언트가 정하면 무적 시간이나 부활 시점을 조작할 수 있다. 반대로 카메라 연출을 서버가 매 단계 지시하면 지연이 그대로 화면에 드러난다. 그래서 다음처럼 나눴다.

| 쪽 | 담당 | 클래스 |
|---|---|---|
| 서버 | LifeState, 리스폰 타이머, 새 Pawn 생성 | `ADMMatchGameMode`, `UDMRespawnCoordinator` |
| 소유 클라이언트 | 관전 카메라 단계, 관전 입력, Fade, 카운트다운 | `UDMSpectatorPresentationComponent` |
| 연결 | 시작 Context 전달, 리스폰 완료 Handoff | `ADMMatchPlayerController`의 Client RPC |

## 서버 생명주기

```text
TryCommitPlayerDeath → CommitDeathState → 사망 정리 → StartPostDeathLifecycle
  → UDMRespawnCoordinator::StartRespawnLifecycle (30초)
  → HandleRespawnTimerExpired
       ├ 성공: 새 Pawn + SpawnProtection → Alive → ClientFinalizeSpectatorRespawnHandoff
       └ 실패: Spectating 유지 → 1초 뒤 재시도
```

경기가 사망으로 바로 끝나면 관전을 새로 시작하지 않는다. 리스폰 실패 시 재시도와 Handoff를 서버 성공 뒤로 옮긴 과정은 [리스폰 재시도](../Debugging/Gameplay/07_Respawn_Failure_Retry.md), [Handoff 경합](../Debugging/Gameplay/08_Respawn_Handoff.md)에 정리했다.

## 관전 단계

| 시간 | 단계 | 처리 |
|---|---|---|
| 0~5초 | Corpse | 실제 시체를 중심으로 한 DeathCam. 벽에 막히면 카메라를 앞으로 당긴다 |
| 5~10초 | KillerView | 공격자의 **현재** Pawn을 ViewTarget으로. 공격자가 죽거나 사라지면 DeathCam으로 대체 |
| 10~25초 | FreeCamera | 제한 관전 Pawn. Sweep 이동으로 벽·바닥 통과 방지 |
| 25~30초 | Fade | 암전과 5·4·3·2·1 카운트다운 |

- `ClientBeginSpectatorSequence(FDMSpectatorContext)`로 사망 위치, 공격자 Pawn, 시작 서버 시각을 받는다.
- 지연이 있어도 30초를 처음부터 세지 않는다. `GetServerWorldTimeSeconds() - SpectatorStartServerTime`으로 현재 단계를 계산한다.
- 과거 장면을 녹화하는 Kill Replay는 쓰지 않는다. KillerView도 살아 있는 월드의 현재 모습이다.
- `ADMRestrictedSpectatorPawn`은 소유 클라이언트에만 있는 비복제 Actor다. Pawn Possess는 바꾸지 않고 ViewTarget으로만 쓴다.
- 관전 중에는 정보 제한을 켠다. 다른 플레이어의 정체나 유물 수를 관전 화면에서 알 수 없게 하고, 음성 모드는 사망자 전역 청취로 바꾼다.

## 관련 코드

- [DMMatchGameMode.cpp](../../Source/DontMelt/Private/Framework/Match/DMMatchGameMode.cpp) — `TryCommitPlayerDeath`, `StartPostDeathLifecycle`
- [DMRespawnCoordinator.cpp](../../Source/DontMelt/Private/Framework/Match/DMRespawnCoordinator.cpp) — `StartRespawnLifecycle`, `HandleRespawnTimerExpired`, `ScheduleRespawnRetry`
- [DMMatchPlayerController.cpp](../../Source/DontMelt/Private/Framework/Match/DMMatchPlayerController.cpp) — `ClientBeginSpectatorSequence_Implementation`, `CompleteSpectatorRespawnHandoff`
- [DMSpectatorPresentationComponent.cpp](../../Source/DontMelt/Private/Spectator/DMSpectatorPresentationComponent.cpp) — `BeginCorpseSpectatorPhase`, `BeginKillerViewSpectatorPhase`, `BeginFreeCameraSpectatorPhase`, `BeginFadeSpectatorPhase`
- [DMRestrictedSpectatorPawn.cpp](../../Source/DontMelt/Private/Spectator/DMRestrictedSpectatorPawn.cpp) — `AddRestrictedMovementInput`

## 핵심 코드

```cpp
void ADMMatchPlayerController::ClientBeginSpectatorSequence_Implementation(FDMSpectatorContext SpectatorContext)
{
    if (!IsLocalController() || ShouldBlockSpectatorPresentation() || !IsValid(SpectatorPresentation))
    {
        return;
    }

    // Context 보관, 단계 초기화(Corpse), 정보 제한은 소유 클라이언트 전용 컴포넌트가 맡는다.
    SpectatorPresentation->ReceiveSpectatorContext(SpectatorContext);

    SetLocalVoiceMode(EDMVoiceMode::DeadGlobal);

    // 상자, 스킬 대상 선택, 시야 효과처럼 관전 중 남으면 안 되는 로컬 게임플레이 상태를 정리한다.
    PrepareLocalGameplayForSpectating();

    SpectatorPresentation->BeginCorpseSpectatorPhase();
}
```

```cpp
void UDMSpectatorPresentationComponent::ReceiveSpectatorContext(const FDMSpectatorContext& SpectatorContext)
{
    LocalSpectatorContext = SpectatorContext;

    // 이전 사망 시퀀스의 관전 단계를 새 시퀀스에 넘기지 않는다.
    LocalSpectatorPhase = EDMSpectatorPhase::Corpse;

    SetSpectatorInformationRestricted(true);
}
```

Controller에는 RPC 진입점만 남고, 단계 전환은 컴포넌트가 맡는다.

---

[Technical 목차](README.md)
