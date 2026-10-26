# 결과표의 "유물 수"와 "순위 기준"을 분리한 작업

| 항목 | 내용 |
|---|---|
| 문제 | 결과표의 유물 열과 등수가 모두 `RelicCount`(종료 시 등록 수) 하나를 쓰고, 동률이면 등수가 비었다 |
| 원인 | 기획상 의미가 다른 두 값(누적 획득 / 최종 등록)이 한 필드에 섞였고, 누적 획득 수는 집계하는 곳 자체가 없었다 |
| 해결 | `AcquiredRelicCount`를 새로 두고 서버 Commit 지점에서 집계했다. 순위는 등록 수 기준 공동 순위로 바꿨다 |
| 관련 코드 | `FDMMatchResultEntry`, `ADMRelicManager::TryCommitRelicAcquisition`, `ADMMatchGameMode::BuildFinalPlayerResults` |

## 기획과 코드 비교

| 결과표 열 | 기획 (10단계 설계서) | 기존 코드 |
|---|---|---|
| 등수 | 종료 순간 등록 수 기준, **공동 순위 가능** | 승자는 무조건 1위, 동률이면 등수 공란(`INDEX_NONE`) |
| 획득한 유물 수 | **경기 전체 누적 획득 수** | 종료 순간 등록 수 |

예: A가 누적 5개 획득·종료 시 2개 등록, B가 누적 3개 획득·3개 등록이면 유물 열은 A=5, B=3이고 등수는 B=1위, A=2위다. 탈취가 있는 게임이라 "얼마나 많이 가져갔는가(활동량)"와 "최종적으로 얼마나 지켰는가(성과)"는 크게 다를 수 있다.

## 설계 판단

| 결정 | 이유 |
|---|---|
| 새 필드 `AcquiredRelicCount` 추가, `RelicCount`는 등록 수로 유지 | 기존 필드의 의미를 바꾸면 이를 쓰는 코드와 BP가 조용히 잘못된 값을 쓴다. 새 의미에는 새 이름을 준다 |
| 집계는 `TryCommitRelicAcquisition()` 성공 직후 | 유물 상태의 원본은 RelicManager다. 클라이언트 입력이나 UI가 아니라 **서버 Commit**이 기준이다 |
| PlayerState에 서버 전용으로 보관, 복제 안 함 | 기존 `KillCount`와 같은 방식. 경기 중 다른 플레이어의 성과를 노출하지 않고 결과 Snapshot으로만 전달한다 |
| 접근은 기존 friend 구조를 따름 | `ADMMatchPlayerState`는 이미 GameMode·RelicManager를 friend로 둔다. 새 public API를 만들지 않았다 |
| 승자 정보는 **동률 시 표시 순서**에만 사용 | "결과표 기준이 바뀌었다고 승리 판정을 바꾸지 않는다"는 원칙. 유물·정규·연장 승자는 등록 수 단독 1위라 자연히 1위가 된다 |

전멸 승리만은 승자가 최다 등록자가 아닐 수 있어 예외가 생긴다. 이 부분은 기획 결정을 받아 [전멸 승리 순위 규칙](../Debugging/Gameplay/09_EliminationVictory_Ranking.md)으로 이어서 처리했다.

## 수정 내용

```cpp
// DMMatchTypes.h - FDMMatchResultEntry
// 종료 순간 상자에 등록된 유물 수. 결과표 등수 계산 기준이다.
UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Result")
int32 RelicCount = 0;

// 경기 중 실제로 획득한 누적 유물 수. 결과표 '획득한 유물 수' 열에 표시한다.
UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Result")
int32 AcquiredRelicCount = 0;
```

```cpp
// DMRelicManager.cpp - TryCommitRelicAcquisition() 성공 직후
// 결과표용 누적 획득 수. Pickup·Steal 획득 Commit이 성공한 시점에만 1 증가한다.
PlayerState->IncrementAcquiredRelicCount();
```

```cpp
// 같은 등록 유물 수는 공동 순위를 부여한다(예: 4,2,2,1 → 1,2,2,4).
for (int32 Index = 0; Index < Results.Num(); ++Index)
{
    FDMMatchResultEntry& Entry = Results[Index];
    const bool bIsTied = Index > 0 && Entry.RelicCount == Results[Index - 1].RelicCount;
    Entry.Rank = bIsTied ? Results[Index - 1].Rank : Index + 1;
}
```

기존의 이중 루프(참가자마다 전체를 다시 돌며 동률 검사)가 정렬 후 단일 순회로 바뀌어 읽기도 쉬워졌다. 이 정렬·순위 로직은 이후 [책임 분리 리팩토링](01_Match_Responsibility_Split.md)에서 `DMMatchResultRules::SortAndRankResults`로 옮겨 Automation Test로 검증한다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| A 획득 5·등록 2 / B 획득 3·등록 3 | 유물 열 A=5, B=3 / 등수 B=1, A=2 |
| 등록 수 4, 2, 2, 1 | 등수 1, 2, 2, 4 |
| 탈취 | 탈취한 쪽 획득 수 +1, 빼앗긴 쪽 기존 획득 수 유지 |
| 자기가 떨어뜨린 유물을 다시 주움 | +1 (Pickup Commit마다 집계) |

## 정리

- **하나의 값이 두 질문에 답하고 있다면 분리를 의심한다.** 같은 숫자처럼 보여도 기획상 의미가 다르면 별도 필드로 둔다.
- 통계는 서버의 확정 시점(Commit)에서 집계해야 신뢰할 수 있다.

---

[Refactoring 목차](README.md)
