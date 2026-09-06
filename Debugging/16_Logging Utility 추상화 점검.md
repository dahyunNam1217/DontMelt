# Logging Utility 과도한 추상화 여부 점검

> **기록 유형:** 디버깅 도구 설계 판단

## 1. 점검 배경

프로젝트에서 `UE_LOG` 호출이 늘어나면서 별도 Custom Log Header나 Wrapper Macro를 만들지 검토했다.

예:

```text
UE_LOG 반복
        ↓
공통 Logging Utility 필요?
```

## 2. Utility를 추가할 때 생기는 비용

단순 Wrapper라도 다음이 추가된다.

```text
새 Header Dependency
새 Macro/API 학습 비용
로그를 따라갈 때 Wrapper 구현 확인
Engine 기본 Log Category와 중복되는 추상화
```

따라서 "`로그 코드가 반복된다`"는 이유만으로 공통 Wrapper를 만들지 않는다.

## 3. 현재 판단

현재 프로젝트에서는 기본:

```cpp
UE_LOG(...)
```

와 필요 시 별도 Log Category를 사용하는 방식으로도 충분히 추적 가능하다고 판단해 **Generic Logging Wrapper 도입을 보류**했다.

이 기록은 Wrapper를 구현했다가 성능을 개선한 사례가 아니라 **필요 없는 추상화를 추가하지 않은 설계 판단**이다.

## 4. 언제 Utility가 의미가 생기는가

다음 요구가 실제로 반복된다면 다시 검토할 수 있다.

```text
모든 Network Log에 NetMode/Role 자동 포함
모든 Log에 Object/World Context 자동 포함
파일/Telemetry 전송
Build Configuration별 Logging Policy
공통 Structured Log Format
```

이때는 단순 `UE_LOG` 단축 Macro가 아니라 **프로젝트 공통 Debug Context를 일관되게 제공하는 기능**이 된다.

## 5. 디버깅 Utility 평가 기준

```text
1. 실제 반복되는 Debug 정보가 있는가?
2. Wrapper가 원본 Log를 찾기 어렵게 만들지는 않는가?
3. 모든 Call Site가 같은 정책을 요구하는가?
4. Engine 기본 기능으로 해결 가능한가?
5. Utility가 Debugging 시간을 실제로 줄이는가?
```

## 6. 기록 결론

디버깅 도구 역시 "`코드 중복 제거`"보다 **문제 추적을 실제로 더 쉽게 만드는가**를 기준으로 도입해야 한다고 판단했다.

현재 단계에서는 Generic Log Wrapper를 추가하지 않고 기본 Unreal Logging 구조를 유지한다.

## 관련 개념

`UE_LOG` `Log Category` `DRY` `Premature Abstraction` `Debugging Utility`
