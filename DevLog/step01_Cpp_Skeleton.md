# 1단계 — C++ 프로젝트 골격

## 05. C++ 프로젝트 골격 구현

확정된 Architecture를 기반으로 실제 Unreal C++ 클래스와 Component의 기본 골격을 구현했다. 이 단계에서는 기능 자체를 넣기보다 클래스 관계와 생성 구조가 정상적으로 동작하는지 확인하는 것을 목표로 했다.

구현 범위:

- Lobby Framework
- Match Framework
- Player 계층
- NPC 계층
- Character 공통 부모 구조
- 필요한 Gameplay Component
- Player / NPC 스폰을 위한 기본 클래스 구성

**최적화**

기능이 없는 Component가 불필요하게 Tick하지 않도록 처리했다.

```cpp
PrimaryComponentTick.bCanEverTick = false;
```

필요하지 않은 경우 `BeginPlay()`와 `TickComponent()`도 제거했다.

**결과**

기능 구현 전에 클래스 생성 및 Component 부착 구조를 먼저 안정화했다.

## 06. C++ 골격 빌드 및 Multiplayer PIE 검증

1단계에서 생성한 프로젝트 골격이 실제 Unreal 환경에서 정상적으로 동작하는지 검증했다.

검증 항목:

- Development Editor Build 성공
- Unreal Editor 실행
- Lobby Test Map 진입
- Match Test Map 진입
- Listen Server 실행
- 여러 PIE Client 접속
- Player Character 생성
- NPC 생성
- Component 정상 부착
- 접속 과정에서 오류 발생 여부 확인

**완료 기준**

```text
C++ Compile 성공
    ↓
Lobby / Match Framework 생성
    ↓
Player / NPC 생성
    ↓
Component 부착
    ↓
Multiplayer PIE 정상 실행
```

이 검증을 통과한 후에만 Match 시스템 구현으로 넘어가기로 했다.
