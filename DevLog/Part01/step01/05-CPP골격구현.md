# 5일차 — C++ 프로젝트 골격 구현

## 오늘 한 일

확정된 Architecture를 기반으로 실제 Unreal C++ 클래스와 Component의 기본 골격을 구현했다. 이 단계에서는 기능 자체를 넣기보다 클래스 관계와 생성 구조가 정상적으로 동작하는지 확인하는 것을 목표로 했다.

구현 범위:

- Lobby Framework
- Match Framework
- Player 계층
- NPC 계층
- Character 공통 부모 구조
- 필요한 Gameplay Component
- Player / NPC 스폰을 위한 기본 클래스 구성

## 복습 개념

- Unreal C++ Class Architecture
- Actor
- Character
- ActorComponent
- `CreateDefaultSubobject`
- Constructor
- Inheritance
- Composition

## 최적화

기능이 없는 Component가 불필요하게 Tick하지 않도록 처리했다.

```cpp
PrimaryComponentTick.bCanEverTick = false;
```

필요하지 않은 경우 `BeginPlay()`와 `TickComponent()`도 제거했다.

## 오늘의 결과

기능 구현 전에 클래스 생성 및 Component 부착 구조를 먼저 안정화했다.
