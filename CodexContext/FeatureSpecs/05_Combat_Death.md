# 05. 공격·방어·오인 공격·사망

## 서버 권한 경계

`UDMCombatComponent`가 입력 검증·ActionState·쿨다운·공격 시퀀스·타격 후보를 다룬다. **실제 사망 확정은 `ADMMatchGameMode::TryCommitPlayerDeath`만** 실행한다. `UDMCharacterStateComponent`가 ActionState/Status Tag 원본이며, NPC 반응은 `UDMNPCPopulationDirector`가 소유한다.

## 공격 처리

```text
로컬 LMB → (원격 Client만 예측 공격 몽타주)
  → ServerRequestAttack(PredictionId)
  → Phase/LifeState/ActionState/Cooldown 검증
  → 서버 공격 몽타주 시작
  → UDMAnimNotify_AttackHit
  → AttackSequence 중복 Notify 방지
  → ECC_Pawn 후보 수집 + 거리·수평 전방·World LOS 검사
  → 보호/정면 방어 또는 사망/NPC 오인 공격 처리
```

- 클라이언트가 HitResult·대상·Kill을 결정하지 않는다.
- 원격 Client는 `PredictionId`로 자신의 예측 몽타주만 취소. Host는 이중 재생에 주의.
- 한 타격의 복수 유효 대상 가능. 같은 `AttackSequence`에서 `LastResolvedAttackHitSequence` 중복 Notify는 무시.
- AttackRange/AttackAngle/DefenseArcAngle 실제 수치는 **DataAsset에서 확인**해야 하며 문서에 모든 값이 없다.

## 수치와 규칙

| 항목 | 최종 문서값 |
|---|---|
| 공격 쿨다운 | 8초 |
| 방어 지속/쿨다운 | 1초 / 10초 |
| 방어 | 정면 한정, 첫 유효 공격 **1회** 소비. 방어 창은 ActionState로 판단 |
| NPC 오인 공격 | NPC 생존, 공격자 **3초 기절**; 같은 AttackSequence에서 한 번만 |
| 보호 | 리스폰 후 **3초** 동안 공격·방어·상호작용·스킬 불가, 적의 공격도 피해 없음 |
| 사망 | HP가 누적되는 구조가 아니라 유효 Hit 한 번으로 사망 확정 |

방어 각도 내적 계산은 `DefenseArcAngle` **전체 각도의 절반**을 기준으로 해석. 리스폰 보호 중 공격을 받더라도 방어 횟수는 소비하지 않는다. 공격자 쿨다운은 소비될 수 있음. 투명화 중 공격 시도는 서버 공격 판정 전에 해제.

## 사망 처리 순서

`TryCommitPlayerDeath` → `CommitDeathState` → 상호작용/예약 및 운반 유물 드롭 정리 → 진행 효과 정리 → 사망 Ragdoll 표현 → 경기가 계속되는 경우 30초 리스폰·관전 컨텍스트 시작. FakeDeath 감정표현은 LifeState에 영향을 주지 않는다.

## 오인 공격 VFX 추가 요구

사용자가 이후 제안한 **3D 느낌표 Static Mesh**는 기존 오인 공격 판정·스턴 확정 후 Cosmetic BP 연출로 실행할 수 있다. 어떤 클라이언트가 봐야 하는지 명세 확인 필요. **현재 해당 표현의 구현 여부 미검증**. [16 시각 연출](16_Visual_Effects_And_Presentation.md) 참조.

## 완료 기준

- Client/Host 공격 예측 중복·취소, 서버 Notify 위치 기준 히트·지형 LOS.
- 여러 Pawn 동시 Hit, 한 공격에서 NPC 여러 명이 피격되어도 공격자 스턴이 중복 적용되지 않음.
- 방어 각도 경계/보호 상태, 서버 사망·드롭·결과 판정 충돌.
- 투명화 공격 해제, 3초 스턴 종료, 종료 Phase의 공격/관전 차단.

[전투 설계](../../Docs/Design/Stages/05_CombatDeathRespawn.md) · [전투 기술](../../Docs/Technical/07_Combat_Death.md) · [원격 쿨다운](../../Docs/Debugging/Gameplay/06_RemoteCooldown_Tolerance.md)
