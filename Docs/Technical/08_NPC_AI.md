# 08. NPC — 서버 인구 관리와 타이머 기반 행동

## 플레이어와 같은 외형의 NPC

`ADMNPCCharacter`는 플레이어와 같은 `ADMJellyCharacterBase`를 상속해 메시·이동·애니메이션 구조를 공유한다. 외형으로는 플레이어와 NPC를 구분할 수 없어야 하기 때문이다. 행동 선택은 `ADMJellyAIController`, 인구와 집단 반응은 `UDMNPCPopulationDirector`가 맡는다. AI 판단은 서버에서만 일어나고, 클라이언트는 이동 복제와 행동 표현만 받는다.

## 인구 확정

경기 시작 인원 `StartingPlayerCountSnapshot`에 `DefaultNPCMultiplier`를 곱해 NPC 수를 정한다. 기본 배율은 15이고, Director는 1~60명을 허용한다.

| 플레이어 | 목표 NPC 수 (배율 15) |
|---:|---:|
| 2명 | 30 |
| 3명 | 45 |
| 4명 | 60 |

서버는 `DM_NPCSpawn` 태그가 붙은 `ATargetPoint`를 수집해 충분한지 검사하고, **순서를 섞은 뒤** 스폰한다. 섞지 않으면 매 경기 같은 구역에만 NPC가 몰려 플레이어 위치가 드러난다([스폰 편향](../Debugging/Gameplay/12_NPC_Spawn_Shuffle.md)). 일부 스폰이 실패하면 등록된 NPC를 회수하고 초기화를 실패로 처리한다. 경기 중 참가자가 나가도 NPC 수는 다시 계산하지 않는다.

## AI 행동

`ADMJellyAIController`는 Behavior Tree나 StateTree 없이 C++ 타이머 기반 상태 머신으로 동작한다.

```text
타이머 만료 → 가중치 기반 행동 선택 → Nav 이동 또는 동작 → 완료 콜백 → 다음 타이머
```

| 행동 | 기본 가중치 |
|---|---:|
| Walk | 40 |
| RelicApproach (유물·상자 근처로 이동 후 정지) | 20 |
| Sprint | 15 |
| Idle | 10 |
| Jump | 5 |

- NPC는 유물·상자에 다가가 머물 수 있지만 줍기·등록·훔치기는 하지 않는다. 플레이어처럼 보이게 하되 게임 결과에는 개입하지 않는다.
- 플레이어의 오인 공격에 직접 맞은 NPC는 반응하고, 주변 NPC는 거리순으로 최대 6명까지 비명·도주한다(반경 1200cm, 도주 거리 900cm).
- 패닉 이벤트는 Director가 등록된 NPC 목록에 전파하고, NPC마다 반응 지연을 둬 동시에 움직이지 않게 한다.
- 최초 행동 0.15~1.0초, 이동 후 0.15~0.4초, 경로 실패 0.5초 대기로 경로 요청이 한 프레임에 몰리지 않게 했다.

## 성능 판단

AI Tick을 끈 것만으로 NPC 60명의 비용이 낮다고 볼 수 없다. NPC는 이동하며 스켈레탈 애니메이션을 재생한다. 측정에서 AI 비용은 작았고, CharacterMovement·애니메이션·이동 복제가 주요 후보로 남았다. 자세한 판단 과정은 [NPC 성능 분석](../Performance/01_NPC_Performance_Analysis.md)에 정리했다.

## 관련 코드

- [DMMatchRuleDataAsset.h](../../Source/DontMelt/Public/Data/DMMatchRuleDataAsset.h) — `DefaultNPCMultiplier`, 반응 반경·최대 응답 수
- [DMNPCPopulationDirector.cpp](../../Source/DontMelt/Private/AI/DMNPCPopulationDirector.cpp) — `InitializePopulation`, `CollectSpawnPoints`, `HandleWrongTargetAttack`
- [DMJellyAIController.cpp](../../Source/DontMelt/Private/Characters/DMJellyAIController.cpp) — `ScheduleNextBehavior`, `SelectNextBehavior`, `OnMoveCompleted`

## 핵심 코드

```cpp
if (!ensureMsgf(
        SpawnPoints.Num() >= InTargetNPCCount,
        TEXT("InitializePopulation failed: not enough NPC spawn points. Required=%d, Found=%d"),
        InTargetNPCCount,
        SpawnPoints.Num()
    ))
{
    return false;
}

// 인원이 적어도 매 경기 같은 구역에만 NPC가 몰리지 않도록 스폰 지점 순서를 섞는다.
for (int32 Index = SpawnPoints.Num() - 1; Index > 0; --Index)
{
    SpawnPoints.Swap(Index, FMath::RandRange(0, Index));
}
```

---

[Technical 목차](README.md)
