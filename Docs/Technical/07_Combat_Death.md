# 07. 전투 — 공격 예측, 서버 히트 판정, 사망 확정

## 응답성과 권한

공격 입력을 누르면 원격 클라이언트는 몽타주를 먼저 재생하지만, 공격이 성공했는지는 서버가 정한다.

1. `UDMCombatComponent::TryStartPredictedAttack`이 로컬 `PredictionId`를 만들고 `ServerRequestAttack`에 함께 보낸다.
2. 서버가 거절하면 `ClientRejectPredictedAttack`이 **같은 ID의 예측 연출만** 되돌린다.
3. 호스트는 로컬 서버 처리와 겹치지 않도록 예측 경로를 쓰지 않는다.

클라이언트 예측은 입력 반응용 몽타주에만 쓴다. 히트 판정을 클라이언트에 넘기지 않는다. 클라이언트가 추정한 서버 시간과 실제 서버 시간의 오차 때문에 쿨다운 경계에서 생기던 거절은 [서버 쪽 여유 0.1초](../Debugging/Gameplay/06_RemoteCooldown_Tolerance.md)로 보정했다.

## 히트 판정

1. `ServerRequestAttack`에서 행동 상태, 쿨다운, 생존 여부를 검사한다.
2. 서버에서 공격 몽타주를 시작하고, `UDMAnimNotify_AttackHit` 시점에 판정한다. 누르자마자 상대가 쓰러지지 않는다.
3. `AttackSequence`와 `LastResolvedAttackHitSequence`로 같은 공격의 중복 Notify를 무효화한다.
4. `ECC_Pawn` 오버랩으로 후보를 모으고 거리·전방·월드 지형 차폐를 다시 검사한다. 범위 안의 유효 대상은 한 공격에서 모두 판정한다.
5. 대상의 스폰 보호, 방어 각도, 남은 방어 횟수를 확인한 뒤 사망 또는 NPC 오인 공격 반응을 Commit한다.

방어는 `ActionState == Defense`가 원본이다. 별도 bool을 두지 않는다. 서버가 이미 소비한 방어 횟수는 클라이언트 연출로 되돌리지 않는다. 투명화 중 공격을 시도하면 공격 판정 전에 서버가 투명화를 해제한다. NPC를 때리면 공격자가 3초 기절하고, 한 공격에서 NPC를 여러 명 맞혀도 기절은 한 번만 적용한다.

## 사망 경계

`ADMMatchGameMode::TryCommitPlayerDeath`와 `CommitDeathState`가 LifeState 전이의 원본이다. 사망이 확정되면 다음 순서로 정리한다.

```text
TryCommitPlayerDeath
  → CommitDeathState (LifeState 변경)
  → CleanupDeathInteractionAndRelic (진행 세션·예약 정리, 운반 유물 Death Drop)
  → 스킬·전투 동작 정리
  → UDMJellyPhysicsComponent 사망 Ragdoll 시작
  → 관전 Context 전송, UDMRespawnCoordinator 30초 리스폰 시작
```

Ragdoll은 표현일 뿐이다. 메시가 쓰러졌다는 사실로 사망 상태가 정해지지 않는다. 감정표현의 가짜 죽음(FakeDeath)과 실제 사망도 서로 다른 상태다.

## 관련 코드

- [DMCombatComponent.cpp](../../Source/DontMelt/Private/Characters/Components/DMCombatComponent.cpp) — `RequestAttack`, `ServerRequestAttack_Implementation`, `TryStartPredictedAttack`, `HandleAttackHitNotify`, `GatherValidAttackTargets`, `TryConsumeDefenseBlockAgainst`
- [DMMatchGameMode.cpp](../../Source/DontMelt/Private/Framework/Match/DMMatchGameMode.cpp) — `TryCommitPlayerDeath`, `CommitDeathState`, `CleanupDeathInteractionAndRelic`

## 핵심 코드

```cpp
/*
 * 아직 유효한 공격 Sequence가 아니거나,
 * 현재 공격에서 Hit Notify가 이미 처리됐다면
 * 중복 판정을 막는다.
 */
if (AttackSequence == 0 || LastResolvedAttackHitSequence == AttackSequence)
{
    return;
}

/*
 * 동일 Montage에 Notify가 실수로 여러 개 들어가더라도
 * 공격 판정은 한 번만 실행된다.
 */
LastResolvedAttackHitSequence = AttackSequence;
```

---

[Technical 목차](README.md)
