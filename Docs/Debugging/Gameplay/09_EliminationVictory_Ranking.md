# 전멸 승리에서 결과표 1위가 승자가 아닐 수 있는 문제

| 항목 | 내용 |
|---|---|
| 문제 | 전멸 승리로 끝나면 화면 배너의 승자와 결과표 1위가 다를 수 있었다 |
| 원인 | 결과표 순위는 등록 유물 수 기준이고, 전멸 승리는 유물이 적어도 이길 수 있다 |
| 해결 | 선택지를 정리해 기획 결정을 받은 뒤, **전멸 승리일 때만** 승자를 단독 1위로 고정했다. 순수 함수 규칙과 Automation Test로 검증했다 |
| 관련 코드 | `DMMatchResultRules::SortAndRankResults`, `ADMMatchGameMode::BuildFinalPlayerResults`, `DMMatchResultRulesTests.cpp` |

## 현상

결과표 순위는 **종료 순간 등록 유물 수** 기준이다([결과표 데이터 분리](../../Refactoring/03_MatchResult_Data.md)에서 정한 규칙). 유물·정규 시간·연장전 승리는 승자가 곧 최다 등록자라 문제가 없다.

- 3인 이상 경기에서 전멸 승리(나만 생존)로 끝난다.
- 승자의 등록 유물이 다른 사람보다 적다.
- 배너는 "A 승리"인데 표에는 B가 1위로 나온다.

## 원인 추적

```cpp
// 기존 정렬: 등록 수 → (동률이면) 승자 → BaseSlot
if (A.RelicCount != B.RelicCount)
{
    return A.RelicCount > B.RelicCount;
}
```

승자 정보는 **동률일 때 표시 순서**로만 쓰였다. 결과표 데이터를 정리할 때 "결과표 기준이 바뀌었다고 승리 판정을 바꾸지 않는다"는 원칙으로 정한 것이다. 코드 결함이 아니라 **두 규칙이 충돌하는 지점**이라 코드에서 임의로 정하지 않았다.

## 해결

선택지를 정리해 기획 쪽 결정을 받았다.

| 선택지 | 결정 |
|---|---|
| **① 전멸 승리일 때만 승자를 맨 앞에 고정** | **채택** |
| ② 지금처럼 등록 수 기준 유지 | 기각 (배너와 표가 어긋남) |

**추가 규칙:** 고정된 승자와 등록 수가 같은 참가자도 **공동 1위로 묶지 않는다.** 나머지는 2위부터 등록 수 기준 공동 순위를 매긴다.

```cpp
// 기본값으로 기존 호출(테스트 포함)을 깨지 않는다.
void SortAndRankResults(TArray<FDMMatchResultEntry>& Results, int32 WinningBaseSlot, bool bPinWinnerFirst = false);
```

```cpp
Results.Sort(
    [WinningBaseSlot, bPinWinnerFirst](const FDMMatchResultEntry& A, const FDMMatchResultEntry& B)
    {
        const bool bAIsWinner = A.BaseSlot == WinningBaseSlot;
        const bool bBIsWinner = B.BaseSlot == WinningBaseSlot;

        // 전멸 승리는 등록 유물 수와 관계없이 승자가 1위다.
        if (bPinWinnerFirst && bAIsWinner != bBIsWinner)
        {
            return bAIsWinner;
        }

        if (A.RelicCount != B.RelicCount)
        {
            return A.RelicCount > B.RelicCount;
        }

        if (bAIsWinner != bBIsWinner)
        {
            return bAIsWinner;
        }

        return A.BaseSlot < B.BaseSlot;
    });

// 고정된 승자가 실제로 맨 앞에 있을 때만 단독 1위로 처리한다.
const bool bHasPinnedWinner = bPinWinnerFirst && Results.Num() > 0 && Results[0].BaseSlot == WinningBaseSlot;

for (int32 Index = 0; Index < Results.Num(); ++Index)
{
    // 고정된 승자와는 등록 수가 같아도 공동 순위로 묶지 않는다.
    const bool bPreviousIsPinnedWinner = bHasPinnedWinner && Index == 1;
    const bool bIsTied = Index > 0 && !bPreviousIsPinnedWinner
        && Results[Index].RelicCount == Results[Index - 1].RelicCount;

    Results[Index].Rank = bIsTied ? Results[Index - 1].Rank : Index + 1;
}
```

호출 쪽은 종료 사유를 받아 결정한다.

```cpp
// BuildFinalPlayerResults(EndReason, WinningBaseSlot)
const bool bPinWinnerFirst = EndReason == EDMMatchEndReason::EliminationVictory;
DMMatchResultRules::SortAndRankResults(Results, WinningBaseSlot, bPinWinnerFirst);
```

**규칙을 순수 함수에 둔 이유:** 판정 규칙이 월드 없이 계산되는 `DMMatchResultRules`에 있어서 Automation Test로 바로 검증할 수 있다. 기존 `DontMelt.Match.ResultRules.Ranking` 테스트에 경우를 추가했다.

```cpp
// 등록 수 4,2,2,1, 승자(BaseSlot 1)의 등록 수 1 → 승자 1위, 나머지 2,3,3
DMMatchResultRules::SortAndRankResults(EliminationResults, 1, true);
TestEqual(TEXT("Elimination winner first"), EliminationResults[0].BaseSlot, 1);
TestEqual(TEXT("Elimination rank 2"), EliminationResults[1].Rank, 2);
TestEqual(TEXT("Elimination rank 3 tied"), EliminationResults[3].Rank, 3);

// 승자와 등록 수가 같아도 공동 1위로 묶지 않는다.
TestEqual(TEXT("Same count other rank 2"), SameCountResults[1].Rank, 2);
```

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| Session Frontend에서 `DontMelt.Match.ResultRules` 실행 | RelicVictory · EliminationVictory · OvertimeWinner · Ranking 4개 통과 |
| 3인 경기 전멸 승리 | 결과표 1위 = 배너 승자 |
| 유물·정규·연장 승리 | 기존 순위와 같음 |

## 정리

- 규칙 충돌은 코드에서 임의로 정하지 않고, **선택지로 정리해 결정을 받은 뒤** 반영한다.
- 매개변수 기본값으로 **기존 호출과 테스트를 깨지 않고** 규칙을 확장했다.
- 게임 규칙을 월드와 분리된 순수 함수로 두면 엔진을 띄우지 않고 테스트할 수 있다.

---

[Debugging 목차](../README.md)
