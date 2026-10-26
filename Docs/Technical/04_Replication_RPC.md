# 04. 네트워크 동기화 — 복제 범위와 RPC 사용 기준

## 데이터 분류

서버가 원본을 가진 값마다 **누구에게 보여야 하는가**를 먼저 정했다. GameState는 공통 경기 정보, PlayerState는 참가자 상태, 각 Component는 자기 기능의 세부 상태를 맡는다.

| 데이터 | 복제 방식 | 소유자 |
|---|---|---|
| Phase, 결과, 공개 퀘스트·이벤트 | 전체 복제 + OnRep | `ADMMatchGameState` |
| `LifeState`, `BaseSlot`, `bIsCarryingRelic` | `COND_OwnerOnly` | `ADMMatchPlayerState` |
| `bIsRematchReady` | 전체 복제 | `ADMMatchPlayerState` |
| 상호작용 세션 시작·종료 시각 | `COND_OwnerOnly` | `UDMInteractionComponent` |
| 공격·방어 쿨다운 종료 시각 | `COND_OwnerOnly` | `UDMCombatComponent` |
| 스킬 설정·쿨다운 | `COND_OwnerOnly` | `UDMSkillComponent` |
| 행동 상태 `ActionState` | 전체 복제 | `UDMCharacterStateComponent` |
| 상태 효과 태그, 투명화 시각 | `COND_OwnerOnly` | `UDMCharacterStateComponent` |
| 스턴·투명화 표현용 bool | 전체 복제 | `UDMCharacterStateComponent` |

다른 플레이어의 `LifeState`를 공개하지 않는 것은 정보 제한 정책과 연결된다. 관전 중인 사망자가 다른 사람의 생존 여부로 정체를 추리하지 못하게 한다.

## RPC 사용 기준

| 종류 | 용도 | 예 |
|---|---|---|
| Server RPC | 플레이어의 **의도** 제출. 서버가 거리·상태·타이밍을 다시 검사한다 | `ServerRequestInteraction`, `ServerRequestAttack`, `ServerRequestTargetedSkill` |
| Client RPC | 거절·정정, 개인 UI·카메라 | 예측 공격 취소, 스킬 대상 선택 시작, 관전 시작 |
| NetMulticast | 여러 클라이언트가 **그 순간** 볼 짧은 연출 | 공격·방어 몽타주, 사망 Ragdoll 시작 |
| RepNotify | **지속되는 상태**. 늦게 Relevant해진 클라이언트도 현재 값으로 복원 | 스턴·투명화 표현, 퀘스트 사이렌 |

가장 중요하게 본 구분은 **사건과 상태**다. Multicast는 보내는 순간 Relevant한 연결에만 간다. 나중에 Actor가 Relevant해진 클라이언트는 복제 변수의 현재 값만 받는다. 그래서 오래 유지되는 효과는 Multicast만으로 만들지 않는다. 이 원칙을 놓쳐 생긴 문제와 수정은 [투명화 Relevancy](../Debugging/Gameplay/01_Invisibility_Relevancy.md), [퀘스트 사이렌](../Debugging/Gameplay/02_QuestSiren_Relevancy.md)에 정리했다.

Listen Server 호스트는 OnRep을 받지 않는다. 서버 Setter에서 값을 바꾼 뒤 표현 갱신 함수를 직접 호출한다.

## 순서가 보장되지 않는 경우

서로 다른 Actor의 복제와 RPC 사이에는 도착 순서가 보장되지 않는다. 결과 복제(GameState)와 맵 이동 명령(PlayerController)이 그 예다. 순서가 필요하면 확인 응답을 만든다. [결과 ACK 설계](../Debugging/Network/07_MatchResult_Travel_Race.md) 참고.

## 서버 전용 객체

`ADMRelicManager`는 `bReplicates=false`, Tick 비활성화다. 공용 정보는 개별 유물 Actor나 GameState를 통해 공개한다. 상자의 등록 수는 전역 복제하지 않고, 상자를 연 플레이어에게만 서버 응답 스냅샷으로 준다.

## 관련 코드

- [DMMatchGameState.cpp](../../Source/DontMelt/Private/Framework/Match/DMMatchGameState.cpp) — `GetLifetimeReplicatedProps`
- [DMMatchPlayerState.cpp](../../Source/DontMelt/Private/Framework/Match/DMMatchPlayerState.cpp) — `GetLifetimeReplicatedProps`
- [DMCharacterStateComponent.cpp](../../Source/DontMelt/Private/Characters/Components/DMCharacterStateComponent.cpp) — `GetLifetimeReplicatedProps`
- [DMInteractionComponent.cpp](../../Source/DontMelt/Private/Characters/Components/DMInteractionComponent.cpp) — `GetLifetimeReplicatedProps`
- [DMCombatComponent.cpp](../../Source/DontMelt/Private/Characters/Components/DMCombatComponent.cpp) — `GetLifetimeReplicatedProps`
- [DMSkillComponent.cpp](../../Source/DontMelt/Private/Skills/DMSkillComponent.cpp) — `GetLifetimeReplicatedProps`

## 핵심 코드

```cpp
void UDMCharacterStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UDMCharacterStateComponent, ActionState);

    DOREPLIFETIME_CONDITION(UDMCharacterStateComponent, ActiveStatusTags, COND_OwnerOnly);

    // 투명화 시간은 효과를 받은 소유 플레이어에게만 전달한다.
    DOREPLIFETIME_CONDITION(UDMCharacterStateComponent, InvisibleTiming, COND_OwnerOnly);

    // 스턴 표현용 bool은 관찰자에게도 복제한다.
    // 실제 상태 태그의 OwnerOnly 정책은 변경하지 않는다.
    DOREPLIFETIME(UDMCharacterStateComponent, bStunPresentationActive);

    // 투명화 표현용 bool도 관찰자에게 복제한다. 실제 상태 태그는 계속 OwnerOnly다.
    DOREPLIFETIME(UDMCharacterStateComponent, bInvisibilityPresentationActive);
}
```

실제 상태(`ActiveStatusTags`)는 본인에게만 보내고, 다른 사람에게는 **표현에 필요한 bool 하나**만 보낸다. 시야 교란 같은 다른 상태가 함께 노출되지 않는다.

---

[Technical 목차](README.md)
