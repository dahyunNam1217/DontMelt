# 08 — 스킬 시스템

## 1. 구조

`UDMSkillComponent`는 `ADMMatchPlayerState`가 소유한다. 쿨다운과 대상 선택은 Pawn이 죽거나 바뀌어도 유지돼야 하므로 Character에 두지 않는다. PlayerController는 로컬 대상 선택 상태와 요청 전달만 맡고, 상태 효과는 기존 `UDMCharacterStateComponent`의 GameplayTag로 적용한다.

```text
PlayerController                 입력·대상 정책 요청·개인 화면 효과
  → PlayerState / UDMSkillComponent   서버 승인·대상 선택·쿨다운
  → CharacterStateComponent           실제 상태 효과 적용
  → AudioComponent / PostProcess      클라이언트별 표현
```

## 2. 스킬 데이터

| SkillTag | 쿨다운 | 지속 | 대상 |
|---|---:|---:|---|
| `Skill.Invisibility` | 25초 | 5초 | 자신 |
| `Skill.FootstepAmplify` | 60초 | 8초 | 유효 상대 |
| `Skill.VisionDistort` | 600초 | 주효과 5초 | 유효 상대 |

시야 교란 화면 표현은 진입 0.75초 + 주효과 5초 + 해제 0.5초, 합계 6.25초다. DataAsset의 `Duration`은 주효과 5초를 뜻한다.

## 3. 쿨다운과 서버 검증

저장하는 값은 `SkillTag`와 `EndServerTime`이고, 남은 시간은 `max(0, EndServerTime − CurrentServerTime)`이다. 쿨다운은 소유자에게만 복제한다. 사망·기절·관전 중에도 흐르고, 사용 실패와 대상 선택 취소는 쿨다운을 소비하지 않는다.

스킬을 Commit하기 전에 Phase, LifeState, ActionState, 상태 효과, 자세, 유물 운반 여부, 대상 자격, 쿨다운을 검증한다. 로컬 UI의 "사용 가능" 표시만 믿고 실행하지 않는다.

## 4. 대상 선택

클라이언트는 `RandomEligible` 또는 `LeadingEligible` 정책만 보내고, Actor 포인터나 상대 정보를 지정하지 않는다. 후보는 플레이어만 허용하고 NPC, 자기 자신, 사망자, 리스폰 보호 중인 대상을 뺀다. 거리와 시야 제한은 두지 않는다.

선두 기준은 `ADMRelicManager::GetRegisteredRelicCount(BaseSlot)`이다. 동률 선두는 그중 무작위로 고른다. 후보가 없으면 대상 선택을 끝내고 쿨다운을 유지한다.

## 5. 개별 효과

**투명화.** 서버가 `Status.Invisible`을 적용한다. Actor를 없애거나 충돌을 끄지 않고 Mesh 가시성만 숨긴다. 유물을 들고 있을 때는 쓸 수 없고, 공격을 시도하면 서버가 해제한다. 관찰자에게는 표현용 bool로 상태를 보내, 늦게 Relevant해진 클라이언트도 투명 상태를 본다.

**발소리 증폭.** 공통 `UDMCharacterAudioComponent`의 발소리 파이프라인을 쓴다. 시전자가 대상의 발소리를 더 크게 듣는 로컬 표현이며, 대상 캐릭터에 새 GameplayTag를 붙이지 않는다. 투명화의 발소리 볼륨 증가, 여러 시전자의 효과 중첩을 따로 처리한다.

**시야 교란.** 서버가 적용을 승인하고, 대상 클라이언트의 PostProcess MID로 효과를 재생한다. 효과 파라미터와 시간 보정은 로컬 표현 책임이다. 다른 플레이어에게 대상의 정체를 알리지 않는다.

## 6. UI와 입력

- 1·2·3번 입력, 대상 선택에서 LMB 랜덤·RMB 선두.
- 대상 선택 중에는 `IMC_Gameplay`를 내리고 `IMC_SkillTargeting`을 쓴다.
- UI는 스킬별 키·아이콘·쿨다운을 읽기만 한다. 실제 사용 가능 여부는 SkillComponent가 계산한다.
- 사망·리스폰·접속 종료·경기 종료 때 남은 일시 효과와 로컬 화면 효과를 정리한다.

## 7. 이후 변경

퀘스트 완료 보상(쿨다운 회복)은 이 단계에서 보류했고, 10단계에서 제거했다. 스킬 쿨다운은 퀘스트 결과와 관계없이 흐른다.

관련 문서: [스킬 구현](../../Technical/10_Skill_Cooldown_Target.md)

[Stages 목차](README.md)
