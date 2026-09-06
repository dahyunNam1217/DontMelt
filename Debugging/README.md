# Don't Melt — Debugging \& Verification

Don't Melt에서 발생한 오류와 구조 검증 항목을 정리한 기록입니다.

이 폴더의 목적은 오류 메시지를 모으는 것이 아니라 **재현 → 관찰 → 원인 후보 → 검증 → 해결/판정 → 회귀 확인**의 과정을 남기는 것입니다.

## Record Policy

* 실제 발생한 문제와 예방적 점검을 구분합니다.
* 발생하지 않은 오류를 경험한 것처럼 작성하지 않습니다.
* 측정하지 않은 성능 수치를 만들지 않습니다.
* 네트워크 문제는 우선 `Owner / Authority / Lifetime / Replication Scope`를 확인합니다.
* 수정 후 동일 조건에서 다시 재현하여 Regression 여부를 확인합니다.

\---

## 1\. Replication / Match Flow

* `01 Replication 등록 누락 가능성 점검`
* `02 Match Phase Client Replication 점검`
* `03 Match Flow Console Command 점검`
* `04 Listen Server 종료 후 Client 복귀`
* `05 Delegate 중복 등록 Regression Test`

## 2\. Authority / State Ownership

* `06 Server Authority 위반 가능성 점검`
* `07 GameMode / GameState 상태 중복 점검`
* `08 PlayerState / Character 데이터 중복 점검`
* `09 Component 상태 중복 소유 점검`
* `10 Enum / GameplayTag 상태 중복 점검`
* `11 Movement State / Action State 충돌 점검`
* `12 UI / Gameplay State 불일치 점검`

## 3\. Structure / Performance Review

* `13 불필요한 Tick 점검`
* `14 반복 탐색 성능 점검`
* `15 불필요한 Replication 비용 점검`
* `16 Logging Utility 추상화 검토`

> 개별 파일명은 저장소의 실제 문서명을 기준으로 유지합니다. 파일명을 변경하지 않고 이 README를 목차로 사용합니다.

\---

## Debugging Checklist

문제를 만났을 때 다음 순서로 확인합니다.

```text
1. 재현 조건을 고정한다.
2. 실제 로그 / 증상을 기록한다.
3. 정상 동작과 현재 동작의 차이를 정의한다.
4. State Owner와 Authority를 확인한다.
5. Lifetime과 Replication Scope를 확인한다.
6. 원인 후보를 하나씩 제거한다.
7. 수정 후 같은 조건으로 재검증한다.
8. 재발 가능성이 있으면 Regression 항목으로 남긴다.
```

