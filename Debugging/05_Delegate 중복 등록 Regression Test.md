# Delegate 중복 등록 Regression Test

> **기록 유형:** 회귀 테스트 설계 / 중복 호출 예방  
> **주의:** 현재 보존된 기록에는 실제 중복 Callback 발생 수치가 없다.

## 1. 점검 목적

Listen Server 종료 후 Client 복귀 로직이 재접속을 반복할수록 여러 번 호출되는 문제가 없는지 확인하기 위한 Regression Test를 정의했다.

의도한 불변식:

```text
Host 종료 1회
→ Disconnect 처리 Callback 1회
```

## 2. 재현 절차

```text
Client가 Host에 접속
        ↓
Match 시작
        ↓
Host 종료
        ↓
Client Lobby 복귀
        ↓
다시 Host에 접속
        ↓
Match 시작
        ↓
Host 다시 종료
```

각 Host 종료마다 다음 로그가 정확히 한 번 발생하는지 확인한다.

```text
Connection to the Listen Server ended.
Returning the Client to the default map.
```

## 3. 중복이 보일 때의 원인 후보

가장 먼저 Delegate 중복 Binding을 확인하지만, 그것만으로 확정하지 않는다.

```text
Callback 2회
        ↓
Broadcast 자체가 2회인가?
        ↓
Binding이 2개인가?
        ↓
Subscriber Instance가 여러 개인가?
```

따라서 Debug Log도 다음 세 층으로 분리하는 편이 좋다.

```text
Bind
Broadcast
Callback
```

## 4. Delegate 중복 Binding 후보

예:

```cpp
SomeDelegate.AddUObject(
    this,
    &ThisClass::HandleDisconnect
);
```

이 Binding 경로가 재접속 때마다 실행되지만 이전 Subscription Lifetime이 끝나지 않았다면 Invocation List가 의도보다 늘어날 수 있다.

## 5. 정리 방법

Native Multicast Delegate라면 상황에 따라:

```cpp
Remove(DelegateHandle);
RemoveAll(this);
```

등을 사용할 수 있다.

Dynamic Delegate라면:

```cpp
RemoveDynamic(...);
```

같은 대응 API를 사용한다.

핵심은 API보다:

```text
Bind
↓
Subscription이 유효한 기간
↓
Unbind
```

를 명확하게 맞추는 것이다.

## 6. 아직 확인되지 않은 부분

현재 기록에는:

```text
1회 재접속 → Callback 1
2회 재접속 → Callback 2
...
```

같은 실제 측정표가 없다.

따라서 이 문서는 "`Delegate 중복 버그를 해결했다`"는 사례가 아니라 **재접속 시 Subscription 누적을 검증하기 위한 Regression Test 기록**으로 남긴다.

## 7. 향후 측정표

| 재접속 회차 | Broadcast 수 | Callback 수 | 판정 |
|---:|---:|---:|---|
| 1 | 미측정 | 미측정 | - |
| 2 | 미측정 | 미측정 | - |
| 3 | 미측정 | 미측정 | - |
| 4 | 미측정 | 미측정 | - |
| 5 | 미측정 | 미측정 | - |

## 기록 결론

한 번 발생해야 하는 Connection Event가 재접속 이후 여러 번 처리되는 문제를 예방하기 위해 **재접속 기반 Delegate Regression Scenario**를 만들었다.

## 관련 개념

`Delegate` `FDelegateHandle` `Reconnect` `Regression Test` `Subscription Lifetime`
