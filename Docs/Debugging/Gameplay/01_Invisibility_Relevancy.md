# 투명화한 플레이어가 일부 클라이언트에게는 보이는 문제

| 항목 | 내용 |
|---|---|
| 문제 | 멀리서 투명화한 뒤 다가가면 상대 화면에서 그대로 보였다 |
| 원인 | 투명 연출을 `NetMulticast` 한 번으로만 보냈다. 그 순간 Relevancy 밖에 있던 클라이언트는 신호를 받지 못한다 |
| 해결 | 같은 코드베이스의 스턴 연출처럼, 관찰자용 bool을 복제 변수 + RepNotify로도 남겼다 |
| 관련 코드 | `UDMCharacterStateComponent::bInvisibilityPresentationActive`, `OnRep_InvisibilityPresentationActive` |

## 현상

- A가 B에게서 멀리 떨어진 곳(캐릭터 기본 Net Cull Distance 약 150m 밖)에서 투명화한다.
- A가 투명 상태로 B에게 다가간다.
- B 화면에서는 A가 그대로 보인다. 숨는 게임에서 핵심 스킬이 무력화된다.
- Relevancy 밖으로 나갔다가 다시 들어올 때도 캐릭터 Actor가 새로 생성되므로 같은 문제가 생긴다.

## 원인 추적

투명화와 관련된 정보가 누구에게 어떻게 가는지 표로 정리했다.

| 정보 | 복제 방식 | 받는 대상 |
|---|---|---|
| `ActiveStatusTags` (실제 투명 상태) | `COND_OwnerOnly` | 본인만 |
| 투명 연출 | `MulticastSetInvisibilityPresentation` | **그 순간** 채널이 열린 클라이언트만 |
| 스턴 연출 | `bStunPresentationActive` (Replicated) | 관련된 모든 클라이언트, 나중에 들어와도 받음 |

UE의 Multicast는 **보내는 순간 Relevant한 연결에만** 전달된다. 나중에 Actor가 Relevant해지면 클라이언트는 **복제 변수의 현재 값**만 받고, 지나간 RPC는 다시 받지 않는다. 투명화에는 다른 클라이언트용 "현재 값"이 없었다.

같은 코드베이스의 스턴 연출은 이미 올바른 패턴이었다. **같은 종류의 문제에 서로 다른 패턴이 섞여 있던** 경우다.

## 해결

| 안 | 내용 | 판단 |
|---|---|---|
| A. 상태 태그를 모두에게 복제 | `ActiveStatusTags` 조건 제거 | 시야 교란 등 다른 상태까지 노출돼 정보가 샌다 |
| B. Relevancy 진입 시 서버가 다시 신호 | Relevancy 이벤트 감지 | 엔진에 깔끔한 훅이 없어 복잡하다 |
| **C. 연출용 bool을 따로 복제** | 스턴과 같은 방식 | **채택.** 노출되는 정보는 "투명 연출 중"이라는 사실뿐이고, 기존 패턴과 일치한다 |

```cpp
// DMCharacterStateComponent.h
/**
 * Status.Invisible의 관찰자용 표현값.
 *
 * 투명화 시작 순간 관련성 밖에 있던 Client나
 * 관련성에 다시 들어와 캐릭터가 새로 생성된 Client도
 * 이 값으로 같은 연출을 복구한다. Gameplay 판정에 사용하지 않는다.
 */
UPROPERTY(ReplicatedUsing = OnRep_InvisibilityPresentationActive, Transient, VisibleInstanceOnly,
    Category = "DM|Character|Presentation")
bool bInvisibilityPresentationActive = false;
```

```cpp
// DMCharacterStateComponent.cpp
DOREPLIFETIME(UDMCharacterStateComponent, bInvisibilityPresentationActive);

if (StatusTag.MatchesTagExact(DMGameplayTags::Status_Invisible))
{
    // 관련성 밖에 있던 Client도 나중에 같은 연출을 받도록 복제 값으로도 남긴다.
    bInvisibilityPresentationActive = HasStatus(DMGameplayTags::Status_Invisible);

    // 지금 관련된 Client에는 기존처럼 즉시 반영한다.
    MulticastSetInvisibilityPresentation(bActive);
}

void UDMCharacterStateComponent::OnRep_InvisibilityPresentationActive()
{
    // 멀티캐스트와 같은 값을 다시 적용해도 결과는 같다.
    ApplyInvisibilityPresentation(bInvisibilityPresentationActive);
}
```

**설계에서 고려한 점**

| 고려 사항 | 처리 |
|---|---|
| Multicast를 남긴 이유 | 가까운 클라이언트는 지금처럼 즉시 바뀐다. 복제 변수는 놓친 경우를 메운다. `SetVisibility`라 두 경로가 같은 값을 두 번 적용해도 결과가 같다(멱등) |
| 값의 출처 | 매개변수 대신 `HasStatus`로 실제 상태에서 값을 만들어 원본과 어긋나지 않게 했다 |
| 새로 생성된 Actor | 기본값이 false라, 투명 중인 캐릭터가 새로 생성되면 true가 복제되며 OnRep이 호출된다 |

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| 150m 이상 떨어진 곳에서 투명화 후 접근 | B 화면에서 A가 계속 보이지 않음 |
| 투명화 종료 | 다시 보임 |
| 가까운 거리에서 투명화 | 기존처럼 즉시 사라짐 |

## 정리

- **RPC는 사건, 복제 변수는 상태**다. 나중에 들어온 관찰자도 알아야 하는 정보는 상태로 복제한다.
- 같은 코드베이스에 이미 있는 올바른 패턴을 찾아 맞추면, 새 패턴을 만드는 것보다 위험이 작다.
- 같은 원리로 [퀘스트 사이렌](02_QuestSiren_Relevancy.md)도 상태 복제로 바꿨다.

---

[Debugging 목차](../README.md)
