# 05 — 전투·사망·리스폰

## 1. 전투 판정

공격은 `UDMCombatComponent`가 Server RPC를 검증한 뒤 시작한다. Attack Sequence를 올리고, 서버 몽타주의 `UDMAnimNotify_AttackHit` 시점에 범위를 다시 검사한다. 같은 Sequence의 Notify를 두 번 처리하지 않도록 `LastResolvedAttackHitSequence`를 둔다.

```text
입력 → RequestAttack → ServerRequestAttack
→ ActionState=Attack / Cooldown Commit
→ 서버 몽타주의 AttackHit Notify
→ 범위·전방·LOS·대상 상태 재검사
→ 방어·보호 적용 또는 GameMode에 사망 요청
```

대상은 오버랩으로 모으고 중복 Actor를 뺀다. 범위는 DataAsset의 `AttackRange`, 전방 판정은 수평 Forward 기준이며, 월드 지형 시야를 통과한 대상만 맞는다. 전방의 유효 대상 여럿을 한 공격으로 처리할 수 있다. 클라이언트는 HitResult·대상 ID·사망 여부를 보내지 않는다.

## 2. 방어

방어는 `ActionState == Defense`가 원본이고, 별도 방어 창 bool을 두지 않는다. 1초 유지되고 기본 1회 막는다. 방어 각도는 DataAsset의 `DefenseArcAngle`을 쓰고, 실제 내적 판정에는 전체 각도의 절반을 적용한다. 한 번 막아도 1초 모션은 유지된다.

| 기본값 | 수치 |
|---|---:|
| 공격 쿨다운 | 8초 |
| 방어 쿨다운 | 10초 |
| 방어 지속 | 1초 |
| 오인 공격 기절 | 3초 |
| 리스폰 보호 | 3초 |

## 3. 대상별 처리

| 대상 | 서버 처리 |
|---|---|
| 살아 있는 플레이어 | 보호·정면 방어 확인 후 유효하면 사망 요청 |
| 리스폰 보호 중 | 피해 없음, 방어 횟수 소비 없음. 공격자의 쿨다운은 그대로 소비 |
| 방어 성공 | 방어 1회 소비, 사망 없음 |
| NPC | 오인 공격, 공격자 기절 |

한 공격에서 NPC를 여러 명 맞혀도 기절은 Sequence당 한 번이다. 맞은 NPC와 주변 NPC의 비명·도주는 6단계 Population Director가 처리한다.

## 4. 사망 처리

`ADMMatchGameMode::TryCommitPlayerDeath()`가 사망의 유일한 진입점이다. 공격 Component는 `LifeState`를 직접 쓰지 않고 GameMode에 요청한다. 서버가 타격과 피해자 상태를 다시 확인한 뒤 Commit한다.

1. PlayerState의 LifeState를 바꾼다.
2. 진행 중인 상호작용 세션과 예약을 정리한다.
3. RelicManager가 운반 유물을 Death Drop한다.
4. `UDMJellyPhysicsComponent`가 사망 Ragdoll을 시작한다.
5. 경기가 계속되면 리스폰 타이머와 관전 Context를 시작한다.

사망 판정과 Ragdoll은 별개다. 메시가 쓰러졌다고 사망 상태가 되지 않는다. 가짜 죽음 감정표현과 실제 사망도 분리한다. 기절이 시작되면 진행 중인 Sprint·Jump와 이동 속도를 실제 이동 경로에서 멈춘다.

## 5. 리스폰

30초 뒤 서버가 BaseSlot에 맞는 유효 RespawnPoint를 고른다. 새 Pawn 생성·Possess에 성공한 뒤 `Alive`를 확정하고 3초 보호를 건다. 보호 중에는 이동·점프만 가능하다. 스킬 쿨다운은 PlayerState 수명이라 리스폰으로 초기화되지 않는다. 리스폰이 실패하면 1초 뒤 다시 시도한다.

3~4인 경기에서 옵션이 켜져 있으면 마지막 생존자 한 명일 때 전멸 승리를 평가한다. 2인전은 쓰지 않는다.

## 6. 이후 변경

| 단계 | 변경 |
|---|---|
| 9단계 | 사망 후 30초 관전 연출을 사망·리스폰 서버 로직과 분리 |
| 10단계 | 공격·방어 HUD, 관전 HUD와 카운트다운 연결 |
| 11.5단계 | 이동 관성은 AnimInstance로, 사망 Ragdoll은 계속 `UDMJellyPhysicsComponent` |
| 이후 | 리스폰 수명주기를 `UDMRespawnCoordinator`로 분리, 실패 시 재시도 |

관련 문서: [전투 구현](../../Technical/07_Combat_Death.md) · [관전과 리스폰](../../Technical/13_Spectator_Respawn.md)

[Stages 목차](README.md)
