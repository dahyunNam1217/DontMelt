# 07. 스킬·대상 선택·쿨다운

## P2 승인 동기화 — 2026-11-13

이번 사용자 P2 요청의 명시 범위만 **설계 승인**으로 채택한다. 과거 승인으로 소급하지 않는다. 기준 HEAD는 `914173a`이며 구현 사실·설계 승인·에셋 적용·실행 검증은 별개다. 상세 근거와 미결정 사항은 [ImplementationStatus 10절](../ImplementationStatus.md#10-p2-승인-설계-및-현재-구현--2026-11-13)을 따른다.

**아래 기존 본문과의 관계:** 이 절 뒤의 기존 본문은 삭제하지 않고 이전 설계·조사 기록으로 보존한다. E1~E10과 충돌하는 값·완료/승인/미확인 표현은 이 절과 중앙10절이 대체한다. 특히90초·Walk300·NPC×10/40·NPC 발소리·3요소 이벤트 HUD·타임아웃 즉시 재요청은 최신 목표가 아니다. E1~E10 밖의 항목을 이번 승인으로 일괄 확정하지 않는다.

### E6 — NPC 이동 발소리 금지

**설계 승인:** NPC는 이동 발소리를 내지 않고 PlayerCharacter만 이동 발소리를 낸다. 사용자가 직접 확인한 의도이며 정책 자체를 재설계하지 않는다. NPC 비명·웃음·이벤트 사운드는 별개다. 이동 발소리는 플레이어 식별 단서가 될 수 있고 발소리 증폭·투명화 표현과 연결된다.

**구현 확인:** `UDMCharacterAudioComponent::CanPlayFootstepLocally`는 소유자가 `ADMPlayerCharacter`인 경우만 허용한다. `HandleFootstepNotify`는 이 검사를 거쳐 로컬 재생하며 발소리 전용 RPC를 추가하지 않는다. 조사한 C++ 발소리 경로에서 NPC 허용 우회는 확인하지 못했다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E6 절을 함께 읽는다.

### E10 — 정보 공개 범주와 Listen Host 신뢰 경계

**설계 승인:** Server Authority·최소 전송·OwnerOnly를 유지한다. 전체 Client 공개 / Owner Client 전용 / 스킬 시전자 전용 / 관전자 예외 / 서버 전용으로 구분한다. 판정 원본과 표현 데이터를 분리한다. UI 비표시를 비전송 보장으로 취급하지 않는다. Listen Host는 권위 서버를 실행하는 신뢰 서버다. Host에게 서버 원본까지 은폐한다고 보장하지 않는다.

| 범주 | 목표 계약 | 현재 코드 관측 / 승인 경계 |
|---|---|---|
| 전체 Client 공개 | 공개 경기 상태·필요한 관찰 표현만 | GameState Phase·공개 이벤트/퀘스트·최종 결과, PlayerState rematch ready, Character Action/스턴·투명 표현. Relevant 범위와 필드별 재검수 필요 |
| Owner 전용 | 개인 상태·개인 응답 최소화 | MatchPlayerState LifeState/BaseSlot/운반 여부, CharacterState 상세 태그·투명화 시간은 OwnerOnly |
| 시전자 전용 | 기능에 필요한 최소 정보의 예외 명세 | ClientSetFootstepAmplifyPresentation은 대상 PlayerState를 전달. 현재 전달 사실이며 해당 참조 전체의 최종 예외 승인은 시스템 명세 확인 필요 |
| 관전자 예외 | 관전 정책상 필요한 범위만 | ClientBeginSpectatorSequence의 Context·KillerPawn 등은 현재 구현. 숨길 정보와 예외 범위를 별도 확인 |
| 서버 전용 | 원본 판정·불필요한 신원/운반 관계 | RelicManager 비복제, 스킬 서버 대상 선정. 클라이언트가 대상 Actor 대신 선택 정책을 제출 |

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E10 절을 함께 읽는다.

**검증 상태:** BP/DataAsset/Animation 실제 적용은 에셋 미검증, 빌드·PIE·Steam은 실행 미검증이다. 아래 완료 기준은 향후 검증 시나리오이며 통과 기록이 아니다. C++·WBP·Config 변경은 이번 문서 승인에 포함되지 않는다. [공통 게임 규칙](../GameplayRules.md) · [네트워크 정책](../NetworkPolicy.md)를 상호 참조한다.

---

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
