# 정규 시간 종료 시 단독 1위 승리 판정이 빠져 있던 문제

| 항목 | 내용 |
|---|---|
| 문제 | 정규 15분이 끝나면 등록 수 단독 1위가 있어도 항상 연장전으로 넘어갔다 |
| 원인 | 정규 시간 종료 처리가 "경기 중 언제든 적용되는 승리 조건"만 검사하고, 시간 종료 순간의 상대 순위는 보지 않았다 |
| 해결 | 연장전 종료에 쓰던 단독 1위 판정 함수를 재사용해, 연장전 진입 전에 정규 시간 승리 분기를 넣었다 |
| 관련 코드 | `ADMMatchGameMode::HandleRegularTimeExpired`, `TryResolveOvertimeWinner` |

## 현상

기획서에는 "정규 시간이 끝났을 때 등록 유물 수 단독 1위면 즉시 승리, 최다 동률이면 연장전"이라고 되어 있다. 코드는 유물 목표 수 달성·전멸 여부만 검사한 뒤, 결과가 없으면 무조건 연장전으로 갔다.

## 원인 추적

```text
HandleRegularTimeExpired()
 → EvaluateMatchResult() : 유물 목표 달성 / 전멸
 → 미종료라면 TryStartOvertimeIntro() : 무조건 연장전
```

코드만 보면 "없는 분기"라 결함으로 보이지 않는다. 처음에는 의도인지 불분명했다. 기획서 5.3·5.4절과 2단계 설계서의 `RegularTimeVictory` 허용 Phase를 코드와 대조하고 나서 규칙 불일치로 분류했다. **설계 문서와 코드를 대조하는 검수**에서만 찾을 수 있는 종류의 문제였다.

## 해결

연장전 종료에 쓰던 `TryResolveOvertimeWinner()`가 이미 "등록 수 단독 1위"를 구한다. 새 판정을 만들지 않고 이 함수를 재사용했다.

```cpp
int32 WinnerBaseSlot = INDEX_NONE;
if (!TryResolveOvertimeWinner(WinnerBaseSlot))
{
    AbortMatchForInternalFailure(TEXT("정규 시간 종료: 등록 유물 수 판정에 실패했습니다."));
    return;
}
if (WinnerBaseSlot != INDEX_NONE)
{
    if (TryFinishMatch(EDMMatchEndReason::RegularTimeVictory, WinnerBaseSlot))
    {
        return;
    }
    // 같은 프레임에 다른 승리 경로가 먼저 확정했다면 그 결과를 유지한다.
    if (MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None)
    {
        return;
    }
    AbortMatchForInternalFailure(TEXT("정규 시간 승리 적용 실패."));
    return;
}
// 최다 동률이면 기존 연장전 인트로 진행
```

결과 UI에는 이미 종료 사유별 분기가 있어 추가 변경이 필요 없었다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| 정규 종료 시 등록 수 3, 1, 0 | 즉시 `RegularTimeVictory` |
| 정규 종료 시 2, 2, 1 | 연장전 진입 |
| 전원 0개 | 연장전 진입 |
| 시간 종료와 목표 유물 달성이 같은 프레임 | 먼저 확정된 결과 하나만 적용 |

## 정리

- 코드에서 "없는 기능"은 결함으로 보이지 않는다. 설계 문서와 대조해야 보인다.
- 같은 판정이 이미 있으면 재사용한다. 규칙이 두 곳에 생기면 나중에 어긋난다.

---

[Debugging 목차](../README.md)
