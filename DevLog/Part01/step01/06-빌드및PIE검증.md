# 6일차 — C++ 골격 빌드 및 Multiplayer PIE 검증

## 오늘 한 일

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

## 복습 개념

- PIE
- Multiplayer PIE
- Listen Server
- Development Editor Build
- Actor Spawn
- Possession
- Runtime Validation

## 완료 기준

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
