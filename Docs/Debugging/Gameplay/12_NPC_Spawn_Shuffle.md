# NPC가 매 경기 같은 구역에만 몰리는 문제

| 항목 | 내용 |
|---|---|
| 문제 | 2~3인 경기에서 NPC가 맵 한쪽에만 몰리고, 다른 구역은 비었다 |
| 원인 | 스폰 지점 목록을 섞지 않고 앞에서부터 N개를 썼다. 액터 순회 순서는 대체로 배치 순서를 따른다 |
| 해결 | 스폰 전에 Fisher-Yates로 지점 목록을 섞었다 |
| 관련 코드 | `UDMNPCPopulationDirector::InitializePopulation`, `CollectSpawnPoints` |

## 현상

NPC 수는 `인원 × 배율`이고, 스폰 지점(`DM_NPCSpawn` 태그)은 최대 인원 기준으로 넉넉하게 배치한다. 4인 기준 60개 지점 중 2인 경기는 30개만 쓴다.

사람 사이에 숨는 게임이라 분포 편향은 연출 문제가 아니다. **NPC가 없는 구역에 있는 캐릭터는 곧 플레이어**라는 정보가 된다.

## 원인 추적

```cpp
CollectSpawnPoints(SpawnPoints);              // TActorIterator 순서 그대로
for (int32 NPCIndex = 0; NPCIndex < TargetNPCCount; ++NPCIndex)
{
    ATargetPoint* SpawnPoint = SpawnPoints[NPCIndex];   // 항상 앞쪽 N개
}
```

`TActorIterator`의 순서는 보장된 순서도 아니고 무작위도 아니다. 레벨에 먼저 배치한 지점이 앞에 오는 경향이 있어, 매 경기 같은 구역이 선택됐다.

## 해결

```cpp
if (!CollectSpawnPoints(SpawnPoints))
{
    return false;
}

// 인원이 적어도 매 경기 같은 구역에만 NPC가 몰리지 않도록 스폰 지점 순서를 섞는다.
for (int32 Index = SpawnPoints.Num() - 1; Index > 0; --Index)
{
    SpawnPoints.Swap(Index, FMath::RandRange(0, Index));
}
```

- **Fisher-Yates를 직접 쓴 이유:** 엔진 버전마다 셔플 유틸리티의 위치와 존재 여부가 달라, 의존성 없이 몇 줄로 끝나는 표준 알고리즘을 썼다. 모든 순열이 같은 확률로 나온다.
- **영향 범위:** NPC 수와 "지점 수 ≥ NPC 수" 검증은 그대로다. 섞는 연산은 경기 시작 때 한 번, 수십 개라 비용이 무시할 수준이다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| 2인 경기를 두 번 시작 | NPC 분포가 매번 다름 |
| 4인 경기 | 모든 지점 사용 (기존과 같음) |

## 정리

- 무작위가 필요한 곳에서는 **명시적으로 섞는다.** 순회 순서에 기대지 않는다.
- 숨바꼭질형 게임에서는 배치 편향도 정보 노출이다. 게임 규칙의 관점에서 코드를 읽어야 보이는 문제였다.

---

[Debugging 목차](../README.md)
