# NPC가 상호작용 시야를 가리던 문제와 1차 수정안을 폐기한 과정

| 항목 | 내용 |
|---|---|
| 문제 | 등록 중 캐릭터와 상자 사이를 NPC나 플레이어가 지나가면 상호작용이 끊기고 유물이 떨어졌다 |
| 원인 | 시야 판정이 "Visibility 트레이스에 처음 맞은 Actor가 대상이어야 성공"이라, 사이에 낀 Pawn이 먼저 맞았다 |
| 해결 | 1차안(오브젝트 타입 트레이스)은 회귀를 만든다는 것을 반영 전에 발견해 폐기했다. 기존 채널 판정을 유지하고 Pawn만 건너뛰는 공통 함수로 다시 구현했다 |
| 관련 코드 | `UDMInteractionComponent::HasServerLineOfSightTo`, `FindLocalInteractionCandidate` |

## 현상

- 군중 속에 숨는 게임이라 NPC가 상자 주변을 배회한다.
- 진행 중인 상호작용은 0.1초마다 시야를 다시 검사한다. 이때 NPC가 사이를 지나가면 `Line of sight is blocked.`로 실패했다.
- 등록(Deposit) 실패에는 유물 Drop 페널티가 있어, 단순한 입력 취소보다 영향이 컸다.

## 원인 추적

```cpp
// 기존 HasServerLineOfSightTo()
QueryParams.AddIgnoredActor(OwnerCharacter);            // 자기 자신만 무시
bHit = LineTraceSingleByChannel(Hit, Eye, TargetCenter, ECC_Visibility, QueryParams);
return bHit && Hit.GetActor() == TargetActor;           // "처음 맞은 것 = 대상"이어야 성공
```

Visibility를 Block하는 캐릭터 메시나 캡슐이 중간에 끼면 바로 실패한다. 같은 규칙이 클라이언트의 상호작용 프롬프트 후보 검사에도 쓰이고 있었다.

## 해결 — 1차안 폐기 과정

### 1차안: 오브젝트 타입 트레이스 (작성 후 폐기)

```cpp
// WorldStatic·WorldDynamic만 차단으로 보고, 대상은 무시
ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
QueryParams.AddIgnoredActor(TargetActor);
return !LineTraceSingleByObjectType(...);   // 지형에 막히지 않으면 성공
```

작성 후 다시 검토하면서 문제를 발견했다.

- **오브젝트 타입 쿼리는 컴포넌트의 채널 응답(Response)을 보지 않는다.** 오브젝트 타입이 목록에 있고 쿼리 충돌이 켜져 있으면 맞는다.
- 그러면 기존에 Visibility를 **Ignore**하던 것들이 갑자기 시야를 막는다. 상자 주변의 QueryOnly 트리거 볼륨(진영 Zone), 상자 안의 유물 Actor 같은 WorldDynamic 오브젝트가 그렇다.
- "Pawn이 가리는 문제"를 고치다가 **"상자 자체를 볼 수 없는 문제"** 를 만들 수 있었다. 판정 의미도 "대상에 닿아야 성공"에서 "막히지만 않으면 성공"으로 바뀐다.

### 최종안: 기존 채널 판정을 유지하고 Pawn만 건너뛰기 (채택)

| 비교 | 1차안 | 최종안 |
|---|---|---|
| 판정 의미 | 막히지 않으면 성공 | **기존과 같음**: 대상에 실제로 닿아야 성공 |
| 채널 응답 | 무시 (회귀 위험) | **존중** (Visibility 그대로) |
| Pawn 처리 | 오브젝트 타입으로 제외 | 맞은 Actor가 Pawn이거나 Pawn에 붙은 Actor면 무시 목록에 넣고 다시 트레이스 |
| 비용 | 트레이스 1회 | 최대 5회. 진행 중 상호작용과 프롬프트 후보 검사에만 쓴다 |

```cpp
namespace DMInteractionLineOfSight
{
    // 사이를 지나가는 Pawn을 건너뛰며 재검사할 최대 횟수.
    constexpr int32 MaxPawnSkipCount = 4;

    /**
     * Visibility 채널 LOS를 검사하되,
     * 사이를 지나가는 Pawn(플레이어·NPC)과 Pawn에 붙은 Actor(운반 중인 유물 등)는
     * 시야 차단으로 보지 않는다.
     * 기존 판정 의미(대상 Actor에 실제로 닿아야 성공, 지형에 막히면 실패)는 그대로 유지한다.
     */
    bool HasVisibilityLineOfSightIgnoringPawns(const UWorld* World, const FVector& Start, const FVector& End,
                                               const AActor* TargetActor, FCollisionQueryParams QueryParams)
    {
        for (int32 SkipIndex = 0; SkipIndex <= MaxPawnSkipCount; ++SkipIndex)
        {
            FHitResult HitResult;
            if (!World->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams))
            {
                return false;
            }

            AActor* HitActor = HitResult.GetActor();
            if (HitActor == TargetActor) { return true; }
            if (!IsValid(HitActor))      { return false; }

            const AActor* AttachParentActor = HitActor->GetAttachParentActor();
            const bool bIsPawnBlocker = HitActor->IsA<APawn>()
                || (IsValid(AttachParentActor) && AttachParentActor->IsA<APawn>());

            // 지형·구조물에 막힌 경우는 기존과 동일하게 실패한다.
            if (!bIsPawnBlocker) { return false; }

            QueryParams.AddIgnoredActor(HitActor);
        }
        return false;
    }
}
```

서버 검증 `HasServerLineOfSightTo()`와 클라이언트 프롬프트 후보 검사 `FindLocalInteractionCandidate()`에 **같은 함수**를 썼다. "프롬프트는 뜨는데 서버가 거부하는" 불일치를 막기 위해서다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| 등록 중 NPC가 사이를 가로지름 | 상호작용 유지, Drop 없음 |
| 등록 중 다른 플레이어가 사이에 섬 | 상호작용 유지 |
| 벽 너머 상자 | 기존처럼 거부 |
| 상자 주변 트리거 볼륨 | 시야 판정에 영향 없음 (1차안의 회귀 지점) |
| 프롬프트와 서버 판정 | 프롬프트가 뜬 대상은 서버도 허용 |

## 정리

- **같아 보이는 두 API가 다른 규칙으로 동작할 수 있다.** 채널 트레이스는 응답을 보고, 오브젝트 타입 트레이스는 타입만 본다. 수정안을 고를 때 판정 의미가 바뀌는지부터 확인한다.
- 수정 범위를 "기존 판정 의미 유지 + 문제 원인(Pawn)만 제외"로 좁히면 회귀 위험이 가장 작다.
- 수정한 코드를 반영 전에 다시 검토하는 단계가 있어서 1차안의 회귀를 미리 잡았다.

---

[Debugging 목차](../README.md)
