# 07. 스킬·대상 선택·쿨다운

## 책임

`UDMSkillComponent`는 `ADMMatchPlayerState` 소유로 사망·Pawn 교체 후에도 쿨다운 유지. `ADMMatchPlayerController`는 로컬 대상 선택 UI/Server RPC 및 개인 화면 효과 연결. `UDMCharacterStateComponent`는 서버 StatusTag 원본. 실제 대상은 서버에서만 선택.

## 스킬 표

| 태그 | 입력 | 쿨다운 | 지속 | 처리 |
|---|---|---:|---:|---|
| `Skill.Invisibility` | 1 | 25s | 5s | 자신 Mesh를 숨기되 Actor/충돌은 유지, 공격 시 종료 |
| `Skill.FootstepAmplify` | 2 | 60s | 8s | 시전자에게만 대상 발소리 증폭 |
| `Skill.VisionDistort` | 3 | 600s | 핵심 5s | 대상 Client PostProcess; 0.75s 진입+5s 핵심+0.5s 해제 |

유물 운반 중 투명화 불가. 스킬 Commit 전에 Phase, LifeState, ActionState, 상태 태그, 자세, 운반 여부, 쿨다운/대상 자격을 검증한다.

## 대상 선택(상대 대상 스킬)

1. 2/3번 선택 시 `IMC_SkillTargeting`으로 전환.
2. `LMB` = `RandomEligible`; `RMB` = `LeadingEligible`(RelicManager 등록 수 최대인 적격 상대, 동률 무작위).
3. 클라이언트는 **대상 Actor·ID·진영 정보가 아니라 정책 Enum만** 서버에 전달.
4. NPC/자신/사망/SpawnProtected는 후보 제외. 거리·시야 제한 없음.
5. 대상 선택 취소·후보 없음·서버 거부는 쿨다운을 소모하지 않고 원래 입력으로 복귀.

## 네트워크 및 표현

`SkillTag + EndServerTime` 소유자 복제, HUD는 서버 시각 차이 계산. 관찰자에게는 실제 필요 표현(투명 상태 등) 최소값 RepNotify; Multicast 한번만으로 지속효과 표현 금지(늦은 Relevancy). 발소리 증폭은 서버 대상에 불필요한 새 Tag를 달지 않고 시전자 전용 청취 경로 사용. 시야 교란은 타깃 본인 Client의 PostProcess MID를 로컬로 처리.

## 수명/회귀

- 사망·스턴·관전·리스폰 중에도 **쿨다운 시간 자체는 계속** 흐름.
- 효과 지속 시 사망/경기 종료/월드 이동하면 효과·MID·오디오 버프를 정리.
- 복수 시전자에 대한 발소리 증폭 효과 누적·해제 순서, 투명화와 발소리, 시야 교란 중 관전 Handoff를 시험.
- 원격 화면의 투명화 상태를 새로 Relevant해진 Client에게 복원하는지 확인.

## 완료 기준

Server/Client 각 스킬 사용 가능·불가, 취소·후보 없음, 리스폰 전후 쿨다운, 대상 정보 비노출, 사망 중 로컬 효과 정리, 동시에 시전한 효과들 정확한 종료.

[스킬 설계](../../Docs/Design/Stages/08_Skills.md) · [기술](../../Docs/Technical/10_Skill_Cooldown_Target.md) · [투명화](../../Docs/Debugging/Gameplay/01_Invisibility_Relevancy.md)
