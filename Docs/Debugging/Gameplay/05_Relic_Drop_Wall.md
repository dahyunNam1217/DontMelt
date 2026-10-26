# 벽 앞에서 내려놓은 유물을 다시 줍지 못하는 문제

| 항목 | 내용 |
|---|---|
| 문제 | 벽에 붙어 유물을 내려놓으면 유물이 벽 속이나 벽 뒤에 놓여 다시 주울 수 없었다 |
| 원인 | 드롭 위치 검증은 "아래에 바닥이 있는가"만 봤고, 줍기는 시야(LOS) 검사를 통과해야 했다 |
| 해결 | 수동 드롭 전에 줍기와 같은 Visibility 채널로 전방을 검사해, 벽 앞 30cm에 내려놓았다 |
| 관련 코드 | `ADMRelicManager::BuildDropTransform`, `TryDropCarriedRelic` |

## 현상

수동 드롭은 자기 캡슐과 겹치지 않도록 **캐릭터 앞 100cm**에 유물을 놓는다. 충돌 검사가 없어서 벽, 기둥, 큰 소품을 마주 보고 드롭하면 유물이 벽에 반쯤 박히거나 벽 너머에 놓였다. 상호작용 표시도 뜨지 않았다. 유물 수로 승패가 갈리는 게임이라, **그 경기에서 유물 하나가 사라진 것과 같았다.**

## 원인 추적

```text
TryDropCarriedRelic(Manual)
  └ BuildDropTransform: 위치 = 캐릭터 위치 + 전방 × 100   ← 충돌 검사 없음
  └ IsRelicWorldTransformValid: 아래 바닥만 검사          → 통과 (벽 속에도 바닥은 있음)
  └ Dropped 상태로 배치
줍기 시도
  └ ValidateCommonServerRules → HasServerLineOfSightTo    → 벽에 막혀 실패
복구 모니터(1초마다)
  ├ 바닥 있음 → 위치 유효
  └ 마지막 유효 위치에서 움직이지 않음 → 복구 대상 아님
```

배치 검증은 **"그 자리에 있을 수 있는가"** 만 봤고, **"플레이어가 다시 상호작용할 수 있는 곳인가"** 는 보지 않았다. 복구 모니터도 같은 기준이라 이 상태를 놓쳤다.

## 해결

| 안 | 내용 | 판단 |
|---|---|---|
| A. 복구 모니터에 LOS 검사 추가 | 줍기 불가 위치를 찾아 복구 지점으로 이동 | 어느 플레이어 시점에서 볼지 기준이 모호하고, 매초 비용이 든다 |
| B. 벽에 막히면 드롭 거절 | "여기에 내려놓을 수 없습니다" | 조작감이 나빠지고 거절 UI가 필요하다 |
| **C. 드롭 위치를 벽 앞으로 당김** | 전방 레이를 쏴 벽 앞 30cm에 배치 | **채택.** 놓는 순간 원인을 없애고 조작감도 유지된다 |

```cpp
if (DropReason == EDMRelicDropReason::Manual)
{
    constexpr float ManualDropForwardOffset = 100.0f;

    // 벽과 유물 사이에 남길 간격. 다시 주울 때 시야 검사가 벽에 막히지 않게 한다.
    constexpr float ManualDropWallClearance = 30.0f;

    const FVector Forward = CarrierCharacter->GetActorForwardVector();
    const FVector TraceStart = CarrierCharacter->GetActorLocation();
    const FVector TraceEnd = TraceStart + Forward * ManualDropForwardOffset;

    float DropForwardDistance = ManualDropForwardOffset;

    if (UWorld* World = GetWorld())
    {
        FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DMRelicManualDropWallCheck), false, CarrierCharacter);
        QueryParams.AddIgnoredActor(Relic);

        FHitResult WallHit;

        // 앞이 막혀 있으면 벽 속이나 벽 뒤가 아닌 벽 앞에 내려놓는다.
        if (World->LineTraceSingleByChannel(WallHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
        {
            // 캐릭터 뒤쪽으로는 넘어가지 않게 0 이상으로 제한한다.
            DropForwardDistance = FMath::Max(0.0f, WallHit.Distance - ManualDropWallClearance);
        }
    }

    DropLocation += Forward * DropForwardDistance;
}
```

**Visibility 채널을 쓴 이유:** 줍기 LOS 검사가 Visibility 채널을 쓴다. 같은 채널로 검사해야 "놓을 수 있는 곳 = 다시 볼 수 있는 곳"이 맞아떨어진다.

**영향 범위**

- 사망·접속 종료 드롭은 원래대로 캐릭터 발밑이라 바뀌지 않는다.
- 상자 근처 수동 드롭을 막는 기존 규칙(`IsManualDropOverlappingRelicChest`)은 그대로 이어서 적용된다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| 벽에 붙어 드롭 → 즉시 줍기 | 성공 |
| 열린 공간에서 드롭 | 기존처럼 100cm 앞 |
| 상자 앞 드롭 | 기존처럼 거절 |

## 정리

- 배치 검증은 **"있을 수 있는가"** 와 **"다시 상호작용할 수 있는가"** 를 함께 봐야 한다.
- 드롭 검사와 줍기 검사를 **같은 채널**로 맞춰, 두 기준이 어긋나는 틈을 없앴다.

---

[Debugging 목차](../README.md)
